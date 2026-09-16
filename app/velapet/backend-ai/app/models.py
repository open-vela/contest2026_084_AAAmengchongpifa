from __future__ import annotations

from datetime import datetime, timezone
from enum import Enum
from typing import Literal

from pydantic import BaseModel, ConfigDict, Field, field_validator


def utc_now() -> datetime:
    return datetime.now(timezone.utc)


class TaskContext(BaseModel):
    model_config = ConfigDict(extra="ignore")

    description: str = Field(default="", max_length=128)
    target: int = Field(default=0, ge=0, le=10_000_000)
    progress: int = Field(default=0, ge=0, le=10_000_000)
    status: int = Field(default=0, ge=0, le=3)


class PetContext(BaseModel):
    model_config = ConfigDict(extra="ignore", populate_by_name=True)

    pet_name: str = Field(default="Vela", min_length=1, max_length=32)
    level: int = Field(default=1, ge=1, le=10_000)
    exp: int = Field(default=0, ge=0, le=10_000_000)
    exp_to_next: int = Field(default=100, ge=1, le=10_000_000)
    title: str = Field(default="egg_baby", max_length=32)
    personality: str = Field(default="cheerful", max_length=32)
    current_skin: str = Field(default="default", max_length=64)
    unlocked_skins: list[str] = Field(default_factory=list, max_length=8)
    total_steps: int = Field(default=0, ge=0, le=2_000_000_000)
    consecutive_days: int = Field(default=0, ge=0, le=100_000)
    daily_progress: int = Field(default=0, ge=0, le=100)
    tasks: list[TaskContext] = Field(default_factory=list, max_length=8)

    @field_validator("unlocked_skins")
    @classmethod
    def validate_skin_names(cls, values: list[str]) -> list[str]:
        return [value[:64] for value in values if value]


class PromptType(str, Enum):
    CHEER = "cheer"
    TASK = "task"
    LEVEL = "level"
    GREETING = "greeting"
    CUSTOM = "custom"


class ChatRequest(BaseModel):
    model_config = ConfigDict(extra="forbid")

    device_id: str = Field(min_length=1, max_length=64, pattern=r"^[A-Za-z0-9_.-]+$")
    prompt_type: PromptType = PromptType.CHEER
    message: str = Field(default="", max_length=240)
    locale: Literal["zh-CN", "en-US"] = "zh-CN"
    max_chars: int = Field(default=50, ge=16, le=80)
    context: PetContext


class ReplyIntent(str, Enum):
    GREETING = "greeting"
    REMINDER = "reminder"
    ENCOURAGEMENT = "encouragement"
    LEVEL_UP = "level_up"
    CHAT = "chat"


class ReplyEmotion(str, Enum):
    CHEERFUL = "cheerful"
    PROUD = "proud"
    GENTLE = "gentle"
    EXCITED = "excited"
    CALM = "calm"


class RewardDecision(BaseModel):
    eligible: bool = False
    event: Literal["ai_encouragement"] | None = None
    event_id: str | None = None


class ChatResponse(BaseModel):
    request_id: str
    reply: str
    intent: ReplyIntent
    emotion: ReplyEmotion
    source: Literal["mimo", "local_fallback"]
    reward: RewardDecision = Field(default_factory=RewardDecision)
    error_code: str | None = None
    created_at: datetime = Field(default_factory=utc_now)


class JobStatus(str, Enum):
    QUEUED = "queued"
    PROCESSING = "processing"
    SUCCEEDED = "succeeded"
    FAILED = "failed"


class AssetKind(str, Enum):
    AVATAR = "avatar"
    HOME = "home"
    SKIN = "skin"


class AssetRecord(BaseModel):
    kind: AssetKind
    version: str
    sha256: str
    byte_size: int
    width: int
    height: int
    mime_type: Literal["image/png"] = "image/png"
    filename: str
    download_url: str
    device_path: str


class ImageJob(BaseModel):
    job_id: str
    device_id: str
    status: JobStatus
    provider: str
    style: str
    requested_kinds: list[AssetKind]
    artifacts: list[AssetRecord] = Field(default_factory=list)
    error_code: str | None = None
    error_message: str | None = None
    created_at: datetime = Field(default_factory=utc_now)
    updated_at: datetime = Field(default_factory=utc_now)


class HealthResponse(BaseModel):
    status: Literal["ok"] = "ok"
    version: str
    chat_provider: Literal["mimo", "local_fallback"]
    image_provider: str
    auth_required: bool


class ApiError(BaseModel):
    error_code: str
    message: str
    retryable: bool = False

