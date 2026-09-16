from __future__ import annotations

import base64
import io
from pathlib import Path
from typing import Protocol
from urllib.parse import urlsplit

import httpx
from PIL import Image, ImageEnhance, ImageFilter, ImageOps

from ..config import Settings
from .mimo import ProviderError


class ImageProvider(Protocol):
    name: str

    async def generate(
        self, source_path: Path, mime_type: str, prompt: str
    ) -> bytes: ...


class DemoImageProvider:
    """Offline adapter for end-to-end integration tests and live demos.

    It creates a device-ready stylized derivative of the uploaded image. It is
    deliberately reported as "demo", never as an AI-generated result.
    """

    name = "demo"

    async def generate(self, source_path: Path, mime_type: str, prompt: str) -> bytes:
        del mime_type, prompt
        with Image.open(source_path) as source:
            image = ImageOps.exif_transpose(source).convert("RGBA")
            image = ImageOps.fit(image, (1024, 1024), method=Image.Resampling.LANCZOS)
            image = ImageEnhance.Color(image).enhance(1.12)
            image = ImageEnhance.Contrast(image).enhance(1.06)
            image = image.filter(ImageFilter.SMOOTH)
            output = io.BytesIO()
            image.save(output, format="PNG", optimize=True)
            return output.getvalue()


class OpenAICompatibleImageProvider:
    name = "openai_compatible"

    def __init__(self, settings: Settings, client: httpx.AsyncClient | None = None):
        self.settings = settings
        self._client = client

    async def generate(self, source_path: Path, mime_type: str, prompt: str) -> bytes:
        if not (
            self.settings.image_api_base_url
            and self.settings.image_api_key
            and self.settings.image_model
        ):
            raise ProviderError(
                "IMAGE_PROVIDER_NOT_CONFIGURED",
                "IMAGE_API_BASE_URL, IMAGE_API_KEY and IMAGE_MODEL are required",
            )

        headers = {"Authorization": f"Bearer {self.settings.image_api_key}"}
        mode = self.settings.image_api_mode
        owns_client = self._client is None
        client = self._client or httpx.AsyncClient(
            timeout=self.settings.image_timeout_seconds
        )
        try:
            if mode == "edits":
                with source_path.open("rb") as source:
                    response = await client.post(
                        f"{self.settings.image_api_base_url}/images/edits",
                        headers=headers,
                        data={
                            "model": self.settings.image_model,
                            "prompt": prompt,
                            "size": "1024x1024",
                            "response_format": "b64_json",
                        },
                        files={"image": (source_path.name, source, mime_type)},
                    )
            elif mode == "generations":
                response = await client.post(
                    f"{self.settings.image_api_base_url}/images/generations",
                    headers={**headers, "Content-Type": "application/json"},
                    json={
                        "model": self.settings.image_model,
                        "prompt": prompt,
                        "size": "1024x1024",
                        "response_format": "b64_json",
                    },
                )
            else:
                raise ProviderError(
                    "IMAGE_PROVIDER_BAD_MODE",
                    "IMAGE_API_MODE must be edits or generations",
                )

            if response.status_code == 429:
                raise ProviderError(
                    "IMAGE_PROVIDER_RATE_LIMITED", "Image provider rate limited", retryable=True
                )
            if response.status_code >= 500:
                raise ProviderError(
                    "IMAGE_PROVIDER_UNAVAILABLE", "Image provider failed", retryable=True
                )
            if response.status_code >= 400:
                raise ProviderError(
                    "IMAGE_PROVIDER_REJECTED",
                    f"Image provider returned HTTP {response.status_code}",
                )

            data = response.json().get("data", [])
            if not data:
                raise ProviderError("IMAGE_PROVIDER_EMPTY", "Image provider returned no image")
            item = data[0]
            if item.get("b64_json"):
                if len(item["b64_json"]) > 28 * 1024 * 1024:
                    raise ProviderError("IMAGE_TOO_LARGE", "Generated image exceeds limit")
                return base64.b64decode(item["b64_json"], validate=True)
            if item.get("url"):
                raise ProviderError("IMAGE_URL_UNSUPPORTED", "Configure image provider to return b64_json")
            raise ProviderError("IMAGE_PROVIDER_EMPTY", "Image response has no data")
        except httpx.TimeoutException as exc:
            raise ProviderError(
                "IMAGE_PROVIDER_TIMEOUT", str(exc), retryable=True
            ) from exc
        except httpx.HTTPError as exc:
            raise ProviderError(
                "IMAGE_PROVIDER_UNAVAILABLE", str(exc), retryable=True
            ) from exc
        except (ValueError, TypeError) as exc:
            raise ProviderError(
                "IMAGE_PROVIDER_INVALID_RESPONSE", "Invalid image provider response"
            ) from exc
        finally:
            if owns_client:
                await client.aclose()


class VolcengineArkImageProvider:
    """Volcengine Ark Seedream image-to-image adapter.

    Ark accepts reference images in the JSON request to images/generations,
    unlike the multipart images/edits shape used by some OpenAI-compatible
    providers.
    """

    name = "volcengine_ark"

    def __init__(self, settings: Settings, client: httpx.AsyncClient | None = None):
        self.settings = settings
        self._client = client

    async def generate(self, source_path: Path, mime_type: str, prompt: str) -> bytes:
        if not (
            self.settings.image_api_base_url
            and self.settings.image_api_key
            and self.settings.image_model
        ):
            raise ProviderError(
                "IMAGE_PROVIDER_NOT_CONFIGURED",
                "IMAGE_API_BASE_URL, IMAGE_API_KEY and IMAGE_MODEL are required",
            )

        payload = {
            "model": self.settings.image_model,
            "prompt": prompt,
            "size": self.settings.image_size,
            "response_format": self.settings.image_response_format,
            "stream": False,
            "watermark": self.settings.image_watermark,
        }
        if self.settings.image_api_mode == "edits":
            encoded_source = base64.b64encode(source_path.read_bytes()).decode("ascii")
            payload["image"] = f"data:{mime_type};base64,{encoded_source}"
        elif self.settings.image_api_mode != "generations":
            raise ProviderError(
                "IMAGE_PROVIDER_BAD_MODE",
                "IMAGE_API_MODE must be edits or generations",
            )
        if self.settings.image_response_format not in {"url", "b64_json"}:
            raise ProviderError(
                "IMAGE_PROVIDER_BAD_FORMAT",
                "IMAGE_RESPONSE_FORMAT must be url or b64_json",
            )
        owns_client = self._client is None
        client = self._client or httpx.AsyncClient(
            timeout=self.settings.image_timeout_seconds
        )
        try:
            response = await client.post(
                f"{self.settings.image_api_base_url}/images/generations",
                headers={
                    "Authorization": f"Bearer {self.settings.image_api_key}",
                    "Content-Type": "application/json",
                },
                json=payload,
            )
            if response.status_code == 429:
                raise ProviderError(
                    "IMAGE_PROVIDER_RATE_LIMITED",
                    "Volcengine Ark rate limited the request",
                    retryable=True,
                )
            if response.status_code >= 500:
                raise ProviderError(
                    "IMAGE_PROVIDER_UNAVAILABLE",
                    "Volcengine Ark is temporarily unavailable",
                    retryable=True,
                )
            if response.status_code >= 400:
                upstream_code = "unknown"
                try:
                    error = response.json().get("error", {})
                    candidate = error.get("code") if isinstance(error, dict) else None
                    if isinstance(candidate, str) and candidate.replace("_", "").replace("-", "").isalnum():
                        upstream_code = candidate[:80]
                except (ValueError, TypeError, AttributeError):
                    pass
                raise ProviderError(
                    "IMAGE_PROVIDER_REJECTED",
                    f"Volcengine Ark returned HTTP {response.status_code} ({upstream_code})",
                )
            data = response.json().get("data", [])
            if not data:
                raise ProviderError("IMAGE_PROVIDER_EMPTY", "Volcengine Ark returned no image")
            encoded = data[0].get("b64_json")
            if isinstance(encoded, str) and encoded:
                if len(encoded) > 28 * 1024 * 1024:
                    raise ProviderError("IMAGE_TOO_LARGE", "Generated image exceeds limit")
                return base64.b64decode(encoded, validate=True)
            image_url = data[0].get("url")
            if isinstance(image_url, str) and image_url:
                return await self._download_result(client, image_url)
            raise ProviderError("IMAGE_PROVIDER_EMPTY", "Image response has no data")
        except httpx.TimeoutException as exc:
            raise ProviderError(
                "IMAGE_PROVIDER_TIMEOUT", "Volcengine Ark request timed out", retryable=True
            ) from exc
        except httpx.HTTPError as exc:
            raise ProviderError(
                "IMAGE_PROVIDER_UNAVAILABLE",
                "Could not reach Volcengine Ark",
                retryable=True,
            ) from exc
        except (ValueError, TypeError) as exc:
            raise ProviderError(
                "IMAGE_PROVIDER_INVALID_RESPONSE", "Invalid Volcengine Ark response"
            ) from exc
        finally:
            if owns_client:
                await client.aclose()

    async def _download_result(self, client: httpx.AsyncClient, image_url: str) -> bytes:
        parsed = urlsplit(image_url)
        hostname = (parsed.hostname or "").lower()
        allowed = hostname == "volces.com" or hostname.endswith(".volces.com")
        if parsed.scheme != "https" or not allowed or parsed.username or parsed.password:
            raise ProviderError(
                "IMAGE_URL_UNTRUSTED", "Ark returned an untrusted image URL"
            )
        if parsed.port not in (None, 443):
            raise ProviderError(
                "IMAGE_URL_UNTRUSTED", "Ark returned an untrusted image URL"
            )
        async with client.stream("GET", image_url, follow_redirects=False) as response:
            if response.status_code >= 400:
                raise ProviderError(
                    "IMAGE_DOWNLOAD_FAILED",
                    f"Ark image download returned HTTP {response.status_code}",
                    retryable=response.status_code >= 500,
                )
            if 300 <= response.status_code < 400:
                raise ProviderError("IMAGE_URL_UNTRUSTED", "Ark image URL redirected")
            content_type = response.headers.get("content-type", "").split(";", 1)[0]
            if content_type and not content_type.startswith("image/"):
                raise ProviderError("IMAGE_DOWNLOAD_INVALID", "Ark URL did not return an image")
            chunks: list[bytes] = []
            total = 0
            async for chunk in response.aiter_bytes():
                total += len(chunk)
                if total > 20 * 1024 * 1024:
                    raise ProviderError("IMAGE_TOO_LARGE", "Generated image exceeds limit")
                chunks.append(chunk)
            if not chunks:
                raise ProviderError("IMAGE_PROVIDER_EMPTY", "Ark returned an empty image")
            return b"".join(chunks)


def build_image_provider(settings: Settings) -> ImageProvider:
    if settings.image_provider == "demo":
        return DemoImageProvider()
    if settings.image_provider == "openai_compatible":
        return OpenAICompatibleImageProvider(settings)
    if settings.image_provider == "volcengine_ark":
        return VolcengineArkImageProvider(settings)
    raise ValueError(
        "VELAPET_IMAGE_PROVIDER must be demo, openai_compatible or volcengine_ark"
    )
