#include "ui_main.h"
#include "ui_settings.h"
#include "ble_actions.h"
#include "pt/pt_display.h"
#include <LittleFS.h>

lv_obj_t* g_main_screen = nullptr;
lv_obj_t* g_wifi_label = nullptr;
volatile bool g_pending_ui_update = false;
volatile bool g_ota_screen_requested = false;
volatile int g_ota_progress = -1;

static lv_obj_t* g_update_screen = nullptr;
static lv_obj_t* g_update_bar = nullptr;
static lv_obj_t* g_update_label = nullptr;
static lv_obj_t* g_update_pct_label = nullptr;

static lv_obj_t* g_btns[MAX_BUTTONS] = {nullptr};
static lv_obj_t* g_btn_labels[MAX_BUTTONS] = {nullptr};
static lv_obj_t* g_btn_icons[MAX_BUTTONS] = {nullptr};
static lv_obj_t* g_grid = nullptr;
static lv_obj_t* g_slider = nullptr;
static lv_obj_t* g_settings_btn = nullptr;

static void btn_event_cb(lv_event_t* e) {
    uint8_t local_idx = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    uint8_t global_idx = (g_current_page * BUTTONS_PER_PAGE) + local_idx;
    handle_button_action(global_idx);
}

static void page_nav_cb(lv_event_t* e) {
    int dir = (int)(uintptr_t)lv_event_get_user_data(e);
    if (dir > 0) {
        g_current_page = (g_current_page + 1) % g_num_pages;
    } else {
        g_current_page = (g_current_page == 0) ? g_num_pages - 1 : g_current_page - 1;
    }
    save_settings(false);
    create_main_ui();
}

static void slider_event_cb(lv_event_t* e) {
    lv_obj_t* slider = (lv_obj_t*)lv_event_get_target(e);
    g_brightness = (uint8_t)lv_slider_get_value(slider);
    bool released = (lv_event_get_code(e) == LV_EVENT_RELEASED);
    // Apply backlight on every tick for smooth real-time feedback.
    // Save pt_backlight_percent and NVS only on release to avoid blocking
    // the render loop with repeated flash writes during drag.
    pt_set_backlight(g_brightness, released);
    if (released) save_settings(false);
}

static void settings_btn_cb(lv_event_t* e) {
    create_settings_ui();
}

void create_main_ui() {
    lv_obj_clean(g_main_screen);
    lv_obj_set_style_bg_color(g_main_screen, lv_color_hex(g_bg_color), LV_PART_MAIN);
    lv_obj_clear_flag(g_main_screen, LV_OBJ_FLAG_SCROLLABLE);

    memset(g_btns, 0, sizeof(g_btns));
    memset(g_btn_labels, 0, sizeof(g_btn_labels));
    memset(g_btn_icons, 0, sizeof(g_btn_icons));
    g_grid = nullptr;
    g_slider = nullptr;
    g_settings_btn = nullptr;

    static int32_t col_dsc[10];
    static int32_t row_dsc[10];

    if (g_rows < 1 || g_rows > 5) g_rows = 3;
    if (g_cols < 1 || g_cols > 5) g_cols = 3;

    int32_t availW = 800 - 20 - (g_cols - 1) * 10;
    int32_t availH = 480 - 20 - 60 - g_rows * 10;

    int32_t cellW = availW / g_cols;
    int32_t cellH = availH / g_rows;

    for (int i = 0; i < g_cols; i++) col_dsc[i] = cellW;
    col_dsc[g_cols] = LV_GRID_TEMPLATE_LAST;

    for (int i = 0; i < g_rows; i++) row_dsc[i] = cellH;
    row_dsc[g_rows] = 60;
    row_dsc[g_rows + 1] = LV_GRID_TEMPLATE_LAST;

    g_grid = lv_obj_create(g_main_screen);
    lv_obj_set_grid_dsc_array(g_grid, col_dsc, row_dsc);
    lv_obj_set_size(g_grid, lv_pct(100), lv_pct(100));
    lv_obj_center(g_grid);
    lv_obj_set_style_bg_color(g_grid, lv_color_hex(g_bg_color), LV_PART_MAIN);
    lv_obj_set_style_border_width(g_grid, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(g_grid, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_gap(g_grid, 10, LV_PART_MAIN);
    lv_obj_clear_flag(g_grid, LV_OBJ_FLAG_SCROLLABLE);

    int btn_count = g_rows * g_cols;
    uint8_t page_offset = g_current_page * BUTTONS_PER_PAGE;

    for (int i = 0; i < btn_count; i++) {
        uint8_t global_idx = page_offset + i;
        lv_obj_t* btn = lv_btn_create(g_grid);
        lv_obj_set_grid_cell(btn, LV_GRID_ALIGN_STRETCH, i % g_cols, 1, LV_GRID_ALIGN_STRETCH, i / g_cols, 1);
        lv_obj_set_style_bg_color(btn, lv_color_hex(g_configs[global_idx].color), LV_PART_MAIN);

        lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_row(btn, 5, 0);

        g_btns[i] = btn;
        lv_obj_set_style_bg_opa(btn, 255, LV_PART_MAIN);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x000000), LV_STATE_PRESSED);
        lv_obj_set_style_bg_opa(btn, 100, LV_STATE_PRESSED);

        bool icon_or_img_present = false;
        if (g_configs[global_idx].imgPath[0] != '\0') {
            String fpath = g_configs[global_idx].imgPath;
            if (!fpath.startsWith("/")) fpath = "/" + fpath;

            if (LittleFS.exists(fpath)) {
                lv_obj_t* img = lv_image_create(btn);
                char full_path[64];
                sprintf(full_path, "L:%s", fpath.c_str());
                lv_image_set_src(img, full_path);

                if (g_cols > 4 || g_rows > 3) lv_obj_set_size(img, 48, 48);
                else lv_obj_set_size(img, 64, 64);

                icon_or_img_present = true;
            }
        }

        if (!icon_or_img_present && g_configs[global_idx].icon[0] != '\0') {
            lv_obj_t* icon = lv_label_create(btn);
            lv_label_set_text(icon, g_configs[global_idx].icon);
            if (g_cols > 4) lv_obj_set_style_text_font(icon, &lv_font_montserrat_18, 0);
            else lv_obj_set_style_text_font(icon, &lv_font_montserrat_24, 0);
            g_btn_icons[i] = icon;
            icon_or_img_present = true;
        }

        if (g_configs[global_idx].label[0] != '\0') {
            lv_obj_t* label = lv_label_create(btn);
            lv_label_set_text(label, g_configs[global_idx].label);
            if (g_cols > 4) lv_obj_set_style_text_font(label, &lv_font_montserrat_12, 0);
            else lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
            g_btn_labels[i] = label;
        }

        if (g_configs[global_idx].type == BTN_TYPE_DISABLED) {
            lv_obj_add_state(btn, LV_STATE_DISABLED);
        } else {
            lv_obj_add_event_cb(btn, btn_event_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)i);
        }
    }

    // Bottom Navigation Bar
    lv_obj_t* nav_cont = lv_obj_create(g_grid);
    lv_obj_set_grid_cell(nav_cont, LV_GRID_ALIGN_STRETCH, 0, g_cols, LV_GRID_ALIGN_STRETCH, g_rows, 1);
    lv_obj_set_style_bg_opa(nav_cont, 0, 0);
    lv_obj_set_style_border_width(nav_cont, 0, 0);
    lv_obj_set_style_pad_all(nav_cont, 0, 0);
    lv_obj_clear_flag(nav_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(nav_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(nav_cont, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Brightness Slider
    g_slider = lv_slider_create(nav_cont);
    lv_obj_set_size(g_slider, 200, 20);
    lv_slider_set_range(g_slider, 10, 100);
    lv_slider_set_value(g_slider, g_brightness, LV_ANIM_OFF);
    // Prevent the vertical component of a slider drag from propagating as a
    // scroll gesture to parent containers (g_grid, g_main_screen).
    lv_obj_clear_flag(g_slider, LV_OBJ_FLAG_SCROLL_CHAIN_VER);
    lv_obj_add_event_cb(g_slider, slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(g_slider, slider_event_cb, LV_EVENT_RELEASED, NULL);

    // Page Indicator and Nav (hidden when only one page is configured)
    if (g_num_pages > 1) {
        lv_obj_t* page_box = lv_obj_create(nav_cont);
        lv_obj_set_size(page_box, 300, 50);
        lv_obj_set_style_bg_opa(page_box, 0, 0);
        lv_obj_set_style_border_width(page_box, 0, 0);
        lv_obj_clear_flag(page_box, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_flex_flow(page_box, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(page_box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        lv_obj_t* prev_btn = lv_btn_create(page_box);
        lv_obj_set_size(prev_btn, 110, 45);
        lv_obj_t* prev_lbl = lv_label_create(prev_btn);
        lv_label_set_text(prev_lbl, LV_SYMBOL_LEFT);
        lv_obj_center(prev_lbl);
        lv_obj_add_event_cb(prev_btn, page_nav_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)-1);

        // lv_obj_t* page_lbl = lv_label_create(page_box);
        // lv_label_set_text_fmt(page_lbl, "Page %d / %d", g_current_page + 1, g_num_pages);
        // lv_obj_set_style_margin_left(page_lbl, 15, 0);
        // lv_obj_set_style_margin_right(page_lbl, 15, 0);

        lv_obj_t* next_btn = lv_btn_create(page_box);
        lv_obj_set_size(next_btn, 110, 45);
        lv_obj_t* next_lbl = lv_label_create(next_btn);
        lv_label_set_text(next_lbl, LV_SYMBOL_RIGHT);
        lv_obj_center(next_lbl);
        lv_obj_add_event_cb(next_btn, page_nav_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)1);
    }

    // IP and Config
    lv_obj_t* right_box = lv_obj_create(nav_cont);
    lv_obj_set_size(right_box, 200, 50);
    lv_obj_set_style_bg_opa(right_box, 0, 0);
    lv_obj_set_style_border_width(right_box, 0, 0);
    lv_obj_set_style_pad_all(right_box, 0, 0);
    lv_obj_clear_flag(right_box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(right_box, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(right_box, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // WiFi Status / IP Address
    g_wifi_label = lv_label_create(right_box);
    String wtxt = "\xEF\x87\xAB " + g_ip_addr;
    lv_label_set_text(g_wifi_label, wtxt.c_str());
    lv_obj_set_style_text_color(g_wifi_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_margin_right(g_wifi_label, 20, 0);

    // Settings Button
    g_settings_btn = lv_btn_create(right_box);
    lv_obj_set_size(g_settings_btn, 45, 45);
    lv_obj_t* set_label = lv_label_create(g_settings_btn);
    lv_label_set_text(set_label, "\xEF\x80\x93");
    lv_obj_center(set_label);
    lv_obj_add_event_cb(g_settings_btn, settings_btn_cb, LV_EVENT_CLICKED, NULL);
}

void refresh_main_ui() {
    if (!g_grid) return;

    lv_obj_set_style_bg_color(g_main_screen, lv_color_hex(g_bg_color), LV_PART_MAIN);
    lv_obj_set_style_bg_color(g_grid, lv_color_hex(g_bg_color), LV_PART_MAIN);

    int btn_count = g_rows * g_cols;
    uint8_t page_offset = g_current_page * BUTTONS_PER_PAGE;
    for (int i = 0; i < btn_count; i++) {
        if (!g_btns[i]) continue;
        uint8_t global_idx = page_offset + i;
        lv_obj_set_style_bg_color(g_btns[i], lv_color_hex(g_configs[global_idx].color), LV_PART_MAIN);
        if (g_btn_labels[i]) lv_label_set_text(g_btn_labels[i], g_configs[global_idx].label);
        if (g_btn_icons[i]) lv_label_set_text(g_btn_icons[i], g_configs[global_idx].icon);
    }

    String wtxt = "\xEF\x87\xAB " + g_ip_addr;
    lv_label_set_text(g_wifi_label, wtxt.c_str());
}

void update_ota_progress(int pct, const char* msg) {
    if (!g_update_screen) return;

    if (msg && g_update_label) {
        lv_label_set_text(g_update_label, msg);
    }

    if (g_update_bar) {
        if (pct >= 0) {
            lv_bar_set_value(g_update_bar, pct, LV_ANIM_ON);
            if (g_update_pct_label) {
                lv_label_set_text_fmt(g_update_pct_label, "%d%%", pct);
            }
        }
    }

    lv_timer_handler();
}

void show_update_screen() {
    if (g_update_screen) {
        lv_scr_load(g_update_screen);
        return;
    }

    g_update_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(g_update_screen, lv_color_hex(0x1a1a1a), LV_PART_MAIN);

    lv_obj_t* cont = lv_obj_create(g_update_screen);
    lv_obj_set_size(cont, 400, 220);
    lv_obj_center(cont);
    lv_obj_set_style_bg_color(cont, lv_color_hex(0x2a2a2a), LV_PART_MAIN);
    lv_obj_set_style_border_color(cont, lv_color_hex(0xffaa00), LV_PART_MAIN);
    lv_obj_set_style_border_width(cont, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(cont, 10, LV_PART_MAIN);

    g_update_label = lv_label_create(cont);
    lv_label_set_text(g_update_label, "Updating System...");
    lv_obj_set_style_text_color(g_update_label, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_set_style_text_align(g_update_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(g_update_label, LV_ALIGN_TOP_MID, 0, 20);

    g_update_bar = lv_bar_create(cont);
    lv_obj_set_size(g_update_bar, 300, 20);
    lv_obj_align(g_update_bar, LV_ALIGN_CENTER, 0, 10);
    lv_bar_set_range(g_update_bar, 0, 100);
    lv_bar_set_value(g_update_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(g_update_bar, lv_color_hex(0x444444), LV_PART_MAIN);
    lv_obj_set_style_bg_color(g_update_bar, lv_color_hex(0xffaa00), LV_PART_INDICATOR);

    g_update_pct_label = lv_label_create(cont);
    lv_label_set_text(g_update_pct_label, "0%");
    lv_obj_set_style_text_font(g_update_pct_label, &lv_font_montserrat_14, 0);
    lv_obj_align(g_update_pct_label, LV_ALIGN_CENTER, 0, 35);

    lv_obj_t* spinner = lv_spinner_create(cont);
    lv_obj_set_size(spinner, 30, 30);
    lv_obj_align(spinner, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_arc_color(spinner, lv_color_hex(0xffaa00), LV_PART_INDICATOR);

    lv_scr_load(g_update_screen);
    lv_timer_handler();
}
