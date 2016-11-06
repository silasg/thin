#include "main_window.h"

static Window *s_main_window;
static TextLayer *s_weekday_layer, *s_day_in_month_layer, *s_month_layer;
static Layer *s_canvas_layer, *s_bg_layer;

static Time s_last_time, s_anim_time;
static char s_weekday_buffer[8], s_month_buffer[8], s_day_in_month_buffer[3];
static bool s_animating, s_connected, s_draw_second_hand;
static int s_draw_second_hand_tap_duration = 0;
static GColor colorForeGround, colorBackGround;

#ifdef PBL_COLOR
static GColor8 colorLight, colorHeavy, colorGreen, colorSecondHand, colorSecondHandFaded, colorAccent, colorAccentFaded;
#endif

void set_last_time(struct tm *tick_time) {
    s_last_time.days = tick_time->tm_mday;
    s_last_time.hours = tick_time->tm_hour;
    s_last_time.minutes = tick_time->tm_min;
    s_last_time.seconds = tick_time->tm_sec;
}

static bool get_draw_second_hand()
{
    BatteryChargeState state = battery_state_service_peek();
    return (config_get(PERSIST_KEY_SECOND_HAND)
            && (!config_get(PERSIST_KEY_SECOND_BATTERY) || state.is_plugged || state.charge_percent >= 20.0F )
            && (!config_get(PERSIST_KEY_SECOND_NIGHT) || (s_last_time.hours > 6 && s_last_time.hours < 23)));
}

static bool get_draw_second_hand_temporary()
{
    return config_get(PERSIST_KEY_SECOND_TAP) && s_draw_second_hand_tap_duration > 0;
}

static bool get_draw_second_hand_resolved()
{
    return get_draw_second_hand() || get_draw_second_hand_temporary();
}


// fuck missing forward declaration in C :x
static void resubscribe_tick_handler_if_needed();
void main_window_reload_config();

static void accel_tap_handler(AccelAxisType axis, int32_t direction) {
  // A tap event occured
  // APP_LOG(APP_LOG_LEVEL_DEBUG, "tap detected");
  s_draw_second_hand_tap_duration = HAND_SECONDS_TAP_DURATION;
  resubscribe_tick_handler_if_needed();
}

static void tick_handler(struct tm *tick_time, TimeUnits changed) {
    
    set_last_time(tick_time);
    
    resubscribe_tick_handler_if_needed();
    
    if (s_draw_second_hand_tap_duration) {
        s_draw_second_hand_tap_duration--;
    }
            
    snprintf(s_day_in_month_buffer, sizeof(s_day_in_month_buffer), "%d", s_last_time.days);
    strftime(s_weekday_buffer, sizeof(s_weekday_buffer), "%a", tick_time);
    strftime(s_month_buffer, sizeof(s_month_buffer), "%b", tick_time);
    
    text_layer_set_text(s_weekday_layer, s_weekday_buffer);
    text_layer_set_text(s_day_in_month_layer, s_day_in_month_buffer);
    text_layer_set_text(s_month_layer, s_month_buffer);
    
    // Finally
    layer_mark_dirty(s_canvas_layer);
}


static void subscribe_tick_handler() {
    if(s_draw_second_hand) {
        tick_timer_service_subscribe(SECOND_UNIT, tick_handler);
    } else {
        tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
    }
    // APP_LOG(APP_LOG_LEVEL_DEBUG, "tick subscribed %d", s_draw_second_hand);
}

static void resubscribe_tick_handler_if_needed() {
    bool new_draw_second_hand = get_draw_second_hand_resolved();
    if (new_draw_second_hand != s_draw_second_hand)
    {
        s_draw_second_hand = new_draw_second_hand;
        // APP_LOG(APP_LOG_LEVEL_DEBUG, "tick unsubscribed");
        tick_timer_service_unsubscribe();
        subscribe_tick_handler();
    }
}



/****************************** AnimationImplementation ***********************/

static void animation_started(Animation *anim, void *context) {
    s_animating = true;
}


static void animation_stopped(Animation *anim, bool stopped, void *context) {
    s_animating = false;
    
    main_window_reload_config();
}

static void animate(int duration, int delay, AnimationImplementation *implementation, bool handlers) {
    Animation *anim = animation_create();
    if(anim) {
        animation_set_duration(anim, duration);
        animation_set_delay(anim, delay);
        animation_set_curve(anim, AnimationCurveEaseInOut);
        animation_set_implementation(anim, implementation);
        if(handlers) {
            animation_set_handlers(anim, (AnimationHandlers) {
                .started = animation_started,
                .stopped = animation_stopped
            }, NULL);
        }
        animation_schedule(anim);
    }
}

void draw_markers(Layer *layer, GContext *ctx) {
    GRect bounds = layer_get_bounds(layer);
    GPoint center = grect_center_point(&bounds);
    
    BatteryChargeState state = battery_state_service_peek();
    int perc = state.charge_percent;
    int batt_hours = (int)(12.0F * ((float)perc / 100.0F)) + 1;
    
    int mmin = (s_last_time.minutes / 5) * 5;
    int mmax = (mmin + 5);
    
    for(int m = 0; m < 60; m+=(config_get(PERSIST_KEY_MINUTE_MARKERS) ? 1 : 5)) {
        int h = m / 5;
        bool isHourMarker = ( m % 5 ) == 0;
        
        if (!isHourMarker && ( m < mmin || m > mmax )) continue;
        if (!isHourMarker && s_animating) continue;
        
        int thickness = isHourMarker ? THICKNESS : 1;
        
        
        for(int y = 0; y < thickness; y++) {
            for(int x = 0; x < thickness; x++) {

                GPoint point = (GPoint) {
                    .x = (int16_t)(sin_lookup(TRIG_MAX_ANGLE * m / 60) * (int32_t)(3 * HAND_LENGTH_SEC) / TRIG_MAX_RATIO) + center.x,
                    .y = (int16_t)(-cos_lookup(TRIG_MAX_ANGLE * m / 60) * (int32_t)(3 * HAND_LENGTH_SEC) / TRIG_MAX_RATIO) + center.y,
                };
                
                if(config_get(PERSIST_KEY_BATTERY)) {
                    if(h < batt_hours) {
#ifdef PBL_COLOR
                        if(state.is_plugged) {
                            // Charging
                            graphics_context_set_stroke_color(ctx, colorGreen);
                        } else {
                            // Discharging at this level
                            graphics_context_set_stroke_color(ctx, colorForeGround);
                        }
#else
                        graphics_context_set_stroke_color(ctx, colorForeGround);
#endif
                    } else {
                        // Empty segment
                        graphics_context_set_stroke_color(ctx, PBL_IF_COLOR_ELSE(colorLight, colorBackGround));
                    }
                } else {
                    // No battery indicator, show all
                    graphics_context_set_stroke_color(ctx, colorForeGround);
                }
                graphics_draw_line(ctx, GPoint(center.x + x, center.y + y), GPoint(point.x + x, point.y + y));
            }
        }
    }
    
    // Make markers
    graphics_context_set_fill_color(ctx, colorBackGround);
#ifdef PBL_ROUND
    graphics_fill_circle(ctx, center, (bounds.size.w / 2) - (2 * MARGIN));
#else
    graphics_fill_rect(ctx, GRect(MARGIN, MARGIN, bounds.size.w - (2 * MARGIN), bounds.size.h - (2 * MARGIN)), 0, GCornerNone);
#endif
}

/****************************** Drawing Functions *****************************/

static void update_background_layer(Layer *layer, GContext *ctx) {
    
    if(config_get(PERSIST_KEY_NO_MARKERS))
        return;
    else
        draw_markers(layer, ctx);
    
}

static GPoint make_hand_point(int quantity, int intervals, int len, GPoint center) {
    return (GPoint) {
        .x = (int16_t)(sin_lookup(TRIG_MAX_ANGLE * quantity / intervals) * (int32_t)len / TRIG_MAX_RATIO) + center.x,
        .y = (int16_t)(-cos_lookup(TRIG_MAX_ANGLE * quantity / intervals) * (int32_t)len / TRIG_MAX_RATIO) + center.y,
    };
}

static int hours_to_minutes(int hours_out_of_12) {
    return (hours_out_of_12 * 60) / 12;
}

void draw_center(GContext *ctx, GPoint center) {
    // Center
    graphics_context_set_fill_color(ctx, colorForeGround);
    graphics_fill_circle(ctx, GPoint(center.x + 1, center.y + 1), 4);
    
    // Draw black if disconnected
    if(config_get(PERSIST_KEY_BT) && !s_connected) {
        graphics_context_set_fill_color(ctx, colorBackGround);
        graphics_fill_circle(ctx, GPoint(center.x + 1, center.y + 1), 3);
    }
}


#ifndef PBL_ROUND
static int get_rectangular_marker_extension(int handPosition) {
    int pos = (handPosition % 30)-15;
    if (pos<0) pos*=-1; // TODO: use Math.Abs() instead
    return pos/2 + 2;
}
#endif

static int get_marker_extension(Time time, int forHand) {
    int result = PBL_IF_COLOR_ELSE(0, 2);
    if (!config_get(PERSIST_KEY_NO_MARKERS)) {
        return result;
    }
    else {
#ifdef PBL_ROUND
        return result+15;
#else
        if (forHand == TIME_MINUTES) {
            return result+get_rectangular_marker_extension(time.minutes);
        }
        else if (forHand == TIME_SECONDS) {
            return result+get_rectangular_marker_extension(time.seconds);
        }
        else { // hours
            return result+5; // don't change hours hand length depending on current time
        }
#endif
    }
}

static bool is_last_faded_second_hand_display() {
    return !get_draw_second_hand() && config_get(PERSIST_KEY_SECOND_TAP) && s_draw_second_hand_tap_duration == 0;
}

void draw_second_hand(GPoint center, Time mode_time, GContext *ctx) {
    
    int len_sec = HAND_LENGTH_SEC;
        
    // Longer when no markers?
    len_sec += get_marker_extension(mode_time, TIME_SECONDS);
    
    // Draw second hand
#ifdef PBL_COLOR
    GPoint second_hand_long = make_hand_point(mode_time.seconds, 60, len_sec, center);
#endif
    len_sec -= (MARGIN + HAND_TIP_SIZE_DELTA);
    GPoint second_hand_short = make_hand_point(mode_time.seconds, 60, len_sec, center);
    
    GColor handColor = PBL_IF_COLOR_ELSE(colorSecondHand, colorForeGround);
    if (is_last_faded_second_hand_display())
        handColor = PBL_IF_COLOR_ELSE(colorSecondHandFaded, colorForeGround);

#ifdef PBL_COLOR
    GColor tipColor = PBL_IF_COLOR_ELSE(colorAccent, colorForeGround);
    if (is_last_faded_second_hand_display())
        tipColor = PBL_IF_COLOR_ELSE(colorAccentFaded, colorForeGround);
#endif
  
    // Use loops
    for(int y = 0; y < THICKNESS - 1; y++) {
        for(int x = 0; x < THICKNESS - 1; x++) {
            graphics_context_set_stroke_color(ctx, handColor);
            graphics_draw_line(ctx, GPoint(center.x + x, center.y + y), GPoint(second_hand_short.x + x, second_hand_short.y + y));
            
#ifdef PBL_COLOR
            // Draw second hand tip
            graphics_context_set_stroke_color(ctx, tipColor);
            graphics_draw_line(ctx, GPoint(second_hand_short.x + x, second_hand_short.y + y), GPoint(second_hand_long.x + x, second_hand_long.y + y));
#endif
        }
    }
}

void draw_min_hour_hands(GPoint center, Time mode_time, GContext *ctx) {
    int len_min = HAND_LENGTH_MIN;
    int len_hour = HAND_LENGTH_HOUR;
    
    // Longer when no markers?
    len_min += get_marker_extension(mode_time, TIME_MINUTES);
    len_hour += get_marker_extension(mode_time, TIME_HOURS);
    
    // Plot hand ends
#ifdef PBL_COLOR
    GPoint minute_hand_long = make_hand_point(mode_time.minutes, 60, len_min, center);
#endif
    
    // Plot shorter overlaid hands
    len_min -= (MARGIN + HAND_TIP_SIZE_DELTA);
    GPoint minute_hand_short = make_hand_point(mode_time.minutes, 60, len_min, center);
    
    float minute_angle = TRIG_MAX_ANGLE * mode_time.minutes / 60;
    float hour_angle;
    if(s_animating) {
        // Hours out of 60 for smoothness
        hour_angle = TRIG_MAX_ANGLE * mode_time.hours / 60;
    } else {
        hour_angle = TRIG_MAX_ANGLE * mode_time.hours / 12;
    }
    hour_angle += (minute_angle / TRIG_MAX_ANGLE) * (TRIG_MAX_ANGLE / 12);
    
    // Hour is more accurate
#ifdef PBL_COLOR
    GPoint hour_hand_long = (GPoint) {
        .x = (int16_t)(sin_lookup(hour_angle) * (int32_t)len_hour / TRIG_MAX_RATIO) + center.x,
        .y = (int16_t)(-cos_lookup(hour_angle) * (int32_t)len_hour / TRIG_MAX_RATIO) + center.y,
    };
#endif
    
    // Shorter hour overlay
    len_hour -= (MARGIN + HAND_TIP_SIZE_DELTA);
    GPoint hour_hand_short = (GPoint) {
        .x = (int16_t)(sin_lookup(hour_angle) * (int32_t)len_hour / TRIG_MAX_RATIO) + center.x,
        .y = (int16_t)(-cos_lookup(hour_angle) * (int32_t)len_hour / TRIG_MAX_RATIO) + center.y,
    };
    
    // Draw hands
    graphics_context_set_stroke_color(ctx, PBL_IF_COLOR_ELSE(colorHeavy, colorForeGround));
    for(int y = 0; y < THICKNESS; y++) {
        for(int x = 0; x < THICKNESS; x++) {
            graphics_draw_line(ctx, GPoint(center.x + x, center.y + y), GPoint(minute_hand_short.x + x, minute_hand_short.y + y));
            graphics_draw_line(ctx, GPoint(center.x + x, center.y + y), GPoint(hour_hand_short.x + x, hour_hand_short.y + y));
        }
    }
#ifdef PBL_COLOR
    // Draw hand tips
    graphics_context_set_stroke_color(ctx, colorForeGround);
    for(int y = 0; y < THICKNESS; y++) {
        for(int x = 0; x < THICKNESS; x++) {
            graphics_draw_line(ctx, GPoint(minute_hand_short.x + x, minute_hand_short.y + y), GPoint(minute_hand_long.x + x, minute_hand_long.y + y));
            graphics_draw_line(ctx, GPoint(hour_hand_short.x + x, hour_hand_short.y + y), GPoint(hour_hand_long.x + x, hour_hand_long.y + y));
        }
    }
#endif
}

static void update_hands_layer(Layer *layer, GContext *ctx) {
    GRect bounds = layer_get_bounds(layer);
    GPoint center = grect_center_point(&bounds);
    
    Time mode_time = (s_animating) ? s_anim_time : s_last_time;
    
    draw_min_hour_hands(center, mode_time, ctx);
    
    if (s_draw_second_hand) {
        draw_second_hand(center, mode_time, ctx);
    }
        
    draw_center(ctx, center);
}

static void bt_handler(bool connected) {
    // Notify disconnection
    if(!connected && s_connected) {
        vibes_long_pulse();
    }
    
    s_connected = connected;
    layer_mark_dirty(s_canvas_layer);
}

static void batt_handler(BatteryChargeState state) {
    resubscribe_tick_handler_if_needed();
    layer_mark_dirty(s_canvas_layer);
}

void create_weekday_layer(int x) {
    s_weekday_layer = text_layer_create(GRect(x, 55, 44, 40));
    text_layer_set_text_alignment(s_weekday_layer, GTextAlignmentCenter);
    text_layer_set_font(s_weekday_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD));
    text_layer_set_text_color(s_weekday_layer, colorForeGround);
    text_layer_set_background_color(s_weekday_layer, GColorClear);
}

void create_day_in_month_layer(int x) {
    s_day_in_month_layer = text_layer_create(GRect(x, 68, 44, 40));
    text_layer_set_text_alignment(s_day_in_month_layer, GTextAlignmentCenter);
    text_layer_set_font(s_day_in_month_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
    text_layer_set_text_color(s_day_in_month_layer, PBL_IF_COLOR_ELSE(colorAccent, colorForeGround));
    text_layer_set_background_color(s_day_in_month_layer, GColorClear);
}

void create_month_layer(int x) {
    s_month_layer = text_layer_create(GRect(x, 95, 44, 40));
    text_layer_set_text_alignment(s_month_layer, GTextAlignmentCenter);
    text_layer_set_font(s_month_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD));
    text_layer_set_text_color(s_month_layer, colorForeGround);
    text_layer_set_background_color(s_month_layer, GColorClear);
}

static void window_load(Window *window) {
    Layer *window_layer = window_get_root_layer(window);
    GRect bounds = layer_get_bounds(window_layer);
    
    s_bg_layer = layer_create(bounds);
    layer_set_update_proc(s_bg_layer, update_background_layer);
    layer_add_child(window_layer, s_bg_layer);
    
    int x = (bounds.size.w * (config_get(PERSIST_KEY_NO_MARKERS) ? 68 : 62)) / 100;
    
    create_weekday_layer(x);
    
    create_day_in_month_layer(x);
    
    create_month_layer(x);
    
    
    s_canvas_layer = layer_create(bounds);
    layer_set_update_proc(s_canvas_layer, update_hands_layer);
    
    //layer_add_child(window_layer, s_canvas_layer);
}

static void window_unload(Window *window) {
    layer_destroy(s_canvas_layer);
    layer_destroy(s_bg_layer);
    
    text_layer_destroy(s_weekday_layer);
    text_layer_destroy(s_day_in_month_layer);
    text_layer_destroy(s_month_layer);
    
    // Self destroying
    window_destroy(s_main_window);
}

static int anim_percentage(AnimationProgress dist_normalized, int max) {
    return (int)(float)(((float)dist_normalized / (float)ANIMATION_NORMALIZED_MAX) * (float)max);
}

static void hands_update(Animation *anim, AnimationProgress dist_normalized) {
    s_anim_time.hours = anim_percentage(dist_normalized, hours_to_minutes(s_last_time.hours % 12));
    s_anim_time.minutes = anim_percentage(dist_normalized, s_last_time.minutes);
    s_anim_time.seconds = anim_percentage(dist_normalized, s_last_time.seconds);
    
    layer_mark_dirty(s_canvas_layer);
}

void set_colors(void) {
    if (config_get(PERSIST_KEY_LIGHT_THEME)) {
        colorForeGround = GColorBlack;
        colorBackGround = GColorWhite;
#ifdef PBL_COLOR
        colorLight = GColorDarkGray;
        colorHeavy = GColorDarkGray;
        colorGreen = GColorIslamicGreen;
        colorSecondHand = GColorFolly;
        colorSecondHandFaded = GColorMelon;
        colorAccent = GColorRed;
        colorAccentFaded = GColorSunsetOrange;
#endif
    }
    else {
        colorForeGround = GColorWhite;
        colorBackGround = GColorBlack;
#ifdef PBL_COLOR
        colorLight = GColorDarkGray;
        colorHeavy = GColorLightGray;
        colorGreen = GColorGreen;
        colorSecondHand = GColorDarkCandyAppleRed;
        colorSecondHandFaded = GColorBulgarianRose;
        colorAccent = GColorChromeYellow;
        colorAccentFaded = GColorWindsorTan;
#endif
    }
}

void main_window_push() {
    
    set_colors();
    
    
    s_main_window = window_create();
    window_set_background_color(s_main_window, colorBackGround);
    window_set_window_handlers(s_main_window, (WindowHandlers) {
        .load = window_load,
        .unload = window_unload,
    });
    window_stack_push(s_main_window, true);
 
    battery_state_service_subscribe(batt_handler);
    
    if (config_get(PERSIST_KEY_SECOND_TAP)) {
        accel_tap_service_subscribe(accel_tap_handler);
        s_draw_second_hand_tap_duration = HAND_SECONDS_TAP_DURATION;
    }
        
    if(config_get(PERSIST_KEY_BT)) {
        bluetooth_connection_service_subscribe(bt_handler);
        bt_handler(bluetooth_connection_service_peek());
    }
    
    s_draw_second_hand = get_draw_second_hand();
    
    // Begin smooth animation
    static AnimationImplementation hands_impl = {
        .update = hands_update
    };
    animate(ANIMATION_DURATION, ANIMATION_DELAY, &hands_impl, true);
    main_window_reload_config();
}

void main_window_reload_config() {
    time_t t = time(NULL);
    struct tm *tm_now = localtime(&t);
    s_last_time.hours = tm_now->tm_hour;
    s_last_time.minutes = tm_now->tm_min;
    s_last_time.seconds = tm_now->tm_sec;
    
    tick_timer_service_unsubscribe();
    
    if(get_draw_second_hand_resolved()) {
        tick_timer_service_subscribe(SECOND_UNIT, tick_handler);
    } else {
        tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
    }
    
    connection_service_unsubscribe();
    if(config_get(PERSIST_KEY_BT)) {
        connection_service_subscribe((ConnectionHandlers) {
            .pebble_app_connection_handler = bt_handler
        });
        bt_handler(connection_service_peek_pebble_app_connection());
    }
    
    battery_state_service_unsubscribe();
    if(config_get(PERSIST_KEY_BATTERY)) {
        battery_state_service_subscribe(batt_handler);
    }
  
    accel_tap_service_unsubscribe();
    if (config_get(PERSIST_KEY_SECOND_TAP)) {
        accel_tap_service_subscribe(accel_tap_handler);
    }
    
    Layer *window_layer = window_get_root_layer(s_main_window);
    layer_remove_from_parent(text_layer_get_layer(s_day_in_month_layer));
    layer_remove_from_parent(text_layer_get_layer(s_weekday_layer));
    layer_remove_from_parent(text_layer_get_layer(s_month_layer));
    if(config_get(PERSIST_KEY_DATE)) {
        layer_add_child(window_layer, text_layer_get_layer(s_day_in_month_layer));
    }
    if(config_get(PERSIST_KEY_DAY)) {
        layer_add_child(window_layer, text_layer_get_layer(s_weekday_layer));
        layer_add_child(window_layer, text_layer_get_layer(s_month_layer));
    }
    layer_add_child(window_layer, s_canvas_layer);
}