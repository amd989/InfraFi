#pragma once

#include <gui/view.h>

/* Carousel main menu: one big icon tile at a time, Left/Right to browse. */

typedef struct WfrMenuView WfrMenuView;
typedef void (*WfrMenuViewCallback)(void* context, uint32_t index);

#define WFR_MENU_VIEW_MAX_ITEMS 6

WfrMenuView* wfr_menu_view_alloc(void);
void wfr_menu_view_free(WfrMenuView* menu);
View* wfr_menu_view_get_view(WfrMenuView* menu);

void wfr_menu_view_reset(WfrMenuView* menu);
void wfr_menu_view_add_item(WfrMenuView* menu, const char* label, const Icon* icon);
void wfr_menu_view_set_selected(WfrMenuView* menu, uint32_t index);
void wfr_menu_view_set_callback(WfrMenuView* menu, WfrMenuViewCallback callback, void* context);
