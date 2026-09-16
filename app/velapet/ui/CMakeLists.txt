cmake_minimum_required(VERSION 3.16)

project(velapet_member3_ui C)

option(VELAPET_UI_BUILD_MOCK "Build VelaPet UI with local mock data" ON)
set(VELAPET_APP_INCLUDE_DIR "" CACHE PATH "Directory containing the production vela_pet_api.h")
set(VELAPET_AI_BRIDGE_INCLUDE_DIR "" CACHE PATH "Directory containing velapet_ai_bridge.h")
set(VELAPET_UI_LVGL_FS_PREFIX "S:" CACHE STRING "LVGL drive prefix mapped to /data/agent/assets/pet")

set(VELAPET_UI_SRCS
    src/ui/velapet_ui.c
    src/ui/ui_router.c
    src/ui/ui_common.c
    src/ui/ui_home.c
    src/ui/ui_task.c
    src/ui/ui_chat.c
    src/ui/ui_pet_image.c
    src/ui/ui_skin.c
)

if(VELAPET_UI_BUILD_MOCK)
    list(APPEND VELAPET_UI_SRCS src/ui/velapet_ui_mock.c)
endif()

add_library(velapet_member3_ui STATIC ${VELAPET_UI_SRCS})

target_compile_definitions(velapet_member3_ui PUBLIC
    VELAPET_UI_LVGL_FS_PREFIX="${VELAPET_UI_LVGL_FS_PREFIX}"
)

target_include_directories(velapet_member3_ui PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/src/ui
)

if(VELAPET_UI_BUILD_MOCK)
    target_include_directories(velapet_member3_ui PUBLIC ${CMAKE_CURRENT_LIST_DIR}/src)
    target_compile_definitions(velapet_member3_ui PUBLIC VELAPET_UI_USE_MOCK)
else()
    if(NOT VELAPET_APP_INCLUDE_DIR)
        message(FATAL_ERROR "Production UI requires -DVELAPET_APP_INCLUDE_DIR=<apps/.../src>")
    endif()
    if(NOT VELAPET_AI_BRIDGE_INCLUDE_DIR)
        message(FATAL_ERROR "Production UI requires -DVELAPET_AI_BRIDGE_INCLUDE_DIR=<AI bridge include directory>")
    endif()
    target_include_directories(velapet_member3_ui BEFORE PUBLIC ${VELAPET_APP_INCLUDE_DIR})
    target_include_directories(velapet_member3_ui PUBLIC ${VELAPET_AI_BRIDGE_INCLUDE_DIR})
endif()

# The parent openvela project should link LVGL and platform drivers.
# Example in parent target:
# target_link_libraries(velapet_app PRIVATE velapet_member3_ui lvgl)
