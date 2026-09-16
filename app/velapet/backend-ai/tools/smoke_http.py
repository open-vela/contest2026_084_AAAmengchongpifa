"""Start a temporary local backend, exercise real HTTP, then stop it."""
from pathlib import Path
import io
import os
import socket
import subprocess
import sys
import tempfile
import time

import httpx
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from client_demo import download_checked


def main():
    with tempfile.TemporaryDirectory(prefix="velapet-smoke-") as temp:
        with socket.socket() as sock:
            sock.bind(("127.0.0.1", 0))
            port = sock.getsockname()[1]
        base = f"http://127.0.0.1:{port}"
        env = dict(os.environ, VELAPET_DATA_ROOT=temp, MIMO_API_KEY="",
                   VELAPET_IMAGE_PROVIDER="demo", VELAPET_PUBLIC_BASE_URL="",
                   VELAPET_DEVICE_TOKENS="smoke-test-token")
        flags = subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0
        process = subprocess.Popen([sys.executable, "-m", "uvicorn", "app.main:app",
                                    "--host", "127.0.0.1", "--port", str(port)],
                                   cwd=ROOT, env=env, stdout=subprocess.DEVNULL,
                                   stderr=subprocess.DEVNULL, creationflags=flags)
        try:
            with httpx.Client(timeout=5, headers={"X-VelaPet-Token": "smoke-test-token"}) as http:
                deadline = time.monotonic() + 15
                while True:
                    try:
                        http.get(base + "/health").raise_for_status()
                        break
                    except httpx.HTTPError:
                        if process.poll() is not None or time.monotonic() > deadline:
                            raise RuntimeError("Temporary server did not start")
                        time.sleep(.1)
                response = http.post(base + "/api/v1/chat", json={
                    "device_id": "smoke", "prompt_type": "cheer", "context": {}})
                response.raise_for_status()
                assert response.json()["source"] == "local_fallback"
                data = io.BytesIO()
                Image.new("RGB", (40, 50), (100, 200, 180)).save(data, "PNG")
                response = http.post(base + "/api/v1/pet-assets/jobs",
                    data={"device_id": "smoke"}, files={"file": ("test.png", data.getvalue(), "image/png")})
                response.raise_for_status()
                job = response.json()
                deadline = time.monotonic() + 15
                while job["status"] in ("queued", "processing"):
                    if time.monotonic() > deadline: raise TimeoutError("Image job timed out")
                    time.sleep(.1)
                    job = http.get(base + "/api/v1/pet-assets/jobs/" + job["job_id"]).json()
                assert job["status"] == "succeeded", job
                for item in job["artifacts"]:
                    path = download_checked(http, base, item, Path(temp) / "client")
                    with Image.open(path) as image:
                        assert image.size == (item["width"], item["height"])
                assert len(job["artifacts"]) == 3
                print("HTTP smoke passed: auth, chat, upload, poll, three verified downloads")
        finally:
            process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()


if __name__ == "__main__":
    main()
