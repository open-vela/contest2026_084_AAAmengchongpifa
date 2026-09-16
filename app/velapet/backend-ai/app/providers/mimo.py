from __future__ import annotations

import base64
import json
from dataclasses import dataclass
from pathlib import Path
from typing import Any

import httpx

from ..config import Settings
from ..models import ChatRequest, ReplyEmotion, ReplyIntent
from ..prompts import SYSTEM_PROMPT, build_user_prompt, sanitize_reply


class ProviderError(RuntimeError):
    def __init__(self, code: str, message: str, *, retryable: bool = False):
        super().__init__(message)
        self.code = code
        self.retryable = retryable


@dataclass(slots=True)
class ProviderReply:
    reply: str
    intent: ReplyIntent
    emotion: ReplyEmotion


class MiMoClient:
    def __init__(self, settings: Settings, client: httpx.AsyncClient | None = None):
        self.settings = settings
        self._client = client

    @property
    def enabled(self) -> bool:
        return bool(self.settings.mimo_api_key)

    async def _post(self, payload: dict[str, Any]) -> dict[str, Any]:
        if not self.enabled:
            raise ProviderError("MIMO_NOT_CONFIGURED", "MIMO_API_KEY is not configured")

        headers = {
            "Authorization": f"Bearer {self.settings.mimo_api_key}",
            "Content-Type": "application/json",
        }
        owns_client = self._client is None
        client = self._client or httpx.AsyncClient(timeout=self.settings.mimo_timeout_seconds)
        try:
            response = await client.post(
                f"{self.settings.mimo_api_base_url}/chat/completions",
                headers=headers,
                json=payload,
            )
        except httpx.HTTPError as exc:
            raise ProviderError("MIMO_UNAVAILABLE", str(exc), retryable=True) from exc
        finally:
            if owns_client:
                await client.aclose()

        if response.status_code == 429:
            raise ProviderError("MIMO_RATE_LIMITED", "MiMo rate limit exceeded", retryable=True)
        if response.status_code >= 500:
            raise ProviderError("MIMO_UPSTREAM_ERROR", "MiMo service error", retryable=True)
        if response.status_code >= 400:
            raise ProviderError(
                "MIMO_REQUEST_REJECTED",
                f"MiMo returned HTTP {response.status_code}",
                retryable=False,
            )
        try:
            return response.json()
        except ValueError as exc:
            raise ProviderError("MIMO_INVALID_RESPONSE", "MiMo returned invalid JSON") from exc

    @staticmethod
    def _message_content(data: dict[str, Any]) -> str:
        try:
            content = data["choices"][0]["message"]["content"]
        except (KeyError, IndexError, TypeError) as exc:
            raise ProviderError("MIMO_INVALID_RESPONSE", "MiMo response has no content") from exc
        if isinstance(content, str):
            return content
        if isinstance(content, list):
            return "".join(
                item.get("text", "") for item in content if isinstance(item, dict)
            )
        raise ProviderError("MIMO_INVALID_RESPONSE", "Unsupported MiMo content format")

    @staticmethod
    def _parse_reply(raw: str, max_chars: int) -> ProviderReply:
        text = raw.strip()
        if text.startswith("```"):
            text = text.removeprefix("```json").removeprefix("```")
            text = text.removesuffix("```").strip()
        start = text.find("{")
        end = text.rfind("}")
        if start >= 0 and end > start:
            try:
                payload = json.loads(text[start : end + 1])
                reply = payload.get("reply")
                if not isinstance(reply, str) or not reply.strip():
                    raise ValueError("Missing reply")
                return ProviderReply(
                    reply=sanitize_reply(reply, max_chars),
                    intent=ReplyIntent(payload.get("intent", ReplyIntent.CHAT.value)),
                    emotion=ReplyEmotion(
                        payload.get("emotion", ReplyEmotion.CHEERFUL.value)
                    ),
                )
            except (ValueError, TypeError, AttributeError) as exc:
                raise ProviderError("MIMO_INVALID_RESPONSE", "Invalid structured reply") from exc

        cleaned = sanitize_reply(text, max_chars)
        if not cleaned:
            raise ProviderError("MIMO_EMPTY_RESPONSE", "MiMo returned an empty reply")
        return ProviderReply(cleaned, ReplyIntent.CHAT, ReplyEmotion.CHEERFUL)

    async def generate_reply(self, request: ChatRequest) -> ProviderReply:
        payload = {
            "model": self.settings.mimo_chat_model,
            "messages": [
                {"role": "system", "content": SYSTEM_PROMPT},
                {"role": "user", "content": build_user_prompt(request)},
            ],
            "max_completion_tokens": 256,
            "stream": False,
            "thinking": {"type": "disabled"},
        }
        data = await self._post(payload)
        return self._parse_reply(self._message_content(data), request.max_chars)

    async def describe_reference_image(
        self, source_path: Path, mime_type: str, style: str
    ) -> str:
        if not self.enabled:
            return _default_image_prompt(style)
        encoded = base64.b64encode(source_path.read_bytes()).decode("ascii")
        payload = {
            "model": self.settings.mimo_vision_model,
            "messages": [
                {
                    "role": "system",
                    "content": (
                        "Analyze the reference image for a pet-avatar generator. "
                        "Describe only visible species, colors, markings, face and accessories. "
                        "Do not identify people. Return one concise English image prompt."
                    ),
                },
                {
                    "role": "user",
                    "content": [
                        {
                            "type": "text",
                            "text": (
                                "Create a friendly square VelaPet smartwatch mascot. "
                                f"Requested style: {style}. Keep the subject recognizable."
                            ),
                        },
                        {
                            "type": "image_url",
                            "image_url": {
                                "url": f"data:{mime_type};base64,{encoded}"
                            },
                        },
                    ],
                },
            ],
            "max_completion_tokens": 300,
            "stream": False,
            "thinking": {"type": "disabled"},
        }
        data = await self._post(payload)
        prompt = sanitize_reply(self._message_content(data), 800)
        return prompt or _default_image_prompt(style)


def _default_image_prompt(style: str) -> str:
    return (
        "A friendly VelaPet smartwatch mascot based on the reference image, "
        f"{style} style, centered full character, simple silhouette, soft lighting, "
        "clean background, readable at 128 pixels, no text, no watermark"
    )
