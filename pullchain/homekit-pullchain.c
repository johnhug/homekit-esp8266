#include <stdio.h>
#include <stdlib.h>
#include <esp8266.h>
#include <FreeRTOS.h>
#include <task.h>
#include <homekit/homekit.h>
#include <homekit/characteristics.h>
#include <sysparam.h>

#include <common.c>
#include <homekit_device.h>

#define PULLCHAIN_GPIO  12  // D6
#define RELAY_GPIO      4   // D2
#define PULLCHAIN_PAUSE 100

uint32_t last_interrupt = 0;
bool last_pullchain = false;

// Home Kit variables
bool hk_on = false;

homekit_value_t on_get();
void on_set(homekit_value_t value);

homekit_service_t info_service = HOMEKIT_SERVICE_(
  ACCESSORY_INFORMATION,
  .id = 1,
  .characteristics = (homekit_characteristic_t*[]) {
    HOMEKIT_CHARACTERISTIC(NAME, HOMEKIT_NAME, .id = 2),
    HOMEKIT_CHARACTERISTIC(MANUFACTURER, "John Hug", .id = 3),
    HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, HOMEKIT_SERIAL, .id = 4),
    HOMEKIT_CHARACTERISTIC(MODEL, "Årstid-Assist", .id = 5),
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
    HOMEKIT_CHARACTERISTIC(NAME, "Light", .id = 9),
    HOMEKIT_CHARACTERISTIC(ON, false, .id = 10,
      .getter = on_get,
      .setter = on_set,
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

void updateRelay() {
  info("updateRelay: %d", hk_on);
  gpio_write(RELAY_GPIO, hk_on ? 1 : 0);
}

void gpio_intr_handler(uint8_t gpio_num) {
  uint32_t now = xTaskGetTickCountFromISR() * portTICK_PERIOD_MS;
  bool pullchain = gpio_read(PULLCHAIN_GPIO);
  if (last_interrupt < now - PULLCHAIN_PAUSE && pullchain != last_pullchain) {
    last_interrupt = now;
    info("pullthechain: %d", pullchain);
    last_pullchain = pullchain;
    hk_on = !hk_on;
    homekit_characteristic_notify(lightbulb_service.characteristics[1], HOMEKIT_BOOL(hk_on));
    updateRelay();
  }
}

homekit_value_t on_get() {
  return HOMEKIT_BOOL(hk_on);
}

void on_set(homekit_value_t value) {
  uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
  if (last_interrupt < now - PULLCHAIN_PAUSE) {
    last_interrupt = now;
    hk_on = value.bool_value;
    sysparam_set_bool("arstid_on", hk_on);
    updateRelay();
  }
}

void on_wifi_connected() {
  debug_heap("before homekit");
  blink_status(1, 2);
  homekit_server_init(&config);
}

void on_homekit_verified() {
  // nothing to do here, relays are too simple
}

void user_init(void) {
  common_init(HOMEKIT_NAME, &info_service);

  gpio_enable(RELAY_GPIO, GPIO_OUTPUT);
  gpio_enable(PULLCHAIN_GPIO, GPIO_INPUT);
  last_pullchain = gpio_read(PULLCHAIN_GPIO);
  gpio_set_interrupt(PULLCHAIN_GPIO, GPIO_INTTYPE_EDGE_ANY, gpio_intr_handler);

  info("Checking for stored state...");
  char *arstid_version = NULL;
  sysparam_get_string("arstid_version", &arstid_version);
  if (arstid_version) {
    sysparam_get_bool("arstid_on", &hk_on);
    info("Found state:\n"
           "\tversion:%s\n"
           "\ton:%d\n",
      arstid_version,
      hk_on
    );
  }
  else {
    info("Unable to find last stored state. Initializing...");
    sysparam_set_string("arstid_version", "1.0");
    sysparam_set_bool("arstid_on", hk_on);
  }

  lightbulb_service.characteristics[1]->value = HOMEKIT_BOOL(hk_on);

  updateRelay();
}
