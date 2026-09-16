"""Perform one minimal live request against each configured AI provider.

The script never prints credentials or upstream response bodies. The generated
image is saved locally so its format can be inspected after the check.
"""
from __future__ import annotations

import asyncio
import io
import sys
from pathlib import Path

from dotenv import load_dotenv
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from app.config import Settings
from app.models import ChatRequest
from app.providers.image import VolcengineArkImageProvider
from app.providers.mimo import MiMoClient, ProviderError
from app.providers.vision import VolcengineArkVisionClient


async def main() -> int:
    load_dotenv(ROOT / ".env")
    settings = Settings.from_env()
    failures = 0

    try:
        reply = await MiMoClient(settings).generate_reply(
            ChatRequest(
                device_id="provider-check",
                prompt_type="greeting",
                message="请用一句简短的话向 VelaPet 用户问好。",
                max_chars=50,
                context={},
            )
        )
        print(f"MiMo: OK (intent={reply.intent.value}, chars={len(reply.reply)})")
    except ProviderError as exc:
        failures += 1
        print(f"MiMo: FAILED ({exc.code})")

    source_dir = settings.data_root.resolve() / "provider-check"
    source_dir.mkdir(parents=True, exist_ok=True)
    source_path = source_dir / "reference.png"
    canvas = Image.new("RGB", (256, 256), "#fff7e8")
    draw = ImageDraw.Draw(canvas)
    draw.ellipse((54, 58, 202, 210), fill="#ef8d32")
    draw.polygon(((78, 86), (90, 38), (120, 82)), fill="#ef8d32")
    draw.polygon(((136, 82), (166, 38), (178, 86)), fill="#ef8d32")
    draw.ellipse((95, 118, 108, 131), fill="#28231f")
    draw.ellipse((148, 118, 161, 131), fill="#28231f")
    canvas.save(source_path, "PNG")

    if settings.ark_vision_enabled:
        try:
            prompt = await VolcengineArkVisionClient(settings).describe_reference_image(
                source_path, "image/png", "soft 3D cartoon"
            )
            print(f"Volcengine Ark vision: OK (prompt_chars={len(prompt)})")
        except ProviderError as exc:
            failures += 1
            print(f"Volcengine Ark vision: FAILED ({exc.code})")

    try:
        generated = await VolcengineArkImageProvider(settings).generate(
            source_path,
            "image/png",
            "Transform the reference into a friendly VelaPet smartwatch fox mascot, "
            "soft 3D cartoon style, centered, clean pale background, no text, no watermark",
        )
        output_path = source_dir / "ark-output.png"
        output_path.write_bytes(generated)
        with Image.open(io.BytesIO(generated)) as result:
            result.verify()
            size = result.size
            fmt = result.format
        print(f"Volcengine Ark: OK (format={fmt}, size={size[0]}x{size[1]})")
        print(f"Saved: {output_path}")
    except (ProviderError, OSError, ValueError) as exc:
        failures += 1
        code = exc.code if isinstance(exc, ProviderError) else "INVALID_IMAGE"
        detail = str(exc) if isinstance(exc, ProviderError) else "invalid image"
        print(f"Volcengine Ark: FAILED ({code}: {detail})")

    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(asyncio.run(main()))
