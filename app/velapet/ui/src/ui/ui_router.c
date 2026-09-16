#include "ui_internal.h"

static void route_to(UiPage page)
{
    if (g_ui_root != NULL) {
        switch (g_ui_page) {
        case UI_PAGE_HOME: ui_home_on_destroy(); break;
        case UI_PAGE_CHAT: ui_chat_on_destroy(); break;
        case UI_PAGE_PET_IMAGE: ui_pet_image_on_destroy(); break;
        default: break;
        }
        lv_obj_del(g_ui_root);
        g_ui_root = NULL;
    }

    g_ui_page = page;

    switch (page) {
    case UI_PAGE_HOME:
        ui_home_create();
        break;
    case UI_PAGE_TASK:
        ui_task_create();
        break;
    case UI_PAGE_CHAT:
        ui_chat_create();
        break;
    case UI_PAGE_PET_IMAGE:
        ui_pet_image_create();
        break;
    case UI_PAGE_SKIN:
        ui_skin_create();
        break;
    default:
        ui_home_create();
        break;
    }
}

void ui_router_show_home(void) { route_to(UI_PAGE_HOME); }
void ui_router_show_task(void) { route_to(UI_PAGE_TASK); }
void ui_router_show_chat(void) { route_to(UI_PAGE_CHAT); }
void ui_router_show_pet_image(void) { route_to(UI_PAGE_PET_IMAGE); }
void ui_router_show_skin(void) { route_to(UI_PAGE_SKIN); }
