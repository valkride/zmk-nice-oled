#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/wpm_state_changed.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/events/ble_active_profile_changed.h>

#include "wpm.h"
#include "layer.h"
#include "profile.h"
#include "output.h"
#include "screen_peripheral.h"
#include "split_sync.h"
#include "../assets/custom_fonts.h" // Add this line to declare pixel_operator_mono_16

// Fallback macros for event helpers only
#ifndef as_zmk_wpm_state_changed
#define as_zmk_wpm_state_changed(eh) ((const struct zmk_wpm_state_changed *)((eh) ? (eh) : NULL))
#endif
#ifndef as_zmk_layer_state_changed
#define as_zmk_layer_state_changed(eh) ((const struct zmk_layer_state_changed *)((eh) ? (eh) : NULL))
#endif
#ifndef as_zmk_ble_active_profile_changed
#define as_zmk_ble_active_profile_changed(eh) ((const struct zmk_ble_active_profile_changed *)((eh) ? (eh) : NULL))
#endif
#ifndef zmk_event_zmk_wpm_state_changed
#define zmk_event_zmk_wpm_state_changed (*(const struct zmk_event_type *)0)
#endif
#ifndef zmk_event_zmk_layer_state_changed
#define zmk_event_zmk_layer_state_changed (*(const struct zmk_event_type *)0)
#endif
#ifndef zmk_event_zmk_ble_active_profile_changed
#define zmk_event_zmk_ble_active_profile_changed (*(const struct zmk_event_type *)0)
#endif
// If ZMK_SUBSCRIPTION is not defined, stub it as a no-op
#ifndef ZMK_SUBSCRIPTION
#define ZMK_SUBSCRIPTION(...)
#endif

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

/**
 * Draw canvas
 **/
static void draw_canvas(lv_obj_t *widget, lv_color_t cbuf[], const struct status_state *state) {
    lv_obj_t *canvas = lv_obj_get_child(widget, 0);
    // Clear the canvas to background color before drawing meters
    lv_canvas_fill_bg(canvas, LVGL_BACKGROUND, LV_OPA_COVER);
    // Only draw meters/info on the peripheral
    draw_wpm_status(canvas, state);
    draw_layer_status(canvas, state);
    draw_profile_status(canvas, state);
    draw_output_status(canvas, state);

    // Rotate for horizontal display
    rotate_canvas(canvas, cbuf);
}

/**
 * WPM status
 **/
static void set_wpm_status(struct zmk_widget_screen_peripheral *widget, struct wpm_status_state state) {
#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_PERIPHERAL)
    widget->state.wpm[9] = split_sync_get_wpm();
#else
    widget->state.wpm[9] = state.wpm;
#endif
    draw_canvas(widget->obj, widget->cbuf, &widget->state);
}

static void wpm_status_update_cb(struct wpm_status_state state) {
    struct zmk_widget_screen_peripheral *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_wpm_status(widget, state); }
}

static struct wpm_status_state wpm_status_get_state(const zmk_event_t *eh) {
    const struct zmk_wpm_state_changed *ev = as_zmk_wpm_state_changed(eh);
    return (struct wpm_status_state){.wpm = (ev != NULL) ? ev->state : 0};
};

ZMK_DISPLAY_WIDGET_LISTENER(widget_wpm_status, struct wpm_status_state, wpm_status_update_cb, wpm_status_get_state)
ZMK_SUBSCRIPTION(widget_wpm_status, zmk_wpm_state_changed);

/**
 * Layer status
 **/
static void set_layer_status(struct zmk_widget_screen_peripheral *widget, struct layer_status_state state) {
#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_PERIPHERAL)
    widget->state.layer_index = split_sync_get_layer();
    widget->state.layer_label = NULL;
#else
    widget->state.layer_index = state.index;
    widget->state.layer_label = state.label;
#endif
    draw_canvas(widget->obj, widget->cbuf, &widget->state);
}

static void layer_status_update_cb(struct layer_status_state state) {
    struct zmk_widget_screen_peripheral *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_layer_status(widget, state); }
}

static struct layer_status_state layer_status_get_state(const zmk_event_t *eh) {
    const struct zmk_layer_state_changed *ev = as_zmk_layer_state_changed(eh);
    return (struct layer_status_state){.index = (ev != NULL) ? ev->layer : 0, .label = NULL};
};

ZMK_DISPLAY_WIDGET_LISTENER(widget_layer_status, struct layer_status_state, layer_status_update_cb, layer_status_get_state)
ZMK_SUBSCRIPTION(widget_layer_status, zmk_layer_state_changed);

/**
 * Profile status
 **/
static void set_profile_status(struct zmk_widget_screen_peripheral *widget, struct profile_status_state state) {
#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_PERIPHERAL)
    widget->state.active_profile_index = split_sync_get_profile();
#else
    widget->state.active_profile_index = state.profile;
#endif
    draw_canvas(widget->obj, widget->cbuf, &widget->state);
}

static void profile_status_update_cb(struct profile_status_state state) {
    struct zmk_widget_screen_peripheral *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_profile_status(widget, state); }
}

static struct profile_status_state profile_status_get_state(const zmk_event_t *eh) {
    const struct zmk_ble_active_profile_changed *ev = as_zmk_ble_active_profile_changed(eh);
    return (struct profile_status_state){.profile = (ev != NULL) ? ev->index : 0};
};

ZMK_DISPLAY_WIDGET_LISTENER(widget_profile_status, struct profile_status_state, profile_status_update_cb, profile_status_get_state)
ZMK_SUBSCRIPTION(widget_profile_status, zmk_ble_active_profile_changed);

/**
 * Initialization
 **/
int zmk_widget_screen_peripheral_init(struct zmk_widget_screen_peripheral *widget, lv_obj_t *parent) {
    LOG_INF("zmk_widget_screen_peripheral_init called");
    widget->obj = lv_obj_create(parent);
    lv_obj_set_size(widget->obj, CANVAS_HEIGHT, CANVAS_WIDTH);

    lv_obj_t *canvas = lv_canvas_create(widget->obj);
    lv_obj_align(canvas, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_canvas_set_buffer(canvas, widget->cbuf, CANVAS_HEIGHT, CANVAS_HEIGHT, LV_IMG_CF_TRUE_COLOR);

    sys_slist_append(&widgets, &widget->node);

    widget_wpm_status_init();
    widget_layer_status_init();
    widget_profile_status_init();

    return 0;
}

lv_obj_t *zmk_widget_screen_peripheral_obj(struct zmk_widget_screen_peripheral *widget) { return widget->obj; }
