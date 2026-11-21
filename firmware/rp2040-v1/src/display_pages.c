// =============================================================================
//  display_pages.c  -  LVGL UI pages implementation
// =============================================================================

#include "display_pages.h"
#include "config.h"
#include <stdio.h>
#include <string.h>
#include "hr2046.h"

// ---------------------------------------------------------------------------
//  Color palette (dark theme)
// ---------------------------------------------------------------------------
#define CLR_BG          lv_color_hex(0x0A0A0A)
#define CLR_PANEL       lv_color_hex(0x1A1A2E)
#define CLR_ACCENT      lv_color_hex(0x00D4FF)
#define CLR_GREEN       lv_color_hex(0x00FF88)
#define CLR_RED         lv_color_hex(0xFF3333)
#define CLR_YELLOW      lv_color_hex(0xFFD700)
#define CLR_ORANGE      lv_color_hex(0xFF8C00)
#define CLR_WHITE       lv_color_hex(0xFFFFFF)
#define CLR_GREY        lv_color_hex(0x888888)
#define CLR_BTN_ON      lv_color_hex(0x00AA55)
#define CLR_BTN_OFF     lv_color_hex(0x333333)

// ---------------------------------------------------------------------------
//  Autopilot parameter selection (encoder controls this)
// ---------------------------------------------------------------------------
typedef enum {
    AP_PARAM_HEADING  = 0,
    AP_PARAM_ALTITUDE = 1,
    AP_PARAM_VSPEED   = 2,
    AP_PARAM_ETRIM    = 3,
    AP_PARAM_COUNT    = 4
} ap_param_t;

// ---------------------------------------------------------------------------
//  Internal state
// ---------------------------------------------------------------------------
static lv_obj_t *s_page_flight;
static lv_obj_t *s_page_ap;
static lv_obj_t *s_page_nav;

static uint8_t s_current_page = 0;

// --- Flight page labels ---
static lv_obj_t *s_lbl_speed;
static lv_obj_t *s_lbl_altitude;
static lv_obj_t *s_lbl_vspeed;
static lv_obj_t *s_lbl_heading;
static lv_obj_t *s_lbl_throttle_val;
static lv_obj_t *s_lbl_mixture_val;
static lv_obj_t *s_bar_throttle;
static lv_obj_t *s_bar_mixture;
static lv_obj_t *rud_pointer;
static lv_obj_t *s_lbl_gear;
static lv_obj_t *s_lbl_flaps;
static lv_obj_t *s_lbl_ap_status;
static lv_obj_t *coords;

// --- Autopilot page ---
static lv_obj_t *s_lbl_ap_hdg_val;
static lv_obj_t *s_lbl_ap_alt_val;
static lv_obj_t *s_lbl_ap_vs_val;
static lv_obj_t *s_lbl_elev_trim;
static lv_obj_t *s_btn_hdg;
static lv_obj_t *s_btn_alt;
static lv_obj_t *s_btn_vs;
static lv_obj_t *s_btn_nav;
static lv_obj_t *s_btn_ap_master;

static uint32_t s_ap_user_edit_lock_ms = 0;

static lv_obj_t *s_param_boxes[AP_PARAM_COUNT]; // highlight boxes

// --- AP settings (written here, read by Core0) ---
static ap_settings_t s_ap_local;
static bool          s_ap_changed = false;
static ap_param_t    s_ap_selected = AP_PARAM_COUNT;

// ---------------------------------------------------------------------------
//  Helper: create a labelled value box inside a parent container
//  Returns the value label so the caller can update it.
// ---------------------------------------------------------------------------
static lv_obj_t *create_data_box(lv_obj_t *parent,
                                  const char *title,
                                  const char *unit,
                                  int col, int row)
{
    // Outer box
    lv_obj_t *box = lv_obj_create(parent);
    lv_obj_set_size(box, 148, 90);
    lv_obj_set_pos(box, 8 + col * 158, 8 + row * 98);
    lv_obj_set_style_bg_color(box, CLR_PANEL, 0);
    lv_obj_set_style_border_color(box, CLR_ACCENT, 0);
    lv_obj_set_style_border_width(box, 1, 0);
    lv_obj_set_style_radius(box, 6, 0);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);

    // Title label (small, top-left)
    lv_obj_t *lbl_title = lv_label_create(box);
    lv_label_set_text(lbl_title, title);
    lv_obj_set_style_text_color(lbl_title, CLR_GREY, 0);
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_12, 0);
    lv_obj_align(lbl_title, LV_ALIGN_TOP_LEFT, 4, 4);

    // Value label (large, centre)
    lv_obj_t *lbl_val = lv_label_create(box);
    lv_label_set_text(lbl_val, "---");
    lv_obj_set_style_text_color(lbl_val, CLR_WHITE, 0);
    lv_obj_set_style_text_font(lbl_val, &lv_font_montserrat_32, 0);
    lv_obj_align(lbl_val, LV_ALIGN_CENTER, -15, 6);

    // Unit label (small, bottom-right)
    lv_obj_t *lbl_unit = lv_label_create(box);
    lv_label_set_text(lbl_unit, unit);
    lv_obj_set_style_text_color(lbl_unit, CLR_GREY, 0);
    lv_obj_set_style_text_font(lbl_unit, &lv_font_montserrat_12, 0);
    lv_obj_align(lbl_unit, LV_ALIGN_BOTTOM_RIGHT, 0, -4);

    return lbl_val;
}

// ---------------------------------------------------------------------------
//  Helper: create a bar row (title + bar + value label)
// ---------------------------------------------------------------------------
static void create_bar_row(lv_obj_t *parent,
                            const char *title,
                            lv_obj_t **bar_out,
                            lv_obj_t **lbl_out,
                            int y_pos,
                            lv_color_t bar_color)
{
    lv_obj_t *lbl_t = lv_label_create(parent);
    lv_label_set_text(lbl_t, title);
    lv_obj_set_style_text_color(lbl_t, CLR_GREY, 0);
    lv_obj_set_style_text_font(lbl_t, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(lbl_t, 40, y_pos);

    lv_obj_t *bar = lv_bar_create(parent);
    lv_obj_set_size(bar, 210, 18);
    lv_obj_set_pos(bar, 40, y_pos + 16);
    lv_bar_set_range(bar, 0, 100);
    lv_obj_set_style_bg_color(bar, CLR_BTN_OFF, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, bar_color, LV_PART_INDICATOR);
    *bar_out = bar;

    lv_obj_t *lbl_v = lv_label_create(parent);
    lv_label_set_text(lbl_v, "0%");
    lv_obj_set_style_text_color(lbl_v, CLR_WHITE, 0);
    lv_obj_set_style_text_font(lbl_v, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(lbl_v, 255, y_pos + 16);
    *lbl_out = lbl_v;
}

// ---------------------------------------------------------------------------
//  AP mode button callback
// ---------------------------------------------------------------------------
static void ap_btn_cb(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);

    if (btn == s_btn_hdg) {
        s_ap_local.hdg_mode = !s_ap_local.hdg_mode;
        s_ap_local.btnpressed= 1;
    } else if (btn == s_btn_alt) {
        s_ap_local.alt_mode = !s_ap_local.alt_mode;
        s_ap_local.btnpressed= 2;
    } else if (btn == s_btn_vs) {
        s_ap_local.vs_mode = !s_ap_local.vs_mode;
        s_ap_local.btnpressed= 3;
        if(s_ap_local.vs_mode==1) s_ap_local.target_vspeed= ((int)local_fd.vspeed_fpm / 100) * 100;
    } else if (btn == s_btn_nav) {
        s_ap_local.nav_mode = !s_ap_local.nav_mode;
        s_ap_local.btnpressed= 4;
    } else if (btn == s_btn_ap_master) {
        s_ap_local.ap_toggle = true;
        s_ap_local.btnpressed= 5;
    }
    s_ap_changed = true;

    // Update button colour to reflect mode state
    lv_color_t c_hdg = s_ap_local.hdg_mode ? CLR_BTN_ON : CLR_BTN_OFF;
    lv_color_t c_alt = s_ap_local.alt_mode ? CLR_BTN_ON : CLR_BTN_OFF;
    lv_color_t c_vs  = s_ap_local.vs_mode  ? CLR_BTN_ON : CLR_BTN_OFF;
    lv_color_t c_nav = s_ap_local.nav_mode ? CLR_BTN_ON : CLR_BTN_OFF;
    lv_obj_set_style_bg_color(s_btn_hdg, c_hdg, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(s_btn_alt, c_alt, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(s_btn_vs,  c_vs,  LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(s_btn_nav, c_nav, LV_STATE_DEFAULT);

    s_ap_user_edit_lock_ms = lv_tick_get() + 1000;
}


// ---------------------------------------------------------------------------
//  Helper: create one AP mode button
// ---------------------------------------------------------------------------
static lv_obj_t *create_ap_btn(lv_obj_t *parent,
                                const char *label,
                                int x, int y)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, 130, 70);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_style_bg_color(btn, CLR_BTN_OFF, LV_STATE_DEFAULT);
    lv_obj_set_style_radius(btn, 8, 0);
    lv_obj_add_event_cb(btn, ap_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, label);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl, CLR_WHITE, 0);
    lv_obj_center(lbl);

    return btn;
}

// ---------------------------------------------------------------------------
//  Helper: AP parameter row  (title + value + selection highlight)
// ---------------------------------------------------------------------------
static void create_ap_param_row(lv_obj_t *parent,
                                 const char *title,
                                 lv_obj_t **lbl_val_out,
                                 lv_obj_t **box_out,
                                 int y)
{
    lv_obj_t *box = lv_obj_create(parent);
    lv_obj_set_size(box, 162, 70);
    lv_obj_set_pos(box, 150, y);
    lv_obj_set_style_bg_color(box, CLR_PANEL, 0);
    lv_obj_set_style_border_color(box, CLR_GREY, 0);
    lv_obj_set_style_border_width(box, 1, 0);
    lv_obj_set_style_radius(box, 6, 0);
    lv_obj_add_flag(box, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    *box_out = box;

    lv_obj_t *lbl_v = lv_label_create(box);
    lv_label_set_text(lbl_v, "---");
    lv_obj_set_style_text_color(lbl_v, CLR_WHITE, 0);
    lv_obj_set_style_text_font(lbl_v, &lv_font_montserrat_24, 0);
    lv_obj_align(lbl_v, LV_ALIGN_RIGHT_MID, -8, 0);
    *lbl_val_out = lbl_v;
}

// ---------------------------------------------------------------------------
//  Highlight the currently selected encoder parameter
// ---------------------------------------------------------------------------
static void refresh_ap_selection(void)
{
    for (int i = 0; i < AP_PARAM_COUNT; i++) {
        lv_color_t border = (i == (int)s_ap_selected) ? CLR_ACCENT : CLR_GREY;
        int        width  = (i == (int)s_ap_selected) ? 2 : 1;
        lv_obj_set_style_border_color(s_param_boxes[i], border, 0);
        lv_obj_set_style_border_width(s_param_boxes[i], width,  0);
    }
}

static void ap_row_click_event_cb(lv_event_t *e)
{
    int param_id = (int)(intptr_t)lv_event_get_user_data(e);
    
    s_ap_selected = param_id== s_ap_selected ? AP_PARAM_COUNT : param_id;

    refresh_ap_selection();
}

// ---------------------------------------------------------------------------
//  Build the Flight Data page
// ---------------------------------------------------------------------------
static void build_flight_page(lv_obj_t *tab)
{

    lv_obj_set_style_bg_color(tab, CLR_BG, 0);
    lv_obj_clear_flag(tab, LV_OBJ_FLAG_SCROLLABLE);

    // --- Row 0: Speed + Altitude ---
    s_lbl_speed    = create_data_box(tab, "SPEED",    "kts", 0, 0);
    s_lbl_altitude = create_data_box(tab, "ALTITUDE", "ft",  1, 0);

    // --- Row 1: V/Speed + Heading ---
    s_lbl_vspeed  = create_data_box(tab, "V/SPEED",  "fpm", 0, 1);
    s_lbl_heading = create_data_box(tab, "HEADING",  "deg", 1, 1);

    // --- Row 2: Bars (throttle, mixture) ---
    create_bar_row(tab, "THROTTLE", &s_bar_throttle,
                   &s_lbl_throttle_val, 210,      CLR_GREEN);
    create_bar_row(tab, "MIXTURE",  &s_bar_mixture,
                   &s_lbl_mixture_val,  256, CLR_ORANGE);

    // --- Status row: Gear + Flaps + AP ---
    s_lbl_gear = lv_label_create(tab);
    lv_label_set_text(s_lbl_gear, "GEAR: --");
    lv_obj_set_style_text_font(s_lbl_gear, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(s_lbl_gear, CLR_WHITE, 0);
    lv_obj_set_pos(s_lbl_gear, 30, 305);

    s_lbl_flaps = lv_label_create(tab);
    lv_label_set_text(s_lbl_flaps, "FLAPS: --");
    lv_obj_set_style_text_font(s_lbl_flaps, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(s_lbl_flaps, CLR_WHITE, 0);
    lv_obj_set_pos(s_lbl_flaps, 180, 305);

    s_lbl_ap_status = lv_label_create(tab);
    lv_label_set_text(s_lbl_ap_status, "AP: OFF");
    lv_obj_set_style_text_font(s_lbl_ap_status, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(s_lbl_ap_status, CLR_RED, 0);
    lv_obj_set_pos(s_lbl_ap_status, 30, 340);

    // --- Rudder Meter (centred, with centre mark) ---
    lv_obj_t *lbl_rud = lv_label_create(tab);
    lv_label_set_text(lbl_rud, "RUDDER");
    lv_obj_set_style_text_color(lbl_rud, CLR_GREY, 0);
    lv_obj_set_style_text_font(lbl_rud, &lv_font_montserrat_12, 0);
    //lv_obj_set_pos(lbl_rud, 8, bar_y + 96);
    lv_obj_align(lbl_rud, LV_ALIGN_TOP_MID, 0, 370);

    static lv_point_t scale_points[] = { {60, 405}, {260, 405} };
    lv_obj_t *rud_line = lv_line_create(tab);
    lv_line_set_points(rud_line, scale_points, 2);
    lv_obj_set_style_line_color(rud_line, CLR_GREY , LV_PART_MAIN);
    lv_obj_set_style_line_width(rud_line, 2, LV_PART_MAIN);

    static lv_point_t center_points[] = { {159, 398}, {159, 405} };
    lv_obj_t *center_marker = lv_line_create(tab);
    lv_line_set_points(center_marker, center_points, 2);
    lv_obj_set_style_line_color(center_marker, CLR_GREY, LV_PART_MAIN);
    lv_obj_set_style_line_width(center_marker, 3, LV_PART_MAIN);

    rud_pointer = lv_obj_create(tab);
    lv_obj_set_size(rud_pointer, 3, 16);
    lv_obj_set_style_bg_color(rud_pointer, CLR_ORANGE, LV_PART_MAIN); // Orange
    lv_obj_set_style_border_width(rud_pointer, 0, LV_PART_MAIN); // Keine Umrandung
    lv_obj_clear_flag(rud_pointer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(rud_pointer, 158, 390);
}

// ---------------------------------------------------------------------------
//  Build the Autopilot Settings page
// ---------------------------------------------------------------------------
static void build_ap_page(lv_obj_t *tab)
{
    lv_obj_set_style_bg_color(tab, CLR_BG, 0);
    lv_obj_clear_flag(tab, LV_OBJ_FLAG_SCROLLABLE);

    // --- AP mode buttons row ---
    s_btn_hdg = create_ap_btn(tab, "HDG",  8,  100);
    s_btn_alt = create_ap_btn(tab, "ALT",  8, 180);
    s_btn_vs  = create_ap_btn(tab, "V/S",  8, 260);
    s_btn_nav = create_ap_btn(tab, "NAV",  175,8);

    // --- AP Master button ---
    s_btn_ap_master = lv_btn_create(tab);
    lv_obj_set_size(s_btn_ap_master, 130, 70);
    lv_obj_set_pos(s_btn_ap_master, 15, 8);
    lv_obj_set_style_bg_color(s_btn_ap_master, lv_color_hex(0x8B0000),
                               LV_STATE_DEFAULT);
    lv_obj_set_style_radius(s_btn_ap_master, 8, 0);
    lv_obj_add_event_cb(s_btn_ap_master, ap_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_ap = lv_label_create(s_btn_ap_master);
    lv_label_set_text(lbl_ap, "AP");
    lv_obj_set_style_text_font(lbl_ap, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_ap, CLR_WHITE, 0);
    lv_obj_center(lbl_ap);

    // --- Parameter rows ---

    lv_obj_t *lbl_hdg_v, *lbl_alt_v, *lbl_vs_v;

    create_ap_param_row(tab, "HEADING",       &lbl_hdg_v,
                        &s_param_boxes[AP_PARAM_HEADING],  100);
    create_ap_param_row(tab, "ALTITUDE  (ft)",&lbl_alt_v,
                        &s_param_boxes[AP_PARAM_ALTITUDE], 180);
    create_ap_param_row(tab, "VERT SPEED(fpm)",&lbl_vs_v,
                        &s_param_boxes[AP_PARAM_VSPEED],   260);

    lv_obj_t *lbl_el_trim = lv_obj_create(tab);
    lv_obj_set_size(lbl_el_trim, 260, 70);
    lv_obj_set_pos(lbl_el_trim, 30, 340);
    lv_obj_set_style_bg_color(lbl_el_trim, CLR_PANEL, 0);
    lv_obj_set_style_border_color(lbl_el_trim, CLR_GREY, 0);
    lv_obj_set_style_border_width(lbl_el_trim, 1, 0);
    lv_obj_set_style_radius(lbl_el_trim, 6, 0);
    lv_obj_add_flag(lbl_el_trim, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(lbl_el_trim, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl_etrim_title =  lv_label_create(lbl_el_trim);
    lv_label_set_text(lbl_etrim_title, "Elev. Trim");
    lv_obj_set_style_text_color(lbl_etrim_title, CLR_GREY, 0);
    lv_obj_set_style_text_font(lbl_etrim_title, &lv_font_montserrat_12, 0);
    lv_obj_align(lbl_etrim_title, LV_ALIGN_LEFT_MID, 8, 0);

    lv_obj_t *lbl_el_trim_val = lv_label_create(lbl_el_trim);
    lv_label_set_text(lbl_el_trim_val, "---");
    lv_obj_set_style_text_color(lbl_el_trim_val, CLR_WHITE, 0);
    lv_obj_set_style_text_font(lbl_el_trim_val, &lv_font_montserrat_24, 0);
    lv_obj_align(lbl_el_trim_val, LV_ALIGN_RIGHT_MID, -8, 0);

    s_param_boxes[AP_PARAM_ETRIM]= lbl_el_trim;

    lv_obj_add_event_cb(s_param_boxes[AP_PARAM_HEADING], ap_row_click_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)AP_PARAM_HEADING);
    lv_obj_add_event_cb(s_param_boxes[AP_PARAM_ALTITUDE], ap_row_click_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)AP_PARAM_ALTITUDE);
    lv_obj_add_event_cb(s_param_boxes[AP_PARAM_VSPEED], ap_row_click_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)AP_PARAM_VSPEED);
    lv_obj_add_event_cb(s_param_boxes[AP_PARAM_ETRIM], ap_row_click_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)AP_PARAM_ETRIM);
    

    s_lbl_ap_hdg_val = lbl_hdg_v;
    s_lbl_ap_alt_val = lbl_alt_v;
    s_lbl_ap_vs_val  = lbl_vs_v;
    s_lbl_elev_trim = lbl_el_trim_val;

    // Initial selection highlight
    refresh_ap_selection();
}

// ===========================================================================
//  Public API
// ===========================================================================

void display_pages_create(void)
{
    s_page_flight = lv_obj_create(lv_scr_act());
    s_page_ap     = lv_obj_create(lv_scr_act());
    s_page_nav = lv_obj_create(lv_scr_act());

    lv_obj_remove_style_all(s_page_flight);
    lv_obj_remove_style_all(s_page_ap);
    lv_obj_remove_style_all(s_page_nav);

    lv_obj_set_size(s_page_flight, 320, 420);
    lv_obj_set_size(s_page_ap,     320, 420);
    lv_obj_set_size(s_page_nav, 320, 420);

    lv_obj_set_pos(s_page_flight, 0, 0);
    lv_obj_set_pos(s_page_ap,     0, 0);
    lv_obj_set_pos(s_page_nav, 0, 0);

    build_flight_page(s_page_flight);
    build_ap_page(s_page_ap);

    lv_obj_t *btn1 = lv_btn_create(lv_scr_act());
    lv_obj_set_size(btn1, 106, 60);
    lv_obj_set_pos(btn1, 0, 420);
    lv_obj_t *btn1label= lv_label_create(btn1);
    lv_label_set_text(btn1label, "PFD");
    lv_obj_set_style_text_font(btn1label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(btn1label, CLR_BG, 0);
    lv_obj_center(btn1label);

    lv_obj_t *btn2 = lv_btn_create(lv_scr_act());
    lv_obj_set_size(btn2, 106, 60);
    lv_obj_set_pos(btn2, 107, 420);
    lv_obj_t *btn2label= lv_label_create(btn2);
    lv_label_set_text(btn2label, "Autopilot");
    lv_obj_set_style_text_font(btn2label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(btn2label, CLR_BG, 0);
    lv_obj_center(btn2label);

    lv_obj_t *btn3 = lv_btn_create(lv_scr_act());
    lv_obj_set_size(btn3, 106, 60);
    lv_obj_set_pos(btn3, 214, 420);

    lv_obj_add_event_cb(btn1, nav_btn_cb,
                        LV_EVENT_CLICKED, (void*)0);

    lv_obj_add_event_cb(btn2, nav_btn_cb,
                        LV_EVENT_CLICKED, (void*)1);

    lv_obj_add_event_cb(btn3, nav_btn_cb,
                        LV_EVENT_CLICKED, (void*)2);

    lv_obj_t *lbl = lv_label_create(s_page_nav);

    lv_label_set_text(lbl, "NAV");

    lv_obj_t *bigbtn = lv_btn_create(s_page_nav);
    lv_obj_set_size(bigbtn, 320,360);
    lv_obj_set_pos(bigbtn, 0, 0);

    lv_obj_add_event_cb(bigbtn, bigbtn_click_event_cb, LV_EVENT_CLICKED, NULL);

    // Default AP values
    s_ap_local.target_heading  = 0;
    s_ap_local.target_altitude = 1000;
    s_ap_local.target_vspeed   = 0;
    s_ap_local.target_elev_trim   = 0;

    coords = lv_label_create(s_page_nav);
    lv_obj_set_style_text_font(coords, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(coords, CLR_WHITE, 0);
    lv_obj_set_pos(coords, 20, 240);
    lv_label_set_text(coords, "rawX= 0 rawY= 0 px= 0 py=0");


    show_page(0);
}

static void show_page(uint8_t page)
{
    lv_obj_add_flag(s_page_flight, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_page_ap, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_page_nav, LV_OBJ_FLAG_HIDDEN);

    switch(page)
    {
        case 0:
            lv_obj_clear_flag(s_page_flight, LV_OBJ_FLAG_HIDDEN);
            break;

        case 1:
            lv_obj_clear_flag(s_page_ap, LV_OBJ_FLAG_HIDDEN);
            break;

        case 2:
            lv_obj_clear_flag(s_page_nav, LV_OBJ_FLAG_HIDDEN);
            break;
    }

    s_current_page = page;
}

static void nav_btn_cb(lv_event_t *e){
    uint32_t page = (uint32_t)lv_event_get_user_data(e);

    show_page(page);
}

static void bigbtn_click_event_cb(lv_event_t *e)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "rawX=%u rawY=%u px=%d py=%d", raw_x, raw_y, px, py);
    lv_label_set_text(coords, buf);

}

// ---------------------------------------------------------------------------
void display_pages_update_flight(const flight_data_t *fd)
{
    if (!fd->valid) return;

    static uint32_t last_update_ms = 0;
    uint32_t now = lv_tick_get();

    char buf[32];

    snprintf(buf, sizeof(buf), "%d", (int)fd->airspeed_kts);
    lv_label_set_text(s_lbl_speed, buf);

    snprintf(buf, sizeof(buf), "%d", (int)fd->altitude_ft);
    lv_label_set_text(s_lbl_altitude, buf);

    snprintf(buf, sizeof(buf), "%+d", (int)fd->vspeed_fpm);
    lv_label_set_text(s_lbl_vspeed, buf);

    snprintf(buf, sizeof(buf), "%03d", (int)fd->heading_deg);
    lv_label_set_text(s_lbl_heading, buf);

    // Throttle bar + label
    int thr = (int)fd->throttle_pct;
    if (thr < 0) thr = 0; if (thr > 100) thr = 100;
    lv_bar_set_value(s_bar_throttle, thr, LV_ANIM_OFF);
    snprintf(buf, sizeof(buf), "%d%%", thr);
    lv_label_set_text(s_lbl_throttle_val, buf);

    // Mixture bar + label
    int mix = (int)fd->mixture_pct;
    if (mix < 0) mix = 0; if (mix > 100) mix = 100;
    lv_bar_set_value(s_bar_mixture, mix, LV_ANIM_OFF);
    snprintf(buf, sizeof(buf), "%d%%", mix);
    lv_label_set_text(s_lbl_mixture_val, buf);

    // Rudder bar  (-100 to +100)
    int rud = (int)fd->rudder_pct;
    if (rud < -100) rud = -100; if (rud > 100) rud = 100;
    int rud_x_pos = 158 + (rud * 100) / 100;
    lv_obj_set_pos(rud_pointer, rud_x_pos, 390);

    // Gear
    lv_label_set_text(s_lbl_gear,
        fd->gear_down ? "GEAR: DOWN" : "GEAR: UP");
    lv_obj_set_style_text_color(s_lbl_gear,
        fd->gear_down ? CLR_GREEN : CLR_YELLOW, 0);

    // Flaps
    uint8_t fi = fd->flaps_pos;
    if(fi==0) snprintf(buf, sizeof(buf), "FLAPS: UP");
    else if(fi==100) snprintf(buf, sizeof(buf), "FLAPS: DOWN");
    else snprintf(buf, sizeof(buf), "FLAPS: %u %%", fi);
    lv_label_set_text(s_lbl_flaps, buf);

    // AP status line (summarise active modes)
    if (fd->ap_engaged) {
        char ap_buf[48] = "AP:";
        if (fd->ap_hdg_mode) strcat(ap_buf, " HDG");
        if (fd->ap_alt_mode) strcat(ap_buf, " ALT");
        if (fd->ap_vs_mode)  strcat(ap_buf, " VS");
        if (fd->ap_nav_mode) strcat(ap_buf, " NAV");
        lv_label_set_text(s_lbl_ap_status, ap_buf);
        lv_obj_set_style_text_color(s_lbl_ap_status, CLR_GREEN, 0);
    } else {
        lv_label_set_text(s_lbl_ap_status, "AP: OFF");
        lv_obj_set_style_text_color(s_lbl_ap_status, CLR_RED, 0);
    }
}

// ---------------------------------------------------------------------------
void display_pages_update_ap(const flight_data_t *fd)
{
    if (!fd->valid) return;
    char buf[32];

    // Sync button colours with actual AP state from sim
    lv_obj_set_style_bg_color(s_btn_hdg,
        fd->ap_hdg_mode ? CLR_BTN_ON : CLR_BTN_OFF, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(s_btn_alt,
        fd->ap_alt_mode ? CLR_BTN_ON : CLR_BTN_OFF, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(s_btn_vs,
        fd->ap_vs_mode  ? CLR_BTN_ON : CLR_BTN_OFF, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(s_btn_nav,
        fd->ap_nav_mode ? CLR_BTN_ON : CLR_BTN_OFF, LV_STATE_DEFAULT);

    // If no local edit has been made yet, show sim values
    if (!s_ap_changed && (lv_tick_get() > s_ap_user_edit_lock_ms)) {
        s_ap_local.target_heading  = fd->ap_heading;
        s_ap_local.target_altitude = fd->ap_altitude;
        s_ap_local.target_vspeed   = fd->ap_vspeed;
        s_ap_local.target_elev_trim   = fd->elev_trim;
        s_ap_local.hdg_mode = fd->ap_hdg_mode;
        s_ap_local.alt_mode = fd->ap_alt_mode;
        s_ap_local.vs_mode  = fd->ap_vs_mode;
        s_ap_local.nav_mode = fd->ap_nav_mode;
    }

    snprintf(buf, sizeof(buf), "%03u deg", s_ap_local.target_heading);
    lv_label_set_text(s_lbl_ap_hdg_val, buf);

    snprintf(buf, sizeof(buf), "%u ft",   s_ap_local.target_altitude);
    lv_label_set_text(s_lbl_ap_alt_val, buf);

    snprintf(buf, sizeof(buf), "%+d fpm", s_ap_local.target_vspeed);
    lv_label_set_text(s_lbl_ap_vs_val,  buf);

    snprintf(buf, sizeof(buf), "%+d %%", s_ap_local.target_elev_trim);
    lv_label_set_text(s_lbl_elev_trim,  buf);
}

// ---------------------------------------------------------------------------
//  Encoder step: change the selected parameter value
// ---------------------------------------------------------------------------
void display_pages_encoder_step(int8_t direction)
{
    switch (s_ap_selected) {
        case AP_PARAM_HEADING:
            s_ap_local.target_heading =
                (uint16_t)((s_ap_local.target_heading + direction + 360) % 360);
            break;
        case AP_PARAM_ALTITUDE:
            int32_t t_alt= (int32_t)s_ap_local.target_altitude + direction * 100;
            s_ap_local.target_altitude = t_alt > 0 ? (uint32_t)t_alt : 0;
            if (s_ap_local.target_altitude > 50000) s_ap_local.target_altitude = 50000;
            break;
        case AP_PARAM_VSPEED:
            s_ap_local.target_vspeed =
                (int16_t)(s_ap_local.target_vspeed + direction * 100);
            if (s_ap_local.target_vspeed >  6000) s_ap_local.target_vspeed =  6000;
            if (s_ap_local.target_vspeed < -6000) s_ap_local.target_vspeed = -6000;
            break;
        case AP_PARAM_ETRIM:
            s_ap_local.target_elev_trim =
                (int8_t)(s_ap_local.target_elev_trim + direction);
            if (s_ap_local.target_elev_trim >  100) s_ap_local.target_elev_trim =  100;
            if (s_ap_local.target_elev_trim < -100) s_ap_local.target_elev_trim = -100;
            break;
        default: break;
    }
    s_ap_changed = true;

    // Immediately refresh displayed value
    char buf[32];
    snprintf(buf, sizeof(buf), "%03u deg", s_ap_local.target_heading);
    lv_label_set_text(s_lbl_ap_hdg_val, buf);
    snprintf(buf, sizeof(buf), "%u ft",   s_ap_local.target_altitude);
    lv_label_set_text(s_lbl_ap_alt_val, buf);
    snprintf(buf, sizeof(buf), "%+d fpm", s_ap_local.target_vspeed);
    lv_label_set_text(s_lbl_ap_vs_val,  buf);
    snprintf(buf, sizeof(buf), "%+d %%", s_ap_local.target_elev_trim);
    lv_label_set_text(s_lbl_elev_trim,  buf);

    s_ap_user_edit_lock_ms = lv_tick_get() + 1000;

}

// ---------------------------------------------------------------------------
//  Encoder button short press: cycle selected parameter
// ---------------------------------------------------------------------------
void display_pages_encoder_press(void)
{
    s_ap_selected = (ap_param_t)((s_ap_selected + 1) % AP_PARAM_COUNT);
    refresh_ap_selection();
}

// ---------------------------------------------------------------------------
//  Encoder button long press: toggle AP master
// ---------------------------------------------------------------------------
void display_pages_encoder_long_press(void)
{
    s_ap_local.ap_toggle = true;
    s_ap_changed = true;
}

// ---------------------------------------------------------------------------
//  Read back AP settings for Core0
// ---------------------------------------------------------------------------
bool display_pages_get_ap_settings(ap_settings_t *out)
{
    if (!s_ap_changed) return false;
    *out = s_ap_local;
    s_ap_local.ap_toggle = false;  // clear one-shot flag
    s_ap_changed         = false;
    out->changed         = true;
    return true;
}