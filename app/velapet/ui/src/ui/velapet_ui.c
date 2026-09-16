#include "ui_internal.h"

lv_obj_t *g_ui_root = NULL;
UiPage g_ui_page = UI_PAGE_HOME;

int velapet_ui_init(void)
{
    if (vela_pet_init() != 0) {
        return -1;
    }

    PetProfile *pet = vela_pet_get_profile();
    ui_apply_theme_by_skin(pet != NULL ? pet->current_skin : NULL);

    /* Show the bundled pet image until the AI image service can provide one. */
    velapet_ui_pet_image_set_state(UI_PET_IMAGE_READY, VELAPET_UI_DEFAULT_ASSET);

    ui_router_show_home();
    return 0;
}

void velapet_ui_show_home(void) { ui_router_show_home(); }
void velapet_ui_show_task(void) { ui_router_show_task(); }
void velapet_ui_show_chat(void) { ui_router_show_chat(); }
void velapet_ui_show_pet_image(void) { ui_router_show_pet_image(); }
void velapet_ui_show_skin(void) { ui_router_show_skin(); }

void velapet_ui_refresh_all(void)
{
    switch (g_ui_page) {
    case UI_PAGE_HOME:
        ui_home_refresh();
        break;
    case UI_PAGE_TASK:
        ui_task_refresh();
        break;
    case UI_PAGE_CHAT:
        ui_chat_refresh();
        break;
    case UI_PAGE_PET_IMAGE:
        ui_pet_image_refresh();
        break;
    case UI_PAGE_SKIN:
        ui_skin_refresh();
        break;
    default:
        break;
    }
}
