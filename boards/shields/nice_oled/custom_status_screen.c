#include "widgets/screen.h"
#include "widgets/screen_peripheral.h"

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include "assets/pixel_operator_mono.c"
#include "assets/pixel_operator_mono_12.c"
#include "assets/pixel_operator_mono_8.c"

// auto save conflict
#include "assets/custom_fonts.h"

// Always declare the widgets to avoid compilation issues
static struct zmk_widget_screen screen_widget;
static struct zmk_widget_screen_peripheral screen_peripheral_widget;

lv_obj_t *zmk_display_status_screen() {
    lv_obj_t *screen;
    screen = lv_obj_create(NULL);

    LOG_INF("Creating status screen...");

#if IS_ENABLED(CONFIG_NICE_VIEW_WIDGET_STATUS)
    LOG_INF("CONFIG_NICE_VIEW_WIDGET_STATUS is enabled");
    
    #if !IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    LOG_INF("Initializing main screen widget");
    // Main/Central keyboard: show only the simple activity bar (Bluetooth icon + battery)
    zmk_widget_screen_init(&screen_widget, screen);
    lv_obj_align(zmk_widget_screen_obj(&screen_widget), LV_ALIGN_TOP_LEFT, 0, 0);
    #else
    LOG_INF("Initializing peripheral screen widget");
    // Peripheral keyboard: show all meters (WPM, layer, profile)
    zmk_widget_screen_peripheral_init(&screen_peripheral_widget, screen);
    lv_obj_align(zmk_widget_screen_peripheral_obj(&screen_peripheral_widget), LV_ALIGN_TOP_LEFT, 0, 0);
    #endif
#else
    LOG_INF("CONFIG_NICE_VIEW_WIDGET_STATUS is NOT enabled - creating both widgets as fallback");
    // Fallback: if config is not enabled, create main widget
    zmk_widget_screen_init(&screen_widget, screen);
    lv_obj_align(zmk_widget_screen_obj(&screen_widget), LV_ALIGN_TOP_LEFT, 0, 0);
#endif

    LOG_INF("Status screen creation completed");
    return screen;
}
