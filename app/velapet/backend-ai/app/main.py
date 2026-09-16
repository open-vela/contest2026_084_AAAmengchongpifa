from __future__ import annotations

from contextlib import asynccontextmanager

from fastapi import (
    BackgroundTasks,
    Depends,
    FastAPI,
    File,
    Form,
    HTTPException,
    Request,
    UploadFile,
    status,
)
from fastapi.responses import FileResponse, JSONResponse

from .config import Settings
from .models import (
    ApiError,
    AssetKind,
    ChatRequest,
    ChatResponse,
    HealthResponse,
    ImageJob,
)
from .providers.image import ImageProvider, build_image_provider
from .providers.mimo import MiMoClient
from .providers.vision import ReferenceImageDescriber, VolcengineArkVisionClient
from .security import SlidingWindowRateLimiter, require_device_token
from .services.assets import AssetService, AssetValidationError
from .services.chat import ChatService


def _parse_kinds(raw: str) -> list[AssetKind]:
    values: list[AssetKind] = []
    try:
        for item in raw.split(","):
            kind = AssetKind(item.strip().lower())
            if kind not in values:
                values.append(kind)
    except ValueError as exc:
        raise HTTPException(
            status_code=status.HTTP_422_UNPROCESSABLE_ENTITY,
            detail={
                "error_code": "INVALID_ASSET_KIND",
                "message": "asset_kinds must contain avatar, home or skin",
            },
        ) from exc
    if not values:
        raise HTTPException(
            status_code=status.HTTP_422_UNPROCESSABLE_ENTITY,
            detail={"error_code": "INVALID_ASSET_KIND", "message": "No asset kind requested"},
        )
    return values


def create_app(
    settings: Settings | None = None,
    *,
    mimo_client: MiMoClient | None = None,
    image_provider: ImageProvider | None = None,
    image_describer: ReferenceImageDescriber | None = None,
) -> FastAPI:
    cfg = settings or Settings.from_env()
    cfg.data_root.mkdir(parents=True, exist_ok=True)
    mimo = mimo_client or MiMoClient(cfg)
    provider = image_provider or build_image_provider(cfg)
    describer = image_describer or (
        VolcengineArkVisionClient(cfg) if cfg.ark_vision_enabled else mimo
    )
    chat_service = ChatService(mimo)
    asset_service = AssetService(cfg, mimo, provider, describer)
    limiter = SlidingWindowRateLimiter(cfg.chat_rate_limit_per_minute)

    @asynccontextmanager
    async def lifespan(_: FastAPI):
        yield

    app = FastAPI(
        title=cfg.app_name,
        version=cfg.app_version,
        description="VelaPet device-facing AI and personalized pet asset API",
        lifespan=lifespan,
    )
    app.state.settings = cfg
    app.state.asset_service = asset_service

    @app.exception_handler(AssetValidationError)
    async def asset_validation_handler(_: Request, exc: AssetValidationError):
        return JSONResponse(
            status_code=status.HTTP_422_UNPROCESSABLE_ENTITY,
            content=ApiError(error_code=exc.code, message=str(exc)).model_dump(),
        )

    @app.get("/health", response_model=HealthResponse)
    async def health() -> HealthResponse:
        return HealthResponse(
            version=cfg.app_version,
            chat_provider="mimo" if mimo.enabled else "local_fallback",
            image_provider=provider.name,
            auth_required=cfg.auth_required,
        )

    @app.post(
        "/api/v1/chat",
        response_model=ChatResponse,
        dependencies=[Depends(require_device_token)],
    )
    async def chat(body: ChatRequest) -> ChatResponse:
        if not limiter.allow(body.device_id):
            raise HTTPException(
                status_code=status.HTTP_429_TOO_MANY_REQUESTS,
                detail={
                    "error_code": "RATE_LIMITED",
                    "message": "Too many chat requests for this device",
                },
            )
        return await chat_service.reply(body)

    @app.post(
        "/api/v1/pet-assets/jobs",
        response_model=ImageJob,
        status_code=status.HTTP_202_ACCEPTED,
        dependencies=[Depends(require_device_token)],
    )
    async def create_asset_job(
        background_tasks: BackgroundTasks,
        file: UploadFile = File(...),
        device_id: str = Form(..., min_length=1, max_length=64, pattern=r"^[A-Za-z0-9_.-]+$"),
        style: str = Form("soft cartoon mascot", max_length=80),
        asset_kinds: str = Form("avatar,home,skin"),
    ) -> ImageJob:
        payload = await file.read(cfg.max_upload_bytes + 1)
        job = await asset_service.submit(
            device_id=device_id,
            payload=payload,
            declared_mime=file.content_type or "",
            style=style,
            requested_kinds=_parse_kinds(asset_kinds),
        )
        background_tasks.add_task(asset_service.process, job.job_id)
        return job

    @app.get(
        "/api/v1/pet-assets/jobs/{job_id}",
        response_model=ImageJob,
        dependencies=[Depends(require_device_token)],
    )
    async def get_asset_job(job_id: str) -> ImageJob:
        job = await asset_service.get(job_id)
        if job is None:
            raise HTTPException(
                status_code=status.HTTP_404_NOT_FOUND,
                detail={"error_code": "JOB_NOT_FOUND", "message": "Unknown asset job"},
            )
        return job

    @app.get(
        "/api/v1/pet-assets/{job_id}/files/{filename}",
        dependencies=[Depends(require_device_token)],
    )
    async def download_asset(job_id: str, filename: str) -> FileResponse:
        path = await asset_service.resolve_file(job_id, filename)
        if path is None:
            raise HTTPException(
                status_code=status.HTTP_404_NOT_FOUND,
                detail={"error_code": "ASSET_NOT_FOUND", "message": "Unknown asset"},
            )
        return FileResponse(
            path,
            media_type="image/png",
            filename=filename,
            headers={"Cache-Control": "private, max-age=31536000, immutable"},
        )

    return app


app = create_app()
