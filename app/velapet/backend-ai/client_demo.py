"""PC integration client: python client_demo.py --image pet.png --cache ./cache."""
from __future__ import annotations

import argparse
import hashlib
import json
import mimetypes
import os
from pathlib import Path
import time
from urllib.parse import urljoin, urlsplit

from dotenv import load_dotenv
import httpx


def download_checked(client: httpx.Client, base: str, asset: dict, cache: Path) -> Path:
    names = {"avatar": "avatar.png", "home": "home_pet.png", "skin": "skin_custom.png"}
    filename = names[asset["kind"]]
    url = urljoin(base.rstrip("/") + "/", asset["download_url"])
    if urlsplit(url).netloc != urlsplit(base).netloc or urlsplit(url).scheme != urlsplit(base).scheme:
        raise ValueError("Asset URL must use the configured backend origin")
    cache.mkdir(parents=True, exist_ok=True)
    target = cache / filename
    if target.is_file() and hashlib.sha256(target.read_bytes()).hexdigest() == asset["sha256"]:
        return target
    if not 0 < asset["byte_size"] <= 1024 * 1024:
        raise ValueError("Invalid asset size")
    temporary = target.with_suffix(".png.part")
    digest = hashlib.sha256()
    count = 0
    try:
        with client.stream("GET", url) as response:
            response.raise_for_status()
            with temporary.open("wb") as output:
                for chunk in response.iter_bytes():
                    count += len(chunk)
                    if count > asset["byte_size"]:
                        raise ValueError("Asset exceeds declared size")
                    digest.update(chunk)
                    output.write(chunk)
                output.flush()
                os.fsync(output.fileno())
        if count != asset["byte_size"] or digest.hexdigest() != asset["sha256"]:
            raise ValueError("Asset checksum mismatch; existing cache preserved")
        os.replace(temporary, target)
    finally:
        temporary.unlink(missing_ok=True)
    return target


def main():
    load_dotenv()
    parser = argparse.ArgumentParser()
    parser.add_argument("--base", default="http://127.0.0.1:8000")
    parser.add_argument("--device", default="pc-demo")
    parser.add_argument("--image", type=Path)
    parser.add_argument("--cache", type=Path, default=Path("runtime-data/client-cache"))
    parser.add_argument("--context", type=Path, help="JSON exported by vela_pet_get_ai_context")
    args = parser.parse_args()
    context = json.loads(args.context.read_text(encoding="utf-8")) if args.context else {
        "pet_name": "Vela", "level": 1, "exp": 60, "exp_to_next": 100,
        "tasks": [{"description": "Walk 3000 steps today", "target": 3000, "progress": 2400, "status": 1}],
    }
    token = os.getenv("VELAPET_CLIENT_TOKEN", "")
    headers = {"X-VelaPet-Token": token} if token else {}
    with httpx.Client(headers=headers, timeout=40, follow_redirects=False) as client:
        reply = client.post(args.base.rstrip("/") + "/api/v1/chat", json={
            "device_id": args.device, "prompt_type": "cheer", "context": context,
        })
        reply.raise_for_status()
        print(json.dumps(reply.json(), ensure_ascii=False, indent=2))
        if not args.image:
            return
        with args.image.open("rb") as source:
            response = client.post(args.base.rstrip("/") + "/api/v1/pet-assets/jobs",
                data={"device_id": args.device},
                files={"file": (args.image.name, source, mimetypes.guess_type(args.image.name)[0] or "image/png")})
        response.raise_for_status()
        job = response.json()
        deadline = time.monotonic() + 180
        while job["status"] in ("queued", "processing"):
            if time.monotonic() >= deadline:
                raise TimeoutError(f"Image job timed out: {job['job_id']}")
            time.sleep(2)
            response = client.get(f"{args.base.rstrip('/')}/api/v1/pet-assets/jobs/{job['job_id']}")
            response.raise_for_status()
            job = response.json()
        if job["status"] != "succeeded":
            raise RuntimeError(f"Image failed: {job['error_code']}")
        for asset in job["artifacts"]:
            print(download_checked(client, args.base, asset, args.cache))
        (args.cache / "manifest.json").write_text(json.dumps(job, ensure_ascii=False, indent=2), encoding="utf-8")


if __name__ == "__main__":
    main()
