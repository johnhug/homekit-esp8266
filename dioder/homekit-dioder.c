#include <stdio.h>
#include <stdlib.h>
#include <esp8266.h>
#include <FreeRTOS.h>
#include <homekit/homekit.h>
#include <homekit/characteristics.h>
#include <sysparam.h>
#include <multipwm.h>

#include <common.c>
#include <homekit_device.h>

#define WHITE_GPIO  4
#define RED_GPIO    13
#define GREEN_GPIO  14
#define BLUE_GPIO   12
#define PWM_MAX     1024

// pwm
bool initialized = false;
pwm_info_t pwm_info;

// Home Kit variables
bool color_on = true;
int color_hue = 0;
int color_saturation = 0;
int color_brightness = 100;
bool white_on = true;
int white_brightness = 100;

homekit_value_t color_on_get();
void color_on_set(homekit_value_t value);
homekit_value_t color_brightness_get();
void color_brightness_set(homekit_value_t value);
homekit_value_t color_hue_get();
void color_hue_set(homekit_value_t value);
homekit_value_t color_saturation_get();
void color_saturation_set(homekit_value_t value);
homekit_value_t white_on_get();
void white_on_set(homekit_value_t value);
homekit_value_t white_brightness_get();
void white_brightness_set(homekit_value_t value);
void on_homekit_event(homekit_event_t event);

homekit_service_t info_service = HOMEKIT_SERVICE_(
  ACCESSORY_INFORMATION,
  .id = 1,
  .characteristics = (homekit_characteristic_t*[]) {
    HOMEKIT_CHARACTERISTIC(NAME, HOMEKIT_NAME, .id = 2),
    HOMEKIT_CHARACTERISTIC(MANUFACTURER, "John Hug", .id = 3),
    HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, HOMEKIT_SERIAL, .id = 4),
    HOMEKIT_CHARACTERISTIC(MODEL, "Dioder-Dator", .id = 5),
    HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "1.0", .id = 6),
    HOMEKIT_CHARACTERISTIC(IDENTIFY, identify), // doesn't like .id =7
    NULL
  }
);
homekit_service_t color_service = HOMEKIT_SERVICE_(
  LIGHTBULB,
  .id = 8, 
  .primary = true,
  .characteristics = (homekit_characteristic_t*[]) {
    HOMEKIT_CHARACTERISTIC(NAME, "Color", .id = 9),
    HOMEKIT_CHARACTERISTIC(ON, true, .id = 10,
      .getter = color_on_get,
      .setter = color_on_set
    ),
    HOMEKIT_CHARACTERISTIC(BRIGHTNESS, 100, .id = 11,
      .getter = color_brightness_get,
      .setter = color_brightness_set
    ),
    HOMEKIT_CHARACTERISTIC(HUE, 0, .id = 12,
      .getter = color_hue_get,
      .setter = color_hue_set
    ),
    HOMEKIT_CHARACTERISTIC(SATURATION, 0, .id = 13,
      .getter = color_saturation_get,
      .setter = color_saturation_set
    ),
    NULL
  }
);
homekit_service_t white_service = HOMEKIT_SERVICE_(
  LIGHTBULB,
  .id = 14,
  .primary = true, 
  .characteristics = (homekit_characteristic_t*[]) {
    HOMEKIT_CHARACTERISTIC(NAME, "White", .id = 15),
    HOMEKIT_CHARACTERISTIC(ON, true, .id = 16,
      .getter = white_on_get,
      .setter = white_on_set
    ),
    HOMEKIT_CHARACTERISTIC(BRIGHTNESS, 100, .id = 17,
      .getter = white_brightness_get,
      .setter = white_brightness_set
    ),
    NULL
  }
);
homekit_server_config_t config = {
  .accessories = (homekit_accessory_t*[]) {
    HOMEKIT_ACCESSORY(
      .id = 1,
      .category = homekit_accessory_category_lightbulb,
      .services = (homekit_service_t*[]) {
        &info_service,
        &color_service,
        &white_service,
        NULL
      }
    ),
    NULL
  },
  .category = homekit_accessory_category_lightbulb,
  .password = HOMEKIT_PASSWORD,
  .setupId = HOMEKIT_SETUPID,
  .config_number = 1,
  .on_event = on_homekit_event
};

void update_pwm() {
  uint16_t white_pin_value = 0;
  uint16_t red_pin_value = 0;
  uint16_t green_pin_value = 0;
  uint16_t blue_pin_value = 0;

  if (white_on) {
    white_pin_value = map(white_brightness, 0, 100, 0, UINT16_MAX);
  }

  if (color_on) {
    uint8_t r, g, b;
    float dim = percent(color_brightness);

    hs2rgb(color_hue, percent(color_saturation), &r, &g, &b);

    red_pin_value = map(r * dim, 0, 255, 0, UINT16_MAX);
    green_pin_value = map(g * dim, 0, 255, 0, UINT16_MAX);
    blue_pin_value = map(b * dim, 0, 255, 0, UINT16_MAX);
  }
  
  multipwm_set_duty(&pwm_info, 0, white_pin_value);
  multipwm_set_duty(&pwm_info, 1, red_pin_value);
  multipwm_set_duty(&pwm_info, 2, green_pin_value);
  multipwm_set_duty(&pwm_info, 3, blue_pin_value);
}

void init_pwm() {
  initialized = true;
  debug_heap("init_pwm");
  blink_status(1, 3);
  pwm_info.channels = 4;

  multipwm_init(&pwm_info);
  multipwm_set_pin(&pwm_info, 0, WHITE_GPIO);
  multipwm_set_pin(&pwm_info, 1, RED_GPIO);
  multipwm_set_pin(&pwm_info, 2, GREEN_GPIO);
  multipwm_set_pin(&pwm_info, 3, BLUE_GPIO);
  multipwm_start(&pwm_info);
  
  update_pwm();
}

homekit_value_t color_on_get() {
  return HOMEKIT_BOOL(color_on);
}

void color_on_set(homekit_value_t value) {
  color_on = value.bool_value;
  sysparam_set_bool("dioder_color_on", color_on);

  update_pwm();
}

homekit_value_t color_brightness_get() {
  return HOMEKIT_INT(color_brightness);
}

void color_brightness_set(homekit_value_t value) {
  color_brightness = value.int_value;
  sysparam_set_int32("dioder_color_brightness", color_brightness);

  update_pwm();
}

homekit_value_t color_hue_get() {
  return HOMEKIT_FLOAT(color_hue);
}

void color_hue_set(homekit_value_t value) {
  color_hue = value.float_value;
  sysparam_set_int32("dioder_color_hue", color_hue);

  update_pwm();
}

homekit_value_t color_saturation_get() {
  return HOMEKIT_FLOAT(color_saturation);
}

void color_saturation_set(homekit_value_t value) {
  color_saturation = value.float_value;
  sysparam_set_int32("dioder_color_saturation", color_saturation);
  
  update_pwm();
}

homekit_value_t white_on_get() {
  return HOMEKIT_BOOL(white_on);
}

void white_on_set(homekit_value_t value) {
  white_on = value.bool_value;
  sysparam_set_bool("dioder_white_on", white_on);

  update_pwm();
}

homekit_value_t white_brightness_get() {
  return HOMEKIT_INT(white_brightness);
}

void white_brightness_set(homekit_value_t value) {
  white_brightness = value.int_value;
  sysparam_set_int32("dioder_white_brightness", white_brightness);

  update_pwm();
}

void on_wifi_connected() {
  debug_heap("before homekit");
  blink_status(1, 2);
  homekit_server_init(&config);
}

void on_homekit_verified() {
  if (!initialized) init_pwm();
}

void user_init(void) {
  common_init(HOMEKIT_NAME, &info_service);

  info("Checking for stored state...");
  char *dioder_version = NULL;
  sysparam_get_string("dioder_version", &dioder_version);
  if (dioder_version) {
    sysparam_get_bool("dioder_color_on", &color_on);
    sysparam_get_int32("dioder_color_brightness", &color_brightness);
    sysparam_get_int32("dioder_color_hue", &color_hue);
    sysparam_get_int32("dioder_color_saturation", &color_saturation);
    sysparam_get_bool("dioder_white_on", &white_on);
    sysparam_get_int32("dioder_white_brightness", &white_brightness);
    info("Found state:\n"
           "\tversion:%s\n"
           "\tcolor_on:%d\n"
           "\tcolor_brightness:%d\n"
           "\tcolor_hue:%d\n"
           "\tcolor_saturation:%d\n"
           "\twhite_on:%d\n"
           "\twhite_brightness:%d\n",
      dioder_version,
      color_on,
      color_brightness,
      color_hue,
      color_saturation,
      white_on,
      white_brightness
    );
  }
  else {
    info("Unable to find last stored state. Initializing...");
    sysparam_set_string("dioder_version", "1.0");
    sysparam_set_bool("dioder_color_on", color_on);
    sysparam_set_int32("dioder_color_brightness", color_brightness);
    sysparam_set_int32("dioder_color_hue", color_hue);
    sysparam_set_int32("dioder_color_saturation", color_saturation);
    sysparam_set_bool("dioder_white_on", white_on);
    sysparam_set_int32("dioder_white_brightness", white_brightness);
  }

  color_service.characteristics[1]->value = HOMEKIT_BOOL(color_on);
  color_service.characteristics[2]->value = HOMEKIT_INT(color_brightness);
  color_service.characteristics[3]->value = HOMEKIT_FLOAT(color_hue);
  color_service.characteristics[4]->value = HOMEKIT_FLOAT(color_saturation);
  white_service.characteristics[1]->value = HOMEKIT_BOOL(white_on);
  white_service.characteristics[2]->value = HOMEKIT_INT(white_brightness);
}
