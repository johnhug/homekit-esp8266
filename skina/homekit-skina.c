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

#define PWM_GPIO  4
#define PWM_MAX   1024

// pwm
bool initialized = false;
pwm_info_t pwm_info;

// Home Kit variables
bool hk_on = true;
uint8_t hk_brightness = 100;

homekit_value_t on_get();
void on_set(const homekit_value_t value);
homekit_value_t brightness_get();
void brightness_set(const homekit_value_t value);

homekit_service_t info_service = HOMEKIT_SERVICE_(
  ACCESSORY_INFORMATION,
  .id = 1,
  .characteristics = (homekit_characteristic_t*[]) {
    HOMEKIT_CHARACTERISTIC(NAME, HOMEKIT_NAME, .id = 2),
    HOMEKIT_CHARACTERISTIC(MANUFACTURER, "John Hug", .id = 3),
    HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, HOMEKIT_SERIAL, .id = 4),
    HOMEKIT_CHARACTERISTIC(MODEL, "Skina-Dimmer", .id = 5),
    HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "1.0", .id = 6),
    HOMEKIT_CHARACTERISTIC(IDENTIFY, identify), // doesn't like .id = 7
    NULL
  }
);
homekit_service_t lightbulb_service = HOMEKIT_SERVICE_(
  LIGHTBULB,
  .id = 8,
  .primary = true,
  .characteristics = (homekit_characteristic_t*[]) {
      HOMEKIT_CHARACTERISTIC(NAME, "Dragonflies", .id = 9),
      HOMEKIT_CHARACTERISTIC(ON, true, .id = 10,
        .getter = on_get,
        .setter = on_set
      ),
      HOMEKIT_CHARACTERISTIC(BRIGHTNESS, 100, .id = 11,
        .getter = brightness_get,
        .setter = brightness_set
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
        &lightbulb_service,
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
  uint16_t pwm_pin_value = 0;

  if (hk_on) {
    pwm_pin_value = map(hk_brightness, 0, 100, 0, UINT16_MAX);
  }

  multipwm_set_duty(&pwm_info, 0, pwm_pin_value);
}

void init_pwm() {
  initialized = true;
  debug_heap("init_pwm");
  blink_status(1, 3);
  pwm_info.channels = 1;

  multipwm_init(&pwm_info);
  multipwm_set_pin(&pwm_info, 0, PWM_GPIO);
  multipwm_start(&pwm_info);
  
  update_pwm();
}

homekit_value_t on_get() {
  return HOMEKIT_BOOL(hk_on);
}

void on_set(homekit_value_t value) {
  hk_on = value.bool_value;
  sysparam_set_bool("skina_on", hk_on);

  update_pwm();
}

homekit_value_t brightness_get() {
  return HOMEKIT_INT(hk_brightness);
}

void brightness_set(homekit_value_t value) {
  hk_brightness = value.int_value;
  sysparam_set_int8("skina_brightness", hk_brightness);

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
  char *skina_version = NULL;
  sysparam_get_string("skina_version", &skina_version);
  if (skina_version) {
    sysparam_get_bool("skina_on", &hk_on);
    sysparam_get_int8("skina_brightness", &hk_brightness);
    info("Found state:\n"
           "\tversion:%s\n"
           "\ton:%d\n"
           "\tbrightness:%d\n",
      skina_version,
      hk_on,
      hk_brightness
    );
  }
  else {
    info("Unable to find last stored state. Initializing...");
    sysparam_set_string("skina_version", "1.0");
    sysparam_set_bool("skina_on", hk_on);
    sysparam_set_int8("skina_brightness", hk_brightness);
  }

  lightbulb_service.characteristics[1]->value = HOMEKIT_BOOL(hk_on);
  lightbulb_service.characteristics[2]->value = HOMEKIT_INT(hk_brightness);
}
