#pragma once

#include <gui/view.h>

/* Saved-networks list: Wi-Fi icon, name, lock icon for secured networks. */

typedef struct WfrListView WfrListView;
typedef void (*WfrListViewCallback)(void* context, uint32_t index);

#define WFR_LIST_VIEW_MAX_ITEMS 20

WfrListView* wfr_list_view_alloc(void);
void wfr_list_view_free(WfrListView* list);
View* wfr_list_view_get_view(WfrListView* list);

void wfr_list_view_reset(WfrListView* list, const char* title);
/* name must stay valid while the view is shown */
void wfr_list_view_add_item(WfrListView* list, const char* name, bool locked);
void wfr_list_view_set_selected(WfrListView* list, uint32_t index);
void wfr_list_view_set_callback(WfrListView* list, WfrListViewCallback callback, void* context);
