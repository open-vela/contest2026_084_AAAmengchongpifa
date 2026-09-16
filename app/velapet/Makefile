# apps/examples/velapet/Makefile

include $(APPDIR)/Make.defs

PROGNAME  = $(CONFIG_EXAMPLES_VELAPET_PROGNAME)
PRIORITY  = $(CONFIG_EXAMPLES_VELAPET_PRIORITY)
STACKSIZE = $(CONFIG_EXAMPLES_VELAPET_STACKSIZE)
MODULE    = $(CONFIG_EXAMPLES_VELAPET)

MAINSRC = src/vela_pet_main.c

CSRCS += src/pet.c
CSRCS += src/task.c
CSRCS += src/reward.c
CSRCS += src/storage.c
CSRCS += src/json_utils.c
CSRCS += src/vela_pet_api.c
CSRCS += src/vela_pet_service.c

include $(APPDIR)/Application.mk
