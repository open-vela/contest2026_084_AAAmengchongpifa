import asyncio
import base64
import hashlib
import io
import json

import httpx
import pytest
from fastapi.testclient import TestClient
from PIL import Image

from app.config import Settings
from app.main import create_app
from app.models import ChatRequest, ReplyIntent
from app.providers.mimo import MiMoClient, ProviderError
from app.providers.image import VolcengineArkImageProvider
from app.providers.vision import VolcengineArkVisionClient
from app.services.chat import ChatService
from client_demo import download_checked


@pytest.mark.parametrize("raw", ['{}', '{"reply":null}', '{"reply":""}', '{"reply":123}', '{"reply":"ok","intent":"invalid"}'])
def test_invalid_structured_reply(raw):
    with pytest.raises(ProviderError):
        MiMoClient._parse_reply(raw, 50)


def test_mimo_http_contract_and_reward_gate():
    def handler(request):
        body = json.loads(request.content)
        assert request.url.path == "/v1/chat/completions"
        assert request.headers["Authorization"] == "Bearer test-only"
        assert body["thinking"] == {"type": "disabled"}
        return httpx.Response(200, json={"choices": [{"message": {"content": json.dumps({
            "reply": "休息一下也很好。", "intent": "chat", "emotion": "gentle"})}}]})
    async def run():
        async with httpx.AsyncClient(transport=httpx.MockTransport(handler)) as http:
            service = ChatService(MiMoClient(Settings(mimo_api_key="test-only"), http))
            response = await service.reply(ChatRequest(device_id="test", prompt_type="cheer", context={}))
            assert response.source == "mimo"
            assert not response.reward.eligible
    asyncio.run(run())


def test_image_failure_is_terminal_and_persisted(tmp_path):
    class FailedProvider:
        name = "test"
        async def generate(self, *args):
            raise ProviderError("IMAGE_PROVIDER_TIMEOUT", "Timed out")
    image = io.BytesIO()
    Image.new("RGB", (20, 20)).save(image, "PNG")
    settings = Settings(data_root=tmp_path)
    with TestClient(create_app(settings, image_provider=FailedProvider())) as client:
        response = client.post("/api/v1/pet-assets/jobs", data={"device_id": "test"},
            files={"file": ("test.png", image.getvalue(), "image/png")})
        job_id = response.json()["job_id"]
        job = client.get(f"/api/v1/pet-assets/jobs/{job_id}").json()
        assert job["status"] == "failed"
        assert job["error_code"] == "IMAGE_PROVIDER_TIMEOUT"
    with TestClient(create_app(settings)) as restarted:
        assert restarted.get(f"/api/v1/pet-assets/jobs/{job_id}").json()["status"] == "failed"


def test_bad_download_preserves_existing_cache(tmp_path):
    old = tmp_path / "home_pet.png"
    old.write_bytes(b"previous verified picture")
    expected = b"correct image"
    asset = {"kind": "home", "byte_size": len(expected), "sha256": hashlib.sha256(expected).hexdigest(),
             "download_url": "/api/v1/pet-assets/test/files/home.png"}
    with httpx.Client(transport=httpx.MockTransport(lambda r: httpx.Response(200, content=b"wrong"))) as client:
        with pytest.raises(ValueError, match="checksum"):
            download_checked(client, "http://testserver", asset, tmp_path)
    assert old.read_bytes() == b"previous verified picture"
    assert not old.with_suffix(".png.part").exists()


def test_download_rejects_different_origin(tmp_path):
    asset = {"kind": "home", "download_url": "https://untrusted.invalid/a.png"}
    with httpx.Client() as client:
        with pytest.raises(ValueError, match="origin"):
            download_checked(client, "http://testserver", asset, tmp_path)


def test_volcengine_ark_image_contract(tmp_path):
    source = tmp_path / "pet.png"
    image = io.BytesIO()
    Image.new("RGB", (8, 8), "orange").save(image, "PNG")
    source.write_bytes(image.getvalue())

    def handler(request):
        body = json.loads(request.content)
        assert request.url.path == "/api/v3/images/generations"
        assert request.headers["Authorization"] == "Bearer ark-test-only"
        assert body["model"] == "doubao-seedream-test"
        assert body["image"].startswith("data:image/png;base64,")
        assert body["size"] == "2K"
        assert body["response_format"] == "b64_json"
        assert body["stream"] is False
        assert body["watermark"] is False
        return httpx.Response(
            200,
            json={"data": [{"b64_json": base64.b64encode(image.getvalue()).decode()}]},
        )

    async def run():
        settings = Settings(
            image_provider="volcengine_ark",
            image_api_base_url="https://ark.cn-beijing.volces.com/api/v3",
            image_api_key="ark-test-only",
            image_model="doubao-seedream-test",
            image_response_format="b64_json",
        )
        async with httpx.AsyncClient(transport=httpx.MockTransport(handler)) as http:
            result = await VolcengineArkImageProvider(settings, http).generate(
                source, "image/png", "make a mascot"
            )
        assert result == image.getvalue()

    asyncio.run(run())


def test_volcengine_ark_rejects_url_only_response(tmp_path):
    source = tmp_path / "pet.png"
    source.write_bytes(b"test")

    async def run():
        settings = Settings(
            image_provider="volcengine_ark",
            image_api_base_url="https://ark.cn-beijing.volces.com/api/v3",
            image_api_key="ark-test-only",
            image_model="doubao-seedream-test",
        )
        transport = httpx.MockTransport(
            lambda request: httpx.Response(200, json={"data": [{"url": "https://example.test/a.png"}]})
        )
        async with httpx.AsyncClient(transport=transport) as http:
            with pytest.raises(ProviderError) as caught:
                await VolcengineArkImageProvider(settings, http).generate(
                    source, "image/png", "make a mascot"
                )
        assert caught.value.code == "IMAGE_URL_UNTRUSTED"

    asyncio.run(run())


def test_volcengine_ark_generation_downloads_allowlisted_url(tmp_path):
    source = tmp_path / "pet.png"
    image = io.BytesIO()
    Image.new("RGB", (8, 8), "orange").save(image, "PNG")
    source.write_bytes(image.getvalue())

    def handler(request):
        if request.url.path == "/api/v3/images/generations":
            body = json.loads(request.content)
            assert body["response_format"] == "url"
            assert "image" not in body
            return httpx.Response(
                200, json={"data": [{"url": "https://assets.volces.com/result.png"}]}
            )
        assert request.url.host == "assets.volces.com"
        return httpx.Response(200, content=image.getvalue(), headers={"content-type": "image/png"})

    async def run():
        settings = Settings(
            image_provider="volcengine_ark",
            image_api_base_url="https://ark.cn-beijing.volces.com/api/v3",
            image_api_key="ark-test-only",
            image_model="doubao-seedream-5-0-pro-260628",
            image_api_mode="generations",
            image_response_format="url",
        )
        async with httpx.AsyncClient(transport=httpx.MockTransport(handler)) as http:
            result = await VolcengineArkImageProvider(settings, http).generate(
                source, "image/png", "make a mascot"
            )
        assert result == image.getvalue()

    asyncio.run(run())


def test_volcengine_ark_vision_contract(tmp_path):
    source = tmp_path / "pet.png"
    source.write_bytes(b"reference")

    def handler(request):
        body = json.loads(request.content)
        assert request.url.path == "/api/v3/chat/completions"
        assert request.headers["Authorization"] == "Bearer ark-test-only"
        assert body["model"] == "doubao-seed-2-1-turbo-260628"
        content = body["messages"][0]["content"]
        assert content[0]["type"] == "image_url"
        assert content[0]["image_url"]["url"].startswith("data:image/png;base64,")
        assert content[1]["type"] == "text"
        return httpx.Response(
            200,
            json={"choices": [{"message": {"content": "An orange fox mascot"}}]},
        )

    async def run():
        settings = Settings(
            image_api_base_url="https://ark.cn-beijing.volces.com/api/v3",
            image_api_key="ark-test-only",
            ark_vision_enabled=True,
        )
        async with httpx.AsyncClient(transport=httpx.MockTransport(handler)) as http:
            prompt = await VolcengineArkVisionClient(settings, http).describe_reference_image(
                source, "image/png", "soft cartoon"
            )
        assert prompt == "An orange fox mascot"

    asyncio.run(run())
