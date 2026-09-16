set(VELAPET_AI_DEVICE_SRCS
    ${CMAKE_CURRENT_LIST_DIR}/src/velapet_ai_client.c
    ${CMAKE_CURRENT_LIST_DIR}/src/velapet_ai_bridge.c
)

# Add VELAPET_AI_DEVICE_SRCS to the existing VelaPet target. Required include
# paths: this directory's include/, the canonical VelaPet core src/, UI src/,
# and cJSON. Required libraries: pthread plus the board's HTTP/TLS stack.

