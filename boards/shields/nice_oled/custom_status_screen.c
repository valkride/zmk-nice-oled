#include "widgets/screen.h"
#include "widgets/screen_peripheral.h"

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include "assets/pixel_operator_mono.c"
#include "assets/pixel_operator_mono_12.c"
#include "assets/pixel_operator_mono_8.c"

// auto save conflict
#include "assets/custom_fonts.h"

#if IS_ENABLED(CONFIG_NICE_VIEW_WIDGET_STATUS)
static struct zmk_widget_screen screen_widget;
static struct zmk_widget_screen_peripheral screen_peripheral_widget;
#endif

lv_obj_t *zmk_display_status_screen() {
    lv_obj_t *screen;
    screen = lv_obj_create(NULL);

#if IS_ENABLED(CONFIG_NICE_VIEW_WIDGET_STATUS)
    #if !IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    // Main/Central keyboard: show only the simple activity bar (Bluetooth icon + battery)
    zmk_widget_screen_init(&screen_widget, screen);
    lv_obj_align(zmk_widget_screen_obj(&screen_widget), LV_ALIGN_TOP_LEFT, 0, 0);
    #else
    // Peripheral keyboard: show all meters (WPM, layer, profile)
    zmk_widget_screen_peripheral_init(&screen_peripheral_widget, screen);
    lv_obj_align(zmk_widget_screen_peripheral_obj(&screen_peripheral_widget), LV_ALIGN_TOP_LEFT, 0, 0);
    #endif
#endif

    return screen;
}
