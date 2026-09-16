if(CONFIG_EXAMPLES_VELAPET)
  set(VELAPET_SRCS
      src/vela_pet_main.c
      src/pet.c
      src/task.c
      src/reward.c
      src/storage.c
      src/json_utils.c
      src/vela_pet_api.c
      src/vela_pet_service.c
      ui/src/ui/velapet_ui.c
      ui/src/ui/ui_router.c
      ui/src/ui/ui_common.c
      ui/src/ui/ui_home.c
      ui/src/ui/ui_task.c
      ui/src/ui/ui_chat.c
      ui/src/ui/ui_pet_image.c
      ui/src/ui/ui_skin.c
      backend-ai/device/src/velapet_ai_client.c
      backend-ai/device/src/velapet_ai_bridge.c)

  set(VELAPET_DEFS)

  if(CONFIG_EXAMPLES_VELAPET_DEMO_INPUT)
    list(APPEND VELAPET_SRCS src/vela_pet_demo_input.c)
    # Raises the daily-steps reward so the level-up path can be reached in a
    # single demo session.  Host builds do not define it, so the regression
    # tests keep checking the production balance.
    list(APPEND VELAPET_DEFS VELAPET_DEMO_REWARD)
  endif()

  nuttx_add_application(
    NAME ${CONFIG_EXAMPLES_VELAPET_PROGNAME}
    SRCS ${VELAPET_SRCS}
    PRIORITY ${CONFIG_EXAMPLES_VELAPET_PRIORITY}
    STACKSIZE ${CONFIG_EXAMPLES_VELAPET_STACKSIZE}
    MODULE ${CONFIG_EXAMPLES_VELAPET}
    DEFINITIONS ${VELAPET_DEFS}
    DEPENDS lvgl
    INCLUDE_DIRECTORIES
      ${CMAKE_CURRENT_LIST_DIR}/src
      ${CMAKE_CURRENT_LIST_DIR}/ui/src/ui
      ${CMAKE_CURRENT_LIST_DIR}/backend-ai/device/include
      ${CMAKE_BINARY_DIR}/apps/include/netutils
  )
endif()
