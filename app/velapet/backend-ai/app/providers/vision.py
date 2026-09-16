from __future__ import annotations

import base64
from pathlib import Path
from typing import Protocol

import httpx

from ..config import Settings
from ..prompts import sanitize_reply
from .mimo import ProviderError


class ReferenceImageDescriber(Protocol):
    async def describe_reference_image(
        self, source_path: Path, mime_type: str, style: str
    ) -> str: ...


class VolcengineArkVisionClient:
    """Describe a reference image with Ark's multimodal chat endpoint."""

    def __init__(self, settings: Settings, client: httpx.AsyncClient | None = None):
        self.settings = settings
        self._client = client

    async def describe_reference_image(
        self, source_path: Path, mime_type: str, style: str
    ) -> str:
        if not self.settings.image_api_key or not self.settings.ark_vision_model:
            raise ProviderError("ARK_VISION_NOT_CONFIGURED", "Ark vision is not configured")
        encoded = base64.b64encode(source_path.read_bytes()).decode("ascii")
        payload = {
            "model": self.settings.ark_vision_model,
            "messages": [{
                "role": "user",
                "content": [
                    {"type": "image_url", "image_url": {"url": f"data:{mime_type};base64,{encoded}"}},
                    {"type": "text", "text": (
                        "Describe only the visible animal, colors, markings, face and accessories "
                        "as one concise English image-generation prompt. Transform it into a "
                        f"friendly VelaPet smartwatch mascot in {style} style, centered, clean "
                        "background, no text, no watermark."
                    )},
                ],
            }],
        }
        owns_client = self._client is None
        client = self._client or httpx.AsyncClient(timeout=self.settings.image_timeout_seconds)
        try:
            response = await client.post(
                f"{self.settings.image_api_base_url}/chat/completions",
                headers={"Authorization": f"Bearer {self.settings.image_api_key}", "Content-Type": "application/json"},
                json=payload,
            )
            if response.status_code == 429:
                raise ProviderError("ARK_VISION_RATE_LIMITED", "Ark vision rate limited", retryable=True)
            if response.status_code >= 500:
                raise ProviderError("ARK_VISION_UNAVAILABLE", "Ark vision unavailable", retryable=True)
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
                    "ARK_VISION_REJECTED",
                    f"Ark vision returned HTTP {response.status_code} ({upstream_code})",
                )
            data = response.json()
            content = data["choices"][0]["message"]["content"]
            if not isinstance(content, str):
                raise TypeError("content is not text")
            prompt = sanitize_reply(content, 800)
            if not prompt:
                raise ValueError("empty content")
            return prompt
        except httpx.TimeoutException as exc:
            raise ProviderError("ARK_VISION_TIMEOUT", "Ark vision timed out", retryable=True) from exc
        except httpx.HTTPError as exc:
            raise ProviderError("ARK_VISION_UNAVAILABLE", "Could not reach Ark vision", retryable=True) from exc
        except (ValueError, TypeError, KeyError, IndexError) as exc:
            raise ProviderError("ARK_VISION_INVALID_RESPONSE", "Invalid Ark vision response") from exc
        finally:
            if owns_client:
                await client.aclose()
