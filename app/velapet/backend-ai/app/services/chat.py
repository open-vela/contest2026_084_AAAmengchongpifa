from __future__ import annotations

from uuid import uuid4

from ..models import (
    ChatRequest,
    ChatResponse,
    PromptType,
    RewardDecision,
    ReplyIntent,
)
from ..prompts import local_reply
from ..providers.mimo import MiMoClient, ProviderError


class ChatService:
    def __init__(self, mimo: MiMoClient):
        self.mimo = mimo

    async def reply(self, request: ChatRequest) -> ChatResponse:
        request_id = uuid4().hex
        reward = RewardDecision()
        if request.prompt_type == PromptType.CHEER:
            reward = RewardDecision(
                eligible=True,
                event="ai_encouragement",
                event_id=request_id,
            )

        if self.mimo.enabled:
            try:
                result = await self.mimo.generate_reply(request)
                if result.intent != ReplyIntent.ENCOURAGEMENT:
                    reward = RewardDecision()
                return ChatResponse(
                    request_id=request_id,
                    reply=result.reply,
                    intent=result.intent,
                    emotion=result.emotion,
                    source="mimo",
                    reward=reward,
                )
            except ProviderError as exc:
                reply, intent, emotion = local_reply(request)
                return ChatResponse(
                    request_id=request_id,
                    reply=reply,
                    intent=intent,
                    emotion=emotion,
                    source="local_fallback",
                    reward=reward,
                    error_code=exc.code,
                )

        reply, intent, emotion = local_reply(request)
        return ChatResponse(
            request_id=request_id,
            reply=reply,
            intent=intent,
            emotion=emotion,
            source="local_fallback",
            reward=reward,
            error_code="MIMO_NOT_CONFIGURED",
        )
