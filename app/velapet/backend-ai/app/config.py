from __future__ import annotations

import os
from dataclasses import dataclass, field
from pathlib import Path


def _env_int(name: str, default: int) -> int:
    raw = os.getenv(name)
    if raw is None or raw.strip() == "":
        return default
    try:
        return int(raw)
    except ValueError as exc:
        raise ValueError(f"{name} must be an integer") from exc


def _env_float(name: str, default: float) -> float:
    raw = os.getenv(name)
    if raw is None or raw.strip() == "":
        return default
    try:
        return float(raw)
    except ValueError as exc:
        raise ValueError(f"{name} must be a number") from exc


def _env_tokens(name: str) -> frozenset[str]:
    return frozenset(
        token.strip() for token in os.getenv(name, "").split(",") if token.strip()
    )


@dataclass(frozen=True, slots=True)
class Settings:
    app_name: str = "VelaPet Backend"
    app_version: str = "0.1.0"
    host: str = "0.0.0.0"
    port: int = 8000
    public_base_url: str = ""
    data_root: Path = field(default_factory=lambda: Path("runtime-data"))
    device_tokens: frozenset[str] = field(default_factory=frozenset)
    chat_rate_limit_per_minute: int = 20
    max_upload_bytes: int = 5 * 1024 * 1024
    max_image_pixels: int = 16_000_000

    mimo_api_key: str = ""
    mimo_api_base_url: str = "https://api.xiaomimimo.com/v1"
    mimo_chat_model: str = "mimo-v2.5-pro"
    mimo_vision_model: str = "mimo-v2.5"
    mimo_timeout_seconds: float = 30.0

    ark_vision_enabled: bool = False
    ark_vision_model: str = "doubao-seed-2-1-turbo-260628"

    image_provider: str = "demo"
    image_api_base_url: str = ""
    image_api_key: str = ""
    image_model: str = ""
    image_api_mode: str = "edits"
    image_size: str = "2K"
    image_watermark: bool = False
    image_response_format: str = "url"
    image_timeout_seconds: float = 120.0

    @classmethod
    def from_env(cls) -> "Settings":
        return cls(
            host=os.getenv("VELAPET_HOST", "0.0.0.0"),
            port=_env_int("VELAPET_PORT", 8000),
            public_base_url=os.getenv("VELAPET_PUBLIC_BASE_URL", "").rstrip("/"),
            data_root=Path(os.getenv("VELAPET_DATA_ROOT", "runtime-data")),
            device_tokens=_env_tokens("VELAPET_DEVICE_TOKENS"),
            chat_rate_limit_per_minute=_env_int(
                "VELAPET_CHAT_RATE_LIMIT_PER_MINUTE", 20
            ),
            max_upload_bytes=_env_int("VELAPET_MAX_UPLOAD_BYTES", 5 * 1024 * 1024),
            max_image_pixels=_env_int("VELAPET_MAX_IMAGE_PIXELS", 16_000_000),
            mimo_api_key=os.getenv("MIMO_API_KEY", "").strip(),
            mimo_api_base_url=os.getenv(
                "MIMO_API_BASE_URL", "https://api.xiaomimimo.com/v1"
            ).rstrip("/"),
            mimo_chat_model=os.getenv("MIMO_CHAT_MODEL", "mimo-v2.5-pro"),
            mimo_vision_model=os.getenv("MIMO_VISION_MODEL", "mimo-v2.5"),
            mimo_timeout_seconds=_env_float("MIMO_TIMEOUT_SECONDS", 30.0),
            ark_vision_enabled=os.getenv("ARK_VISION_ENABLED", "false").strip().lower()
            in {"1", "true", "yes", "on"},
            ark_vision_model=os.getenv(
                "ARK_VISION_MODEL", "doubao-seed-2-1-turbo-260628"
            ).strip(),
            image_provider=os.getenv("VELAPET_IMAGE_PROVIDER", "demo").strip().lower(),
            image_api_base_url=os.getenv("IMAGE_API_BASE_URL", "").rstrip("/"),
            image_api_key=os.getenv("IMAGE_API_KEY", "").strip(),
            image_model=os.getenv("IMAGE_MODEL", "").strip(),
            image_api_mode=os.getenv("IMAGE_API_MODE", "edits").strip().lower(),
            image_size=os.getenv("IMAGE_SIZE", "2K").strip(),
            image_watermark=os.getenv("IMAGE_WATERMARK", "false").strip().lower()
            in {"1", "true", "yes", "on"},
            image_response_format=os.getenv("IMAGE_RESPONSE_FORMAT", "url").strip().lower(),
            image_timeout_seconds=_env_float("IMAGE_TIMEOUT_SECONDS", 120.0),
        )

    @property
    def jobs_root(self) -> Path:
        return self.data_root / "jobs"

    @property
    def assets_root(self) -> Path:
        return self.data_root / "assets"

    @property
    def auth_required(self) -> bool:
        return bool(self.device_tokens)
