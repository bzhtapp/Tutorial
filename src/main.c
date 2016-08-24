#include <pebble.h>

static Window *s_main_window;
static TextLayer *s_date_layer;
static TextLayer *s_time_layer;
static TextLayer *s_weather_layer;
static GFont s_weather_font;
static GFont s_battery_font;
static int s_battery_level;
static TextLayer *s_battery_layer;
static BitmapLayer *s_bt_icon_layer;
static GBitmap *s_bt_icon_bitmap;
static TextLayer *s_step_layer;
static GFont s_step_font;
const uint32_t s_samples_per_update = 5;
static uint16_t s_step_gForce;
const uint16_t s_step_min_diff = 400;
const uint16_t s_step_timeStamp_diff_min = 150;
const uint16_t s_step_threshold = 550;
const AccelSamplingRate s_acelSamplingRate = ACCEL_SAMPLING_10HZ;
uint32_t s_step_key = 1;
static time_t s_timeStamp;
static int s_timeStamp_diff;
static TextLayer *s_forecastHr3_layer;
static TextLayer *s_forecastDay2_layer;
static TextLayer *s_forecastDay3_layer;
static TextLayer *s_forecastCity_layer;
static uint16_t s_forecast_check;

static void update_step(int steps) {
  
  int step_count = persist_read_int(s_step_key);
    
  // Check to see if step variable has been initialized
  step_count += steps; //increment step
  
  persist_write_int(s_step_key, step_count);
  
  static char s_buffer[15];
  
  snprintf(s_buffer, sizeof(s_buffer), "Steps: %d", step_count);
  
  // Display this time on the TextLayer
  text_layer_set_text(s_step_layer, s_buffer);
}

static void accel_data_handler(AccelData *data, uint32_t num_samples) {
  
  // Read first sample gForce
  int16_t gForce = data[0].y;
  bool did_vibrate = data[0].did_vibrate;
  time_t timeStamp = data[0].timestamp;
  
  //default number of credited steps per event
  int steps = 1;
  
  //initialize static timeStamp
  if (!s_timeStamp) {
    s_timeStamp = timeStamp;
  }
  
  s_timeStamp_diff = timeStamp - s_timeStamp;
  
  //Count as a step if difference between last stored value and current value exceed minimum difference
  //and current gForce above threshold
  //and watch did not vibrate
  //and time different between previous sample more than 200 millisecond
  if ( abs(gForce - s_step_gForce) > s_step_min_diff
      && abs(gForce) > s_step_threshold 
     && !did_vibrate
     && s_timeStamp_diff > s_step_timeStamp_diff_min) 
  {
    update_step(steps); 
    s_timeStamp = timeStamp;
  }
  
  //store new current value
  s_step_gForce = gForce;

  
}

static void bluetooth_callback(bool connected) {
  // Show icon if disconnected
  layer_set_hidden(bitmap_layer_get_layer(s_bt_icon_layer), connected);

  if(!connected) {
    // Issue a vibrating alert
    vibes_double_pulse();
  }
}

  
static void battery_callback(BatteryChargeState state) {
  // Record the new battery level
  
  s_battery_level = state.charge_percent;
  
  static char s_buffer[5];
  snprintf(s_buffer, sizeof(s_buffer), "%d%%", s_battery_level);
  
  // Display this time on the TextLayer
  text_layer_set_text(s_battery_layer, s_buffer);
  
}

static void update_time() {
  // Get a tm structure
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);

  // Write the current hours and minutes into a buffer
  static char s_buffer[8];
  
  strftime(s_buffer, sizeof(s_buffer), clock_is_24h_style() ?
                                          "%H:%M" : "%I:%M", tick_time);

  // Display this time on the TextLayer
  text_layer_set_text(s_time_layer, s_buffer);
}

static void update_date() {
  // Get a tm structure
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);

  // Write the current date in like "Aug 08"
  static char s_buffer[15];
  
  strftime(s_buffer, sizeof(s_buffer), "%a %b %d", tick_time);

  // Display this time on the TextLayer
  text_layer_set_text(s_date_layer, s_buffer);
}


static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_time();
  update_date();
  
  // Reset step count at midnight 23:59
  if (tick_time->tm_hour == 23
      && tick_time->tm_min == 59) {
    persist_write_int(s_step_key, 0);
  }
  
  //store the previous minute value when forecast was first checked
  if (!s_forecast_check)
  {
    if (tick_time->tm_min == 0){
      s_forecast_check = 59;
    }
    else {
      s_forecast_check = tick_time->tm_min - 1;
    }
  }
  
  // Get weather update every half hour from the last check
  if( tick_time->tm_min % 30 == s_forecast_check % 30) { 
    // Begin dictionary
    DictionaryIterator *iter;
    app_message_outbox_begin(&iter);

    // Add a key-value pair
    dict_write_uint8(iter, 0, 0);

    // Send the message!
    app_message_outbox_send();
  }
  
}

static void main_window_load(Window *window) {
  // Get information about the Window
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  
  //Separate row pedometer is in into two columns
  int dividerStep = bounds.size.w * 3 / 4;
  
  //Track current Y position of bottom most layer as layers are being stack top to bottom
  int currentY = 0;
  
  //defines height of Step layer
  int heightStep = 14;
  
  // Create Pedometer Layer, GRect is (X,Y,Width,Height)
  s_step_layer = text_layer_create(
    GRect(0, currentY, dividerStep, heightStep));  
  //  GRect(0, PBL_IF_ROUND_ELSE(5, currentY), dividerStep, heightStep));
  
  s_step_font = fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD);
  text_layer_set_font(s_step_layer, s_step_font);
  
  text_layer_set_background_color(s_step_layer, GColorWhite);
  text_layer_set_text_color(s_step_layer, GColorBlack);
  text_layer_set_text_alignment(s_step_layer, GTextAlignmentLeft);
  
  layer_add_child(window_layer, text_layer_get_layer(s_step_layer));
  
    // Create battery meter Layer
  s_battery_layer = text_layer_create(
    GRect(dividerStep, currentY, bounds.size.w - dividerStep, heightStep));
  
  s_battery_font = fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD);
  text_layer_set_font(s_battery_layer, s_battery_font);
  
  text_layer_set_background_color(s_battery_layer, GColorWhite);
  text_layer_set_text_color(s_battery_layer, GColorBlack);
  text_layer_set_text_alignment(s_battery_layer, GTextAlignmentRight);

  layer_add_child(window_layer, text_layer_get_layer(s_battery_layer));
  currentY += heightStep; //current Y position is now at bottom of height Layer
  
  /*** End Step row ***/
  
  
  int heightDate = 30;
  
    // Create Date layer
  s_date_layer = text_layer_create(
      GRect(0, currentY, bounds.size.w, heightDate));

  // Improve the layout to be more like a watchface
  text_layer_set_background_color(s_date_layer, GColorWhite);
  text_layer_set_text_color(s_date_layer, GColorBlack);
  text_layer_set_font(s_date_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(s_date_layer, GTextAlignmentCenter);
  
  // Add it as a child layer to the Window's root layer
  layer_add_child(window_layer, text_layer_get_layer(s_date_layer));

  // Create the Bluetooth icon GBitmap
  s_bt_icon_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BT_ICON);

  // Create the BitmapLayer to display the GBitmap
  int btWidth = 30;
  int btX = bounds.size.w - btWidth; //place bluetooth bitmap on right side
  
  s_bt_icon_layer = bitmap_layer_create(GRect(btX, 15, btWidth, 30));
  bitmap_layer_set_bitmap(s_bt_icon_layer, s_bt_icon_bitmap);
  layer_add_child(window_get_root_layer(window), bitmap_layer_get_layer(s_bt_icon_layer));

  // Show the correct state of the BT connection from the start
  bluetooth_callback(connection_service_peek_pebble_app_connection());
  currentY += heightDate;

  /*** End Date row ***/
 
  // Create Time TextLayer with specific bounds
  int heightTime = 44;
  s_time_layer = text_layer_create(
      GRect(0, currentY, bounds.size.w, heightTime));

  // Improve the layout to be more like a watchface
  text_layer_set_background_color(s_time_layer, GColorWhite);
  text_layer_set_text_color(s_time_layer, GColorBlack);
  text_layer_set_font(s_time_layer, fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD));
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);
  
  layer_add_child(window_layer, text_layer_get_layer(s_time_layer));
  currentY += heightTime;
  
  /*** End Time Row ***/

  // Create weather Layer
  int heightWeather = 14;
  s_weather_layer = text_layer_create(
    GRect(0, currentY, bounds.size.w, heightWeather));

  // Style the text
  text_layer_set_background_color(s_weather_layer, GColorWhite);
  text_layer_set_text_color(s_weather_layer, GColorBlack);
  text_layer_set_text_alignment(s_weather_layer, GTextAlignmentCenter);
  text_layer_set_text(s_weather_layer, "Loading Weather ...");
  
  // Create second custom font, apply it and add to Window
  s_weather_font = fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD);
  text_layer_set_font(s_weather_layer, s_weather_font);
  
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_weather_layer));
  currentY += heightWeather;
  
  /*** End Weather Row ***/
  
  
  // Create forecast hour 3 row ***/
  int heightforecastHr3 = 14;
  s_forecastHr3_layer = text_layer_create(
    GRect(0, currentY, bounds.size.w, heightforecastHr3));

  // Style the text
  text_layer_set_background_color(s_forecastHr3_layer, GColorWhite);
  text_layer_set_text_color(s_forecastHr3_layer, GColorBlack);
  text_layer_set_text_alignment(s_forecastHr3_layer, GTextAlignmentCenter);
  text_layer_set_text(s_forecastHr3_layer, "Loading 3-hr Forecast...");
  text_layer_set_font(s_forecastHr3_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_forecastHr3_layer));
  currentY += heightforecastHr3;
  
  /*** End ForeCast 3 Hour row ***/
  
  // Create forecast Day 2 layer
  int heightforecastDay2 = 14;
  s_forecastDay2_layer = text_layer_create(
    GRect(0, currentY, bounds.size.w, heightforecastDay2));

  // Style the text
  text_layer_set_background_color(s_forecastDay2_layer, GColorWhite);
  text_layer_set_text_color(s_forecastDay2_layer, GColorBlack);
  text_layer_set_text_alignment(s_forecastDay2_layer, GTextAlignmentCenter);
  text_layer_set_text(s_forecastDay2_layer, "Loading morning forecast...");
  text_layer_set_font(s_forecastDay2_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_forecastDay2_layer));
  currentY += heightforecastDay2;
  
  // Create forecast Day 3 layer
  int heightforecastDay3 = 14;
  s_forecastDay3_layer = text_layer_create(
    GRect(0, currentY, bounds.size.w, heightforecastDay3));

  // Style the text
  text_layer_set_background_color(s_forecastDay3_layer, GColorWhite);
  text_layer_set_text_color(s_forecastDay3_layer, GColorBlack);
  text_layer_set_text_alignment(s_forecastDay3_layer, GTextAlignmentCenter);
  text_layer_set_text(s_forecastDay3_layer, "Loading evening forecast...");
  text_layer_set_font(s_forecastDay3_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_forecastDay3_layer));
  currentY += heightforecastDay3;
  
  int heightforecastCity = 14;
  s_forecastCity_layer = text_layer_create(
    GRect(0, currentY, bounds.size.w, heightforecastDay3));

  // Style the text
  text_layer_set_background_color(s_forecastCity_layer, GColorWhite);
  text_layer_set_text_color(s_forecastCity_layer, GColorBlack);
  text_layer_set_text_alignment(s_forecastCity_layer, GTextAlignmentCenter);
  text_layer_set_text(s_forecastCity_layer, "Loading city...");
  text_layer_set_font(s_forecastCity_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_forecastCity_layer));
  currentY += heightforecastCity;
}

static void main_window_unload(Window *window) {

  // Destroy Step Layer
  text_layer_destroy(s_step_layer);
  
  // Destroy battery layer
  text_layer_destroy(s_battery_layer);
  
  //destory bluetooth  layer and bit map
  gbitmap_destroy(s_bt_icon_bitmap);
  bitmap_layer_destroy(s_bt_icon_layer);
  
  //Destroy Date Layer
  text_layer_destroy(s_date_layer);
    
  // Destroy Time Layer
  text_layer_destroy(s_time_layer);
  
  // Destory WeatherLayer
  text_layer_destroy(s_weather_layer);
  
  // Destory ForecastHour3 Layer
  text_layer_destroy(s_forecastHr3_layer);
  
  // Destory ForecastDay2 Layer
  text_layer_destroy(s_forecastDay2_layer);
  
  // Destory ForecastDay3 Layer
  text_layer_destroy(s_forecastDay3_layer);
  
  // Destory ForecastCity Layer
  text_layer_destroy(s_forecastCity_layer);
  
}

static void inbox_received_callback(DictionaryIterator *iterator, void *context) {

  // Store incoming information
  static char temperature_buffer[8];
  static char conditions_buffer[32];
  static char weather_layer_buffer[32];
  static char forecastHr3_layer_buffer[32];
  static char forecastHr3_temperature_buffer[8];
  static char forecastHr3_conditions_buffer[32];
  static char forecastDay2_layer_buffer[32];
  static char forecastDay2_temperature_buffer[8];
  static char forecastDay2_conditions_buffer[32];
  static char forecastDay3_layer_buffer[32];
  static char forecastDay3_temperature_buffer[8];
  static char forecastDay3_conditions_buffer[32];
  static char forecastCity_layer_buffer[32];
  static char forecastCity_buffer[32];
  
  static char time_buffer[8];
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);
  
  // Read Weather tuples for data
  Tuple *temp_tuple = dict_find(iterator, MESSAGE_KEY_TEMPERATURE);
  Tuple *conditions_tuple = dict_find(iterator, MESSAGE_KEY_CONDITIONS);

  // If all Weather data is available, use it
  if(temp_tuple && conditions_tuple) {
    snprintf(temperature_buffer, sizeof(temperature_buffer), "%dF", (int)temp_tuple->value->int32);
    snprintf(conditions_buffer, sizeof(conditions_buffer), "%s", conditions_tuple->value->cstring);
    
    //Set time weather was last successfully update
    strftime(time_buffer, sizeof(time_buffer), clock_is_24h_style() ?
                                          "%H:%M" : "%I:%M", tick_time);
    
    //Update weather layer
    snprintf(weather_layer_buffer, sizeof(weather_layer_buffer), "%s %s @ %s", temperature_buffer, conditions_buffer, time_buffer);
    text_layer_set_text(s_weather_layer, weather_layer_buffer);
  }
  
  // Read ForeCast tuples for data
  Tuple *forecastHr3_temp_tuple = dict_find(iterator, MESSAGE_KEY_FORECAST_HR3_TEMPERATURE);
  Tuple *forecastHr3_conditions_tuple = dict_find(iterator, MESSAGE_KEY_FORECAST_HR3_CONDITIONS);

  // If all forecast data is available, use it
  if(forecastHr3_temp_tuple && forecastHr3_conditions_tuple) {
    snprintf(forecastHr3_temperature_buffer, sizeof(forecastHr3_temperature_buffer), "%dF", (int)forecastHr3_temp_tuple->value->int32);
    snprintf(forecastHr3_conditions_buffer, sizeof(forecastHr3_conditions_buffer), "%s", forecastHr3_conditions_tuple->value->cstring);
    
    //Update forecast hour 3 layer
    snprintf(forecastHr3_layer_buffer, sizeof(forecastHr3_layer_buffer), "%s, %s in 4 hrs", forecastHr3_temperature_buffer, forecastHr3_conditions_buffer);
    text_layer_set_text(s_forecastHr3_layer, forecastHr3_layer_buffer);
  }
  
  // Read ForeCast tuples for data
  Tuple *forecastDay2_temp_tuple = dict_find(iterator, MESSAGE_KEY_FORECAST_DAY2_TEMPERATURE);
  Tuple *forecastDay2_conditions_tuple = dict_find(iterator, MESSAGE_KEY_FORECAST_DAY2_CONDITIONS);

  // If all forecast data is available, use it
  if(forecastDay2_temp_tuple && forecastDay2_conditions_tuple) {
    snprintf(forecastDay2_temperature_buffer, sizeof(forecastDay2_temperature_buffer), "%dF", (int)forecastDay2_temp_tuple->value->int32);
    snprintf(forecastDay2_conditions_buffer, sizeof(forecastDay2_conditions_buffer), "%s", forecastDay2_conditions_tuple->value->cstring);
    
    //Update forecast hour 3 layer
    snprintf(forecastDay2_layer_buffer, sizeof(forecastDay2_layer_buffer), "%s %s nxt morning", forecastDay2_conditions_buffer, forecastDay2_temperature_buffer);
    text_layer_set_text(s_forecastDay2_layer, forecastDay2_layer_buffer);
  }
  
  // Read ForeCast Day3 tuples for data
  Tuple *forecastDay3_temp_tuple = dict_find(iterator, MESSAGE_KEY_FORECAST_DAY3_TEMPERATURE);
  Tuple *forecastDay3_conditions_tuple = dict_find(iterator, MESSAGE_KEY_FORECAST_DAY3_CONDITIONS);
  
  // If all forecast data is available, use it
  if(forecastDay3_temp_tuple && forecastDay3_conditions_tuple) {
    snprintf(forecastDay3_temperature_buffer, sizeof(forecastDay3_temperature_buffer), "%dF", (int)forecastDay3_temp_tuple->value->int32);
    snprintf(forecastDay3_conditions_buffer, sizeof(forecastDay3_conditions_buffer), "%s", forecastDay3_conditions_tuple->value->cstring);
    
    //Update forecast hour 3 layer
    snprintf(forecastDay3_layer_buffer, sizeof(forecastDay3_layer_buffer), "%s %s nxt evening", forecastDay3_conditions_buffer, forecastDay3_temperature_buffer);
    text_layer_set_text(s_forecastDay3_layer, forecastDay3_layer_buffer);
  }

  // Read ForeCast City tuples for data
  Tuple *forecastCity_tuple = dict_find(iterator, MESSAGE_KEY_FORECAST_CITY);
  
  // If all forecast data is available, use it
  if(forecastCity_tuple) {
    snprintf(forecastCity_buffer, sizeof(forecastCity_buffer), "%s", forecastCity_tuple->value->cstring);
    
    //Update forecast hour 3 layer
    snprintf(forecastCity_layer_buffer, sizeof(forecastCity_layer_buffer), "Forecast for %s", forecastCity_buffer);
    text_layer_set_text(s_forecastCity_layer, forecastCity_layer_buffer);
  }
}

static void inbox_dropped_callback(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped!");
}

static void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed!");
}

static void outbox_sent_callback(DictionaryIterator *iterator, void *context) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Outbox send success!");
}


static void init() {
  // Create main Window element and assign to pointer
  s_main_window = window_create();
  
  // Register callbacks
  app_message_register_inbox_received(inbox_received_callback);
  app_message_register_inbox_dropped(inbox_dropped_callback);
  app_message_register_outbox_failed(outbox_failed_callback);
  app_message_register_outbox_sent(outbox_sent_callback);

  // Open AppMessage
  const int inbox_size = 128;
  const int outbox_size = 128;
  app_message_open(inbox_size, outbox_size);

  // Set handlers to manage the elements inside the Window
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });

  // Show the Window on the watch, with animated=true
  window_stack_push(s_main_window, true);
  
  // Register with TickTimerService
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  
  // Make sure the time is displayed from the start
  update_time();
  
  //Make sure the date is displayed from the start
  update_date();
  
  //change accelerameter sampling rate
  accel_service_set_sampling_rate(s_acelSamplingRate);

  // Subscribe to batched data events (for step count, etc)
  accel_data_service_subscribe(s_samples_per_update, accel_data_handler);
  
  //Make sure the step count is displayed from the start
  update_step(0);
 
  // Ensure battery level is displayed from the start
  battery_callback(battery_state_service_peek());
  
  // Register for battery level updates
  battery_state_service_subscribe(battery_callback);
  
  // Register for Bluetooth connection updates
  connection_service_subscribe((ConnectionHandlers) {
    .pebble_app_connection_handler = bluetooth_callback
  });
  
  
}

static void deinit() {
  // Destroy Window
  window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}