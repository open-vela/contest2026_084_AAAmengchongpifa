from __future__ import annotations

import io

from fastapi.testclient import TestClient
from PIL import Image

from app.config import Settings
from app.main import create_app
from app.providers.mimo import ProviderError


def make_png(size: tuple[int, int] = (64, 48)) -> bytes:
    output = io.BytesIO()
    Image.new("RGB", size, (116, 210, 190)).save(output, "PNG")
    return output.getvalue()


def chat_body(device_id: str = "bandx-demo") -> dict:
    return {
        "device_id": device_id,
        "prompt_type": "task",
        "locale": "zh-CN",
        "max_chars": 50,
        "message": "",
        "context": {
            "pet_name": "Vela",
            "level": 2,
            "exp": 80,
            "exp_to_next": 250,
            "title": "egg_baby",
            "current_skin": "sport_blue",
            "unlocked_skins": ["default", "sport_blue"],
            "total_steps": 2500,
            "consecutive_days": 2,
            "daily_progress": 76,
            "tasks": [
                {
                    "description": "Walk 3000 steps today",
                    "target": 3000,
                    "progress": 2500,
                    "status": 1,
                }
            ],
        },
    }


def test_health_and_local_chat(tmp_path):
    settings = Settings(data_root=tmp_path)
    with TestClient(create_app(settings)) as client:
        health = client.get("/health")
        assert health.status_code == 200
        assert health.json()["chat_provider"] == "local_fallback"

        response = client.post("/api/v1/chat", json=chat_body())
        assert response.status_code == 200
        data = response.json()
        assert data["source"] == "local_fallback"
        assert data["error_code"] == "MIMO_NOT_CONFIGURED"
        assert data["intent"] == "reminder"
        assert "500" in data["reply"]
        assert len(data["reply"]) <= 50


class FailingMiMo:
    enabled = True

    async def generate_reply(self, request):
        del request
        raise ProviderError("MIMO_UNAVAILABLE", "network down", retryable=True)

    async def describe_reference_image(self, source_path, mime_type, style):
        del source_path, mime_type, style
        raise ProviderError("MIMO_UNAVAILABLE", "network down", retryable=True)


def test_mimo_error_degrades_without_breaking_device_contract(tmp_path):
    settings = Settings(data_root=tmp_path, mimo_api_key="fake")
    with TestClient(create_app(settings, mimo_client=FailingMiMo())) as client:
        response = client.post("/api/v1/chat", json=chat_body())
        assert response.status_code == 200
        data = response.json()
        assert data["source"] == "local_fallback"
        assert data["error_code"] == "MIMO_UNAVAILABLE"
        assert data["reply"]


def test_device_auth(tmp_path):
    settings = Settings(data_root=tmp_path, device_tokens=frozenset({"secret"}))
    with TestClient(create_app(settings)) as client:
        assert client.post("/api/v1/chat", json=chat_body()).status_code == 401
        assert (
            client.post(
                "/api/v1/chat",
                json=chat_body(),
                headers={"X-VelaPet-Token": "wrong"},
            ).status_code
            == 403
        )
        assert (
            client.post(
                "/api/v1/chat",
                json=chat_body(),
                headers={"X-VelaPet-Token": "secret"},
            ).status_code
            == 200
        )


def test_chat_rate_limit(tmp_path):
    settings = Settings(data_root=tmp_path, chat_rate_limit_per_minute=1)
    with TestClient(create_app(settings)) as client:
        assert client.post("/api/v1/chat", json=chat_body()).status_code == 200
        response = client.post("/api/v1/chat", json=chat_body())
        assert response.status_code == 429
        assert response.json()["detail"]["error_code"] == "RATE_LIMITED"


def test_demo_asset_pipeline_and_download(tmp_path):
    settings = Settings(data_root=tmp_path, public_base_url="http://testserver")
    with TestClient(create_app(settings)) as client:
        response = client.post(
            "/api/v1/pet-assets/jobs",
            data={
                "device_id": "bandx-demo",
                "style": "soft cartoon",
                "asset_kinds": "avatar,home,skin",
            },
            files={"file": ("pet.png", make_png(), "image/png")},
        )
        assert response.status_code == 202
        job_id = response.json()["job_id"]

        job = client.get(f"/api/v1/pet-assets/jobs/{job_id}")
        assert job.status_code == 200
        data = job.json()
        assert data["status"] == "succeeded"
        assert len(data["artifacts"]) == 3

        home = next(item for item in data["artifacts"] if item["kind"] == "home")
        assert home["width"] == 240
        assert home["height"] == 240
        assert home["device_path"] == "/data/agent/assets/pet/home_pet.png"
        downloaded = client.get(home["download_url"].removeprefix("http://testserver"))
        assert downloaded.status_code == 200
        assert downloaded.headers["content-type"].startswith("image/png")
        with Image.open(io.BytesIO(downloaded.content)) as image:
            assert image.size == (240, 240)


def test_asset_rejects_non_image(tmp_path):
    settings = Settings(data_root=tmp_path)
    with TestClient(create_app(settings)) as client:
        response = client.post(
            "/api/v1/pet-assets/jobs",
            data={"device_id": "bandx-demo"},
            files={"file": ("bad.png", b"not an image", "image/png")},
        )
        assert response.status_code == 422
        assert response.json()["error_code"] == "INVALID_IMAGE"


def test_asset_kind_validation(tmp_path):
    settings = Settings(data_root=tmp_path)
    with TestClient(create_app(settings)) as client:
        response = client.post(
            "/api/v1/pet-assets/jobs",
            data={"device_id": "bandx-demo", "asset_kinds": "wallpaper"},
            files={"file": ("pet.png", make_png(), "image/png")},
        )
        assert response.status_code == 422
        assert response.json()["detail"]["error_code"] == "INVALID_ASSET_KIND"
