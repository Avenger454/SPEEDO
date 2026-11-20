#include "../ui.h"
#include <math.h>
#include <stdio.h>

extern const uint8_t face_start[] asm("_binary_speedo_face_rgb565_start");
extern const uint8_t boost_face_start[] asm("_binary_boost_face_rgb565_start");
static const lv_img_dsc_t boost_face = {
    .header = {.cf = LV_IMG_CF_TRUE_COLOR, .w = 512, .h = 512},
    .data_size = 512 * 512 * 2, .data = boost_face_start
};
LV_FONT_DECLARE(speedo_digits_170);
LV_FONT_DECLARE(speedo_text_28);
static const lv_img_dsc_t face = {
    .header = {.cf = LV_IMG_CF_TRUE_COLOR, .w = 512, .h = 512},
    .data_size = 512 * 512 * 2, .data = face_start
};
lv_obj_t *ui_Screen1, *ui_rpm_bg, *ui_rpm_arc;
lv_obj_t *ui_label_mph_value, *ui_label_mph, *ui_label_gear;
lv_obj_t *ui_label_gear_value, *ui_label_odometer_value;
static lv_obj_t *trip_label;
static lv_obj_t *render_root;
typedef struct {
    lv_obj_t *root, *background, *needle, *speed, *trip, *odo;
} speedo_page_widgets_t;
static speedo_page_widgets_t pages[2];
static ui_speedo_page_t active_page = UI_SPEEDO_PAGE_MAIN;
static int current_mph;
static float current_pressure_psi;
static int last_pressure_tenth = -9999;
static int last_speed = -1;
static double last_trip = -1, last_odo = -1;
static lv_point_t pointer_points[2], last_screen_points[2];
static bool pointer_position_valid;

static lv_obj_t *black_panel(int x, int y, int width, int height)
{
    lv_obj_t *obj = lv_obj_create(render_root);
    lv_obj_remove_style_all(obj);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, width, height);
    lv_obj_set_style_bg_color(obj, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    return obj;
}

static lv_obj_t *value_label(lv_obj_t *parent, const lv_font_t *font)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_remove_style_all(label);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_label_set_text(label, "0");
    lv_obj_center(label);
    return label;
}


/* Build the scale from one shared linear mapping, replacing the mockup's
 * uneven numbers and ticks. All scale objects are static after creation. */
static lv_point_t scale_tick_points[41][2];

static lv_obj_t *scale_arc(int radius, int width, uint32_t color,
                           int start_angle, int end_angle)
{
    lv_obj_t *arc = lv_arc_create(render_root);
    lv_obj_remove_style_all(arc);
    lv_obj_set_pos(arc, 360 - radius, 360 - radius);
    lv_obj_set_size(arc, radius * 2, radius * 2);
    lv_arc_set_rotation(arc, 0);
    lv_arc_set_bg_angles(arc, start_angle, end_angle);
    lv_obj_set_style_arc_color(arc, lv_color_hex(color), LV_PART_MAIN);
    lv_obj_set_style_arc_opa(arc, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, width, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(arc, false, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(arc, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    return arc;
}

static void create_speed_scale(void)
{
    /* Erase the old annulus, including every printed number and tick.
     * Redraw both boundaries so the mask cannot leave mismatched scale ends. */
    scale_arc(354, 136, 0x000000, 150, 390);
    scale_arc(352, 3, 0x00BFFF, 150, 390);
    scale_arc(220, 3, 0x009FFF, 150, 390);
    scale_arc(352, 3, 0xFF0000, 330, 390);
    for(unsigned tick = 0; tick <= 40; ++tick) {
        int mph = (int)tick * 2;
        bool major = mph % 10 == 0;
        float angle = (150.0f + mph * 3.0f) * (3.14159265358979323846f / 180.0f);
        float x = cosf(angle), y = sinf(angle);
        int inner = major ? 326 : 339;
        scale_tick_points[tick][0] = (lv_point_t){
            (lv_coord_t)lroundf(360 + inner * x),
            (lv_coord_t)lroundf(360 + inner * y)};
        scale_tick_points[tick][1] = (lv_point_t){
            (lv_coord_t)lroundf(360 + 348 * x),
            (lv_coord_t)lroundf(360 + 348 * y)};
        lv_obj_t *line = lv_line_create(render_root);
        lv_obj_remove_style_all(line);
        lv_line_set_points(line, scale_tick_points[tick], 2);
        lv_obj_set_style_line_color(line, lv_color_white(), 0);
        lv_obj_set_style_line_width(line, major ? 4 : 2, 0);
        lv_obj_clear_flag(line, LV_OBJ_FLAG_CLICKABLE);
        if(major) {
            lv_obj_t *label = lv_label_create(render_root);
            lv_obj_set_style_text_font(label, &speedo_text_28, 0);
            lv_obj_set_style_text_color(label, lv_color_white(), 0);
            lv_label_set_text_fmt(label, "%d", mph);
            lv_obj_update_layout(label);
            lv_obj_set_pos(label,
                (lv_coord_t)lroundf(360 + 288 * x) - lv_obj_get_width(label) / 2,
                (lv_coord_t)lroundf(360 + 288 * y) - lv_obj_get_height(label) / 2);
        }
    }
}

static lv_point_t boost_tick_points[41][2];

static void create_boost_scale(void)
{
    scale_arc(354, 136, 0x000000, 180, 360);
    scale_arc(352, 3, 0x00BFFF, 180, 270);
    scale_arc(352, 3, 0xFF0000, 270, 360);
    scale_arc(220, 3, 0x009FFF, 180, 360);
    for(unsigned tick = 0; tick <= 40; ++tick) {
        int value = (int)tick - 20;
        bool major = value % 5 == 0;
        float radians = (180.0f + tick * 4.5f) * (3.14159265358979323846f / 180.0f);
        float x = cosf(radians), y = sinf(radians);
        int radius = major ? 326 : 339;
        boost_tick_points[tick][0] = (lv_point_t){
            (lv_coord_t)lroundf(360 + radius * x),
            (lv_coord_t)lroundf(360 + radius * y)};
        boost_tick_points[tick][1] = (lv_point_t){
            (lv_coord_t)lroundf(360 + 348 * x),
            (lv_coord_t)lroundf(360 + 348 * y)};
        lv_obj_t *line = lv_line_create(render_root);
        lv_obj_remove_style_all(line);
        lv_line_set_points(line, boost_tick_points[tick], 2);
        lv_obj_set_style_line_color(line, lv_color_white(), 0);
        lv_obj_set_style_line_width(line, major ? 4 : 2, 0);
        lv_obj_clear_flag(line, LV_OBJ_FLAG_CLICKABLE);
        if(major) {
            lv_obj_t *label = value_label(render_root, &speedo_text_28);
            /* Coordinates below are absolute within the page, not offsets
             * from the center alignment used by value_label(). */
            lv_obj_set_align(label, LV_ALIGN_TOP_LEFT);
            lv_label_set_text_fmt(label, "%d", value);
            lv_obj_update_layout(label);
            lv_obj_set_pos(label,
                (lv_coord_t)lroundf(360 + 288 * x) - lv_obj_get_width(label) / 2,
                (lv_coord_t)lroundf(360 + 288 * y) - lv_obj_get_height(label) / 2 -
                    ((tick == 0 || tick == 40) ? 18 : 0));
        }
    }
    /* Replace the reference's ambiguous units with separate VAC/BOOST units. */
    lv_obj_t *vac = value_label(black_panel(14, 360, 145, 80), &speedo_text_28);
    lv_label_set_text(vac, "VAC\ninHg");
    lv_obj_set_style_text_align(vac, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(vac, lv_color_hex(0x00BFFF), 0);
    lv_obj_center(vac);
    lv_obj_t *boost = value_label(black_panel(558, 360, 150, 80), &speedo_text_28);
    lv_label_set_text(boost, "BOOST\nPSI");
    lv_obj_set_style_text_align(boost, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(boost, lv_color_hex(0xFF0000), 0);
    lv_obj_center(boost);
}

static void build_page(ui_speedo_page_t page)
{
    speedo_page_widgets_t *widgets = &pages[page];
    widgets->root = lv_obj_create(ui_Screen1);
    lv_obj_remove_style_all(widgets->root);
    lv_obj_set_size(widgets->root, 720, 720);
    lv_obj_clear_flag(widgets->root, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    render_root = widgets->root;
    widgets->background = lv_img_create(render_root);
    lv_img_set_src(widgets->background, page == UI_SPEEDO_PAGE_MAIN ? &face : &boost_face);
    lv_obj_center(widgets->background);
    lv_img_set_zoom(widgets->background, 425);
    if(page == UI_SPEEDO_PAGE_MAIN) create_speed_scale();
    else create_boost_scale();
    widgets->speed = value_label(black_panel(225, 300, 275, 134), &speedo_digits_170);
    if(page == UI_SPEEDO_PAGE_MAIN) {
        widgets->trip = value_label(black_panel(201, 539, 128, 40), &speedo_text_28);
        widgets->odo = value_label(black_panel(385, 539, 155, 40), &speedo_text_28);
    }
    widgets->needle = lv_line_create(render_root);
    lv_obj_remove_style_all(widgets->needle);
    lv_obj_set_style_line_width(widgets->needle, 7, 0);
    lv_obj_set_style_line_color(widgets->needle, lv_color_hex(0xFF0000), 0);
    lv_obj_set_style_line_opa(widgets->needle, LV_OPA_COVER, 0);
    lv_obj_set_style_line_rounded(widgets->needle, false, 0);
    lv_obj_clear_flag(widgets->needle, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(widgets->root, LV_OBJ_FLAG_HIDDEN);
}

void ui_speedo_set_page(ui_speedo_page_t page)
{
    if(page != UI_SPEEDO_PAGE_MAIN && page != UI_SPEEDO_PAGE_BOOST) return;
    if(!pages[page].root) return;
    if(ui_label_mph_value && active_page == page) return;
    for(unsigned i = 0; i < 2; ++i) lv_obj_add_flag(pages[i].root, LV_OBJ_FLAG_HIDDEN);
    active_page = page;
    speedo_page_widgets_t *widgets = &pages[page];
    ui_rpm_bg = widgets->background;
    ui_rpm_arc = widgets->needle;
    ui_label_mph_value = widgets->speed;
    trip_label = widgets->trip;
    ui_label_odometer_value = widgets->odo;
    last_speed = -1;
    last_pressure_tenth = -9999;
    last_trip = last_odo = -1;
    pointer_position_valid = false;
    ui_speedo_set_speed(current_mph);
    ui_speedo_set_pressure(current_pressure_psi);
    lv_obj_clear_flag(widgets->root, LV_OBJ_FLAG_HIDDEN);
}

void ui_Screen1_screen_init(void)
{
    ui_Screen1 = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(ui_Screen1, lv_color_black(), 0);
    lv_obj_clear_flag(ui_Screen1, LV_OBJ_FLAG_SCROLLABLE);
    ui_label_mph_value = NULL;
    current_mph = 0;
    current_pressure_psi = 0;
    build_page(UI_SPEEDO_PAGE_MAIN);
    build_page(UI_SPEEDO_PAGE_BOOST);
    ui_speedo_set_page(UI_SPEEDO_PAGE_MAIN);
    ui_speedo_set_distances(0, 0);
}

static void set_needle_angle(float angle)
{
    float radians = angle * (3.14159265358979323846f / 180.0f);
    float x = cosf(radians), y = sinf(radians);
    lv_point_t screen_points[2] = {
        {(lv_coord_t)lroundf(360.0f + 222.0f * x),
         (lv_coord_t)lroundf(360.0f + 222.0f * y)},
        {(lv_coord_t)lroundf(360.0f + 348.0f * x),
         (lv_coord_t)lroundf(360.0f + 348.0f * y)}
    };
    if(pointer_position_valid &&
       screen_points[0].x == last_screen_points[0].x &&
       screen_points[0].y == last_screen_points[0].y &&
       screen_points[1].x == last_screen_points[1].x &&
       screen_points[1].y == last_screen_points[1].y) return;

    lv_coord_t left = LV_MIN(screen_points[0].x, screen_points[1].x);
    lv_coord_t top = LV_MIN(screen_points[0].y, screen_points[1].y);
    lv_coord_t width = LV_ABS(screen_points[1].x - screen_points[0].x) + 1;
    lv_coord_t height = LV_ABS(screen_points[1].y - screen_points[0].y) + 1;
    /* Invalidate the previous geometry BEFORE modifying the referenced points. */
    lv_obj_invalidate(ui_rpm_arc);
    for(unsigned i = 0; i < 2; ++i) {
        last_screen_points[i] = screen_points[i];
        pointer_points[i].x = screen_points[i].x - left;
        pointer_points[i].y = screen_points[i].y - top;
    }
    /* Local coordinates keep the object's dirty rectangle around the needle,
     * rather than stretching from screen origin to the needle's far end. */
    lv_obj_set_pos(ui_rpm_arc, left, top);
    lv_obj_set_size(ui_rpm_arc, width, height);
    lv_line_set_points(ui_rpm_arc, pointer_points, 2);
    pointer_position_valid = true;
}


void ui_speedo_set_speed(int mph)
{
    if(mph < 0) mph = 0;
    if(mph > 999) mph = 999;
    current_mph = mph;
    if(!ui_label_mph_value || !ui_rpm_arc || mph == last_speed) return;
    last_speed = mph;
    lv_label_set_text_fmt(ui_label_mph_value, "%d", mph);
    lv_obj_center(ui_label_mph_value);
    if(active_page == UI_SPEEDO_PAGE_MAIN) {
        int capped = mph > 80 ? 80 : mph;
        set_needle_angle(150.0f + capped * 3.0f);
    }
}

void ui_speedo_set_pressure(float gauge_psi)
{
    if(!isfinite(gauge_psi)) gauge_psi = 0;
    /* Input is gauge PSI from the existing CAN mapping. Negative pressure is
     * displayed as inHg vacuum; positive pressure remains PSI boost. */
    current_pressure_psi = gauge_psi;
    if(active_page != UI_SPEEDO_PAGE_BOOST || !ui_rpm_arc) return;
    float value = gauge_psi < 0 ? gauge_psi * 2.03602f : gauge_psi;
    if(value < -20) value = -20;
    if(value > 20) value = 20;
    int tenth = (int)lroundf(value * 10.0f);
    if(tenth == last_pressure_tenth) return;
    last_pressure_tenth = tenth;
    set_needle_angle(270.0f + tenth * 0.45f);
}

void ui_speedo_set_distances(double trip_miles, double odo_miles)
{
    if(!trip_label || !ui_label_odometer_value) return;
    if(!isfinite(trip_miles) || trip_miles < 0) trip_miles = 0;
    if(!isfinite(odo_miles) || odo_miles < 0) odo_miles = 0;
    double trip = floor(trip_miles * 10.0) / 10.0;
    double odo = floor(odo_miles);
    char text[32];
    if(trip != last_trip) {
        snprintf(text, sizeof(text), "%.1f mi", trip);
        lv_label_set_text(trip_label, text);
        lv_obj_center(trip_label);
        last_trip = trip;
    }
    if(odo != last_odo) {
        snprintf(text, sizeof(text), "%06.0f mi", odo);
        lv_label_set_text(ui_label_odometer_value, text);
        lv_obj_center(ui_label_odometer_value);
        last_odo = odo;
    }
}

/* Retained for existing legacy tach-task linkage only. */
void ui_tach_set_rpm(int rpm) { (void)rpm; }
void ui_tach_set_gear(int gear) { (void)gear; }

void ui_Screen1_screen_destroy(void)
{
    if(ui_Screen1) lv_obj_del(ui_Screen1);
    ui_Screen1 = ui_rpm_bg = ui_rpm_arc = render_root = NULL;
    ui_label_mph_value = ui_label_mph = ui_label_gear = NULL;
    ui_label_gear_value = ui_label_odometer_value = trip_label = NULL;
    for(unsigned i = 0; i < 2; ++i) pages[i] = (speedo_page_widgets_t){0};
    last_speed = -1;
    pointer_position_valid = false;
}
