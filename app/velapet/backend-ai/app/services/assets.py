from __future__ import annotations

import asyncio
import hashlib
import io
import json
import os
from pathlib import Path
from uuid import uuid4

from PIL import Image, ImageOps, UnidentifiedImageError

from ..config import Settings
from ..models import AssetKind, AssetRecord, ImageJob, JobStatus, utc_now
from ..providers.image import ImageProvider
from ..providers.mimo import MiMoClient, ProviderError
from ..providers.vision import ReferenceImageDescriber


ASSET_SIZES: dict[AssetKind, tuple[int, int]] = {
    AssetKind.AVATAR: (128, 128),
    AssetKind.HOME: (240, 240),
    AssetKind.SKIN: (160, 160),
}

DEVICE_FILENAMES: dict[AssetKind, str] = {
    AssetKind.AVATAR: "avatar.png",
    AssetKind.HOME: "home_pet.png",
    AssetKind.SKIN: "skin_custom.png",
}


class AssetValidationError(ValueError):
    def __init__(self, code: str, message: str):
        super().__init__(message)
        self.code = code


class AssetService:
    def __init__(
        self,
        settings: Settings,
        mimo: MiMoClient,
        image_provider: ImageProvider,
        image_describer: ReferenceImageDescriber | None = None,
    ):
        self.settings = settings
        self.mimo = mimo
        self.image_describer = image_describer or mimo
        self.image_provider = image_provider
        self._jobs: dict[str, ImageJob] = {}
        self._lock = asyncio.Lock()
        self.settings.jobs_root.mkdir(parents=True, exist_ok=True)
        self.settings.assets_root.mkdir(parents=True, exist_ok=True)
        self._load_jobs()

    def _load_jobs(self) -> None:
        for path in self.settings.jobs_root.glob("*.json"):
            try:
                job = ImageJob.model_validate_json(path.read_text(encoding="utf-8"))
                if job.status in (JobStatus.QUEUED, JobStatus.PROCESSING):
                    job.status = JobStatus.FAILED
                    job.error_code = "SERVER_RESTARTED"
                    job.error_message = "The server restarted before the job completed"
                    job.updated_at = utc_now()
                    self._persist(job)
                self._jobs[job.job_id] = job
            except (OSError, ValueError):
                continue

    def _persist(self, job: ImageJob) -> None:
        final_path = self.settings.jobs_root / f"{job.job_id}.json"
        temp_path = final_path.with_suffix(".json.tmp")
        temp_path.write_text(
            job.model_dump_json(indent=2), encoding="utf-8", newline="\n"
        )
        os.replace(temp_path, final_path)

    def _validate_upload(self, payload: bytes, declared_mime: str) -> tuple[str, str]:
        if not payload:
            raise AssetValidationError("EMPTY_UPLOAD", "Uploaded image is empty")
        if len(payload) > self.settings.max_upload_bytes:
            raise AssetValidationError("UPLOAD_TOO_LARGE", "Uploaded image is too large")
        try:
            with Image.open(io.BytesIO(payload)) as image:
                width, height = image.size
                if width * height > self.settings.max_image_pixels:
                    raise AssetValidationError("IMAGE_DIMENSIONS_REJECTED", "Image dimensions are unsafe")
                fmt = (image.format or "").upper()
                image.verify()
        except AssetValidationError:
            raise
        except Image.DecompressionBombError as exc:
            raise AssetValidationError("IMAGE_DIMENSIONS_REJECTED", "Image dimensions are unsafe") from exc
        except (UnidentifiedImageError, OSError, ValueError) as exc:
            raise AssetValidationError("INVALID_IMAGE", "Unsupported or damaged image") from exc
        if width <= 0 or height <= 0 or width * height > self.settings.max_image_pixels:
            raise AssetValidationError("IMAGE_DIMENSIONS_REJECTED", "Image dimensions are unsafe")
        mime_by_format = {"PNG": "image/png", "JPEG": "image/jpeg", "WEBP": "image/webp"}
        if fmt not in mime_by_format:
            raise AssetValidationError("UNSUPPORTED_IMAGE", "Use PNG, JPEG or WebP")
        mime_type = mime_by_format[fmt]
        if declared_mime and declared_mime not in mime_by_format.values():
            raise AssetValidationError("UNSUPPORTED_IMAGE", "Use PNG, JPEG or WebP")
        suffix = {"PNG": ".png", "JPEG": ".jpg", "WEBP": ".webp"}[fmt]
        return mime_type, suffix

    async def submit(
        self,
        *,
        device_id: str,
        payload: bytes,
        declared_mime: str,
        style: str,
        requested_kinds: list[AssetKind],
    ) -> ImageJob:
        mime_type, suffix = self._validate_upload(payload, declared_mime)
        job_id = uuid4().hex
        source_dir = self.settings.assets_root / job_id
        source_dir.mkdir(parents=True, exist_ok=False)
        source_path = source_dir / f"source{suffix}"
        source_path.write_bytes(payload)
        job = ImageJob(
            job_id=job_id,
            device_id=device_id,
            status=JobStatus.QUEUED,
            provider=self.image_provider.name,
            style=style[:80] or "soft cartoon mascot",
            requested_kinds=requested_kinds,
        )
        async with self._lock:
            self._jobs[job_id] = job
            self._persist(job)
        (source_dir / "source-meta.json").write_text(
            json.dumps({"mime_type": mime_type}, ensure_ascii=False),
            encoding="utf-8",
        )
        return job.model_copy(deep=True)

    async def get(self, job_id: str) -> ImageJob | None:
        async with self._lock:
            job = self._jobs.get(job_id)
            return job.model_copy(deep=True) if job else None

    async def _update(self, job: ImageJob) -> None:
        job.updated_at = utc_now()
        async with self._lock:
            self._jobs[job.job_id] = job
            self._persist(job)

    async def process(self, job_id: str) -> None:
        job = await self.get(job_id)
        if job is None:
            return
        job.status = JobStatus.PROCESSING
        await self._update(job)
        source_dir = self.settings.assets_root / job_id
        try:
            meta = json.loads((source_dir / "source-meta.json").read_text(encoding="utf-8"))
            source_path = next(source_dir.glob("source.*"))
            mime_type = meta["mime_type"]
            try:
                # Demo processing stays local even when a MiMo key is configured.
                if self.image_provider.name == "demo":
                    raise ProviderError("DEMO_LOCAL", "Local image processing")
                prompt = await self.image_describer.describe_reference_image(
                    source_path, mime_type, job.style
                )
            except ProviderError:
                prompt = (
                    "A friendly VelaPet smartwatch mascot based on the reference, "
                    f"{job.style}, centered, clean background, no text"
                )
            generated = await self.image_provider.generate(source_path, mime_type, prompt)
            artifacts = await asyncio.to_thread(
                self._write_variants, job_id, generated, job.requested_kinds
            )
            job.artifacts = artifacts
            job.status = JobStatus.SUCCEEDED
            job.error_code = None
            job.error_message = None
        except ProviderError as exc:
            job.status = JobStatus.FAILED
            job.error_code = exc.code
            job.error_message = str(exc)[:240]
        except (OSError, ValueError, StopIteration, UnidentifiedImageError) as exc:
            job.status = JobStatus.FAILED
            job.error_code = "ASSET_PROCESSING_FAILED"
            job.error_message = str(exc)[:240]
        except Exception as exc:
            job.status = JobStatus.FAILED
            job.error_code = "ASSET_PROCESSING_FAILED"
            job.error_message = "Image processing failed"
        await self._update(job)

    def _write_variants(
        self, job_id: str, generated: bytes, requested_kinds: list[AssetKind]
    ) -> list[AssetRecord]:
        output_dir = self.settings.assets_root / job_id
        if len(generated) > 20 * 1024 * 1024:
            raise ValueError("Generated image exceeds 20 MB")
        try:
            source = Image.open(io.BytesIO(generated))
            if source.width * source.height > self.settings.max_image_pixels:
                raise ValueError("Generated image dimensions exceed limit")
            source.load()
        except (UnidentifiedImageError, OSError) as exc:
            raise ValueError("Image provider output is not a valid image") from exc
        source = ImageOps.exif_transpose(source).convert("RGBA")
        artifacts: list[AssetRecord] = []
        for kind in requested_kinds:
            width, height = ASSET_SIZES[kind]
            image = ImageOps.fit(source, (width, height), method=Image.Resampling.LANCZOS)
            output = io.BytesIO()
            image.save(output, format="PNG", optimize=True)
            payload = output.getvalue()
            digest = hashlib.sha256(payload).hexdigest()
            version = digest[:12]
            filename = f"{kind.value}-{version}.png"
            path = output_dir / filename
            path.write_bytes(payload)
            relative_url = f"/api/v1/pet-assets/{job_id}/files/{filename}"
            download_url = (
                f"{self.settings.public_base_url}{relative_url}"
                if self.settings.public_base_url
                else relative_url
            )
            artifacts.append(
                AssetRecord(
                    kind=kind,
                    version=version,
                    sha256=digest,
                    byte_size=len(payload),
                    width=width,
                    height=height,
                    filename=filename,
                    download_url=download_url,
                    device_path=f"/data/agent/assets/pet/{DEVICE_FILENAMES[kind]}",
                )
            )
        return artifacts

    async def resolve_file(self, job_id: str, filename: str) -> Path | None:
        job = await self.get(job_id)
        if job is None or job.status != JobStatus.SUCCEEDED:
            return None
        if not any(asset.filename == filename for asset in job.artifacts):
            return None
        path = (self.settings.assets_root / job_id / filename).resolve()
        expected_parent = (self.settings.assets_root / job_id).resolve()
        if path.parent != expected_parent or not path.is_file():
            return None
        return path
