#include "../ui.h"
#include <math.h>
#include <stdio.h>

extern const uint8_t face_start[] asm("_binary_speedo_face_rgb565_start");
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
static int last_speed = -1;
static double last_trip = -1, last_odo = -1;
static lv_point_t pointer_points[2], last_screen_points[2];
static bool pointer_position_valid;

static lv_obj_t *black_panel(int x, int y, int width, int height)
{
    lv_obj_t *obj = lv_obj_create(ui_Screen1);
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
    lv_obj_t *arc = lv_arc_create(ui_Screen1);
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
        lv_obj_t *line = lv_line_create(ui_Screen1);
        lv_obj_remove_style_all(line);
        lv_line_set_points(line, scale_tick_points[tick], 2);
        lv_obj_set_style_line_color(line, lv_color_white(), 0);
        lv_obj_set_style_line_width(line, major ? 4 : 2, 0);
        lv_obj_clear_flag(line, LV_OBJ_FLAG_CLICKABLE);
        if(major) {
            lv_obj_t *label = lv_label_create(ui_Screen1);
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

void ui_Screen1_screen_init(void)
{
    ui_Screen1 = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(ui_Screen1, lv_color_black(), 0);
    lv_obj_clear_flag(ui_Screen1, LV_OBJ_FLAG_SCROLLABLE);
    ui_rpm_bg = lv_img_create(ui_Screen1);
    lv_img_set_src(ui_rpm_bg, &face);
    lv_obj_center(ui_rpm_bg);
    /* Fill the display; decorative silver bezel stays outside the screen. */
    lv_img_set_zoom(ui_rpm_bg, 425);
    create_speed_scale();

    lv_obj_t *speed_panel = black_panel(225, 309, 275, 125);
    ui_label_mph_value = value_label(speed_panel, &speedo_digits_170);
    /* Keep title and divider artwork visible while replacing sample values. */
    trip_label = value_label(black_panel(201, 539, 128, 40), &speedo_text_28);
    ui_label_odometer_value = value_label(black_panel(385, 539, 155, 40), &speedo_text_28);

    ui_rpm_arc = lv_line_create(ui_Screen1);
    lv_obj_remove_style_all(ui_rpm_arc);
    lv_obj_set_style_line_width(ui_rpm_arc, 7, 0);
    lv_obj_set_style_line_color(ui_rpm_arc, lv_color_hex(0xFF0000), 0);
    lv_obj_set_style_line_opa(ui_rpm_arc, LV_OPA_COVER, 0);
    lv_obj_set_style_line_rounded(ui_rpm_arc, false, 0);
    lv_obj_clear_flag(ui_rpm_arc, LV_OBJ_FLAG_CLICKABLE);
    /* Keep unused legacy callbacks valid without adding visible gear UI. */
    lv_obj_t **legacy[] = {&ui_label_mph, &ui_label_gear, &ui_label_gear_value};
    for(unsigned i = 0; i < sizeof(legacy) / sizeof(legacy[0]); ++i) {
        *legacy[i] = lv_label_create(ui_Screen1);
        lv_obj_add_flag(*legacy[i], LV_OBJ_FLAG_HIDDEN);
    }
    last_speed = -1;
    last_trip = last_odo = -1;
    pointer_position_valid = false;
    ui_speedo_set_speed(0);
    ui_speedo_set_distances(0, 0);
}

void ui_speedo_set_speed(int mph)
{
    if(!ui_label_mph_value || !ui_rpm_arc) return;
    if(mph < 0) mph = 0;
    if(mph > 999) mph = 999;
    if(mph == last_speed) return;
    last_speed = mph;
    lv_label_set_text_fmt(ui_label_mph_value, "%d", mph);
    lv_obj_center(ui_label_mph_value);
    int capped = mph > 80 ? 80 : mph;
    /* Same mapping as labels and ticks: 30 degrees per 10 MPH. */
    float angle = 150.0f + capped * 3.0f;
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
    ui_Screen1 = ui_rpm_bg = ui_rpm_arc = NULL;
    ui_label_mph_value = ui_label_mph = ui_label_gear = NULL;
    ui_label_gear_value = ui_label_odometer_value = trip_label = NULL;
    last_speed = -1;
    pointer_position_valid = false;
}
