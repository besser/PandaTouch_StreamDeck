#pragma once
#ifndef STREAMDECK_H
#define STREAMDECK_H

#include <Arduino.h>
#include <lvgl.h>
#include "constants.h"

// Sleep overlay — transparent clickable LVGL object parented to g_main_screen.
// Null when awake; non-null while display is off. All modules that call
// create_main_ui() must zero this before the call (lv_obj_clean destroys the
// overlay as a child but does not zero the pointer automatically).
extern lv_obj_t* g_sleep_overlay;

class StreamDeckApp {
public:
    static void setup();
    static void loop();
    static void handle_button(uint8_t action_id);
    static void log(const char* fmt, ...);
};

#endif
