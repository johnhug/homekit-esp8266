#include <stdlib.h>
#include <math.h>
#include <esp8266.h>
#include <FreeRTOS.h>
#include <task.h>
#include <wifi_config.h>
#include <espressif/esp_wifi.h>
#include <espressif/esp_system.h>
#include <homekit/homekit.h>

#define LED_INBUILT_GPIO 2

#define info(fmt, ...) printf("%s" fmt "\n", "homekit-esp8266: ", ## __VA_ARGS__);
#ifdef HOMEKIT_ESP8266_DEBUG 
#define debug(fmt, ...) info(fmt, ## __VA_ARGS__);
#define debug_heap(label) debug("Free Heap: %s: %d\n", label, xPortGetFreeHeapSize());
#else
#define debug(fmt, ...)
#define debug_heap(label)
#endif

// these need implemented in the project
void on_wifi_connected();
void on_homekit_verified();

void led_write(bool on) {
  gpio_write(LED_INBUILT_GPIO, on ? 0 : 1);
}

void blink_task(void *_pvParams) {
  uint8_t prefix = *(uint8_t *)_pvParams;
  uint8_t suffix = *(uint8_t *)(_pvParams + sizeof(uint8_t));

  // prefix blink (50ms cycle)
  for (uint8_t i = 0; i < prefix; i++) {
    led_write(true);
    vTaskDelay(50 / portTICK_PERIOD_MS);
    led_write(false);
    vTaskDelay(50 / portTICK_PERIOD_MS);
  }

  // suffix blink (200ms cycle)
  for (uint8_t i = 0; i < suffix; i++) {
    led_write(true);
    vTaskDelay(200 / portTICK_PERIOD_MS);
    led_write(false);
    vTaskDelay(200 / portTICK_PERIOD_MS);
  }

  vTaskDelete(NULL);
}

void blink_status(uint8_t prefix, uint8_t suffix) {
  uint8_t pvParams[2] = {prefix, suffix};
  xTaskCreate(blink_task, "BlinkemLed", configMINIMAL_STACK_SIZE, &pvParams, 2, NULL);
}

void identify(homekit_value_t _value) {
  blink_status(5, 5);
}

void init_blink() {
  gpio_enable(LED_INBUILT_GPIO, GPIO_OUTPUT);
}

void on_wifi_event(wifi_config_event_t event) {
  if (event == WIFI_CONFIG_CONNECTED) {
    on_wifi_connected();
  }
  else if (event == WIFI_CONFIG_DISCONNECTED) {
    sdk_system_restart();
  }
}

void on_homekit_event(homekit_event_t event) {
  info("on_homekit_event: %d", event);
  switch (event) {
    case HOMEKIT_EVENT_SERVER_INITIALIZED:
      break;
    case HOMEKIT_EVENT_CLIENT_CONNECTED:
      break;
    case HOMEKIT_EVENT_CLIENT_VERIFIED:
      // this gets called for both initial pairing as well as subsequent pair success
      // we want to delay initialization of further memory usage due to the initial pairing needing ~25k memory
      on_homekit_verified();
      break;
    case HOMEKIT_EVENT_CLIENT_DISCONNECTED:
      break;
    case HOMEKIT_EVENT_PAIRING_ADDED:
      break;
    case HOMEKIT_EVENT_PAIRING_REMOVED:
      break;
    default:
      break;
  }
}

void init_wifi(const char *ssid_prefix) {
  wifi_config_init2(ssid_prefix, NULL, on_wifi_event);
}

void get_device_name(const char *name, char **name_value) {
  uint8_t macaddr[6];
  sdk_wifi_get_macaddr(STATION_IF, macaddr);
  int name_len = sizeof(name) + 1 + 6 + 1;
  *name_value = malloc(name_len);
  snprintf(*name_value, name_len, "%s-%02X%02X%02X", name, macaddr[3], macaddr[4], macaddr[5]);
}

void common_init(const char *name, homekit_service_t *info_service) {
  debug_heap("start");
  init_blink();
  blink_status(1, 1);
  init_wifi(name);

  char *name_value = NULL;
  get_device_name(name, &name_value);
  info_service->characteristics[0]->value = HOMEKIT_STRING(name_value);
}

//int min(int a, int b) {
//  return a < b ? a : b;
//}

//int max(int a, int b) {
//  return a > b ? a : b;
//}

//int limit(int value, int lower, int upper) {
//  return min(max(value, lower), upper);
//}

float percent(int value) {
  return value / 100.0f;
}

int map(int value, int in_lower, int in_upper, int out_lower, int out_upper) {
  return out_lower + ((out_upper - out_lower) / (in_upper - in_lower)) * (value - in_lower);
}

//https://github.com/espressif/esp-idf/issues/83
double __ieee754_remainder(double x, double y) {
	return x - y * floor(x/y);
}

// https://en.wikipedia.org/wiki/HSL_and_HSV
float hsv_f(float n, uint16_t h, float s, float v) {
  float k = fmod(n + h/60.0f, 6);
  return v - v * s * fmax(fmin(fmin(k, 4-k), 1), 0);
}

void hs2rgb(uint16_t h, uint8_t s, uint8_t *r, uint8_t *g, uint8_t *b) {
  float v = 1;
  *r = 255 * hsv_f(5, h, s, v);
  *g = 255 * hsv_f(3, h, s, v);
  *b = 255 * hsv_f(1, h, s, v);
}
