from __future__ import annotations

import hmac
import time
from collections import defaultdict, deque

from fastapi import Header, HTTPException, Request, status

from .config import Settings


async def require_device_token(
    request: Request, x_velapet_token: str | None = Header(default=None)
) -> None:
    settings: Settings = request.app.state.settings
    if not settings.auth_required:
        return
    if not x_velapet_token:
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail={"error_code": "AUTH_REQUIRED", "message": "Missing device token"},
        )
    if not any(
        hmac.compare_digest(x_velapet_token, expected)
        for expected in settings.device_tokens
    ):
        raise HTTPException(
            status_code=status.HTTP_403_FORBIDDEN,
            detail={"error_code": "AUTH_INVALID", "message": "Invalid device token"},
        )


class SlidingWindowRateLimiter:
    def __init__(self, requests_per_minute: int):
        self.limit = max(1, requests_per_minute)
        self._hits: dict[str, deque[float]] = defaultdict(deque)

    def allow(self, key: str) -> bool:
        now = time.monotonic()
        cutoff = now - 60.0
        hits = self._hits[key]
        while hits and hits[0] < cutoff:
            hits.popleft()
        if len(hits) >= self.limit:
            return False
        hits.append(now)
        return True

