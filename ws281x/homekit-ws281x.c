#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <esp8266.h>
#include <espressif/esp_system.h>
#include <FreeRTOS.h>
#include <task.h>
#include <homekit/homekit.h>
#include <homekit/characteristics.h>
#include <sysparam.h>
#include <ws2812_i2s/ws2812_i2s.h>

#include <common.c>
#include <homekit_device.h>

#define UUID_MODE       "14174570-FDD6-40C0-AA03-7638FCA6B9F0"
#define UUID_SPEED      "14174571-FDD6-40C0-AA03-7638FCA6B9F0"
#define UUID_REVERSE    "14174572-FDD6-40C0-AA03-7638FCA6B9F0"
#define UUID_DENSITY    "14174573-FDD6-40C0-AA03-7638FCA6B9F0"
#define UUID_FADE       "14174574-FDD6-40C0-AA03-7638FCA6B9F0"
#define UUID_LED_COUNT  "14174575-FDD6-40C0-AA03-7638FCA6B9F0"
#define UUID_DATA_ORDER "14174576-FDD6-40C0-AA03-7638FCA6B9F0"

typedef enum {
  MD_SOLID,     // solid color
  MD_CHASE,     // static colors with dimming chase
  MD_TWINKLE,   // static colors with twinkle dimming
  MD_SEQUENCE,  // sequence of colors 1 pixel each
  MD_STRIPES,   // stripes of colors density variable pixels each
  MD_COMETS,    // comets of colors
  MD_FIREWORKS, // random pixels light up with one of the colors and fade
  MD_CYCLE      // cycle through enabled colors like solid fading between
} animation_mode;

typedef enum {
  OT_GRB,       // ws2812, ws2813, ws2815
  OT_RGB        // ws2811
} color_order;

typedef struct {
  uint8_t red;
  uint8_t green;
  uint8_t blue;
} rgb_t;

typedef struct {
  rgb_t rgb;
  float brightness;
  bool increasing;
} working_pixel_t;

uint8_t colors_enabled;
rgb_t *colors;
working_pixel_t *working_pixels;
uint8_t position = -1;
uint8_t color_index = 0;

// Home Kit variables
bool    hk_strip_on             = true;
int8_t  hk_strip_brightness     = 100;
int8_t  hk_colors_service_id[]  = {  20,   25,   30,   35,   40,   45,   50};
bool    hk_colors_on[]          = {true, true, true, true, true, true, true};
int16_t hk_colors_hue[]         = {   0,  240,  120,  360,  180,   60,  300};
int8_t  hk_colors_saturation[]  = {   0,  100,  100,  100,  100,  100,  100};
int8_t  hk_mode                 = MD_SOLID;
int8_t  hk_speed                = 100;
bool    hk_reverse              = false;
int8_t  hk_density              = 25;
int8_t  hk_fade                 = 50;
int16_t hk_led_count            = 300;
int8_t  hk_data_order           = OT_GRB;

homekit_value_t led_on_get();
void led_on_set(const homekit_value_t value);
homekit_value_t led_brightness_get();
void led_brightness_set(const homekit_value_t value);
homekit_value_t led_color_on_get_ex(const homekit_characteristic_t *ch);
void led_color_on_set_ex(homekit_characteristic_t *ch, const homekit_value_t value);
homekit_value_t led_color_hue_get_ex(const homekit_characteristic_t *ch);
void led_color_hue_set_ex(homekit_characteristic_t *ch, const homekit_value_t value);
homekit_value_t led_color_saturation_get_ex(const homekit_characteristic_t *ch);
void led_color_saturation_set_ex(homekit_characteristic_t *ch, const homekit_value_t value);
homekit_value_t led_mode_get();
void led_mode_set(const homekit_value_t value);
homekit_value_t led_mode_name_get();
homekit_value_t led_speed_get();
void led_speed_set(const homekit_value_t value);
homekit_value_t led_reverse_get();
void led_reverse_set(const homekit_value_t value);
homekit_value_t led_density_get();
void led_density_set(const homekit_value_t value);
homekit_value_t led_fade_get();
void led_fade_set(const homekit_value_t value);
homekit_value_t led_count_get();
void led_count_set(const homekit_value_t value);
homekit_value_t led_data_order_get();
void led_data_order_set(const homekit_value_t value);
void on_homekit_event(homekit_event_t event);

homekit_service_t info_service = HOMEKIT_SERVICE_(
  ACCESSORY_INFORMATION,
  .id = 1,
  .characteristics = (homekit_characteristic_t*[]) {
    HOMEKIT_CHARACTERISTIC(NAME, HOMEKIT_NAME, .id = 2),
    HOMEKIT_CHARACTERISTIC(MANUFACTURER, "John Hug", .id = 3),
    HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, HOMEKIT_SERIAL, .id = 4),
    HOMEKIT_CHARACTERISTIC(MODEL, "WS281x-FX", .id = 5),
    HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "1.0", .id = 6),
    HOMEKIT_CHARACTERISTIC(IDENTIFY, identify), // doesn't like .id = 7
    NULL
  }
);
homekit_service_t color0_service = HOMEKIT_SERVICE_(
  LIGHTBULB,
  .id = 20,
  .primary = false,
  .characteristics = (homekit_characteristic_t*[]) {
    HOMEKIT_CHARACTERISTIC(NAME, "Color 0", .id = 21),
    HOMEKIT_CHARACTERISTIC(ON, true, .id = 22,
      .getter_ex = led_color_on_get_ex,
      .setter_ex = led_color_on_set_ex
    ),
    HOMEKIT_CHARACTERISTIC(HUE, 0, .id = 23,
      .getter_ex = led_color_hue_get_ex,
      .setter_ex = led_color_hue_set_ex
    ),
    HOMEKIT_CHARACTERISTIC(SATURATION, 0, .id = 24,
      .getter_ex = led_color_saturation_get_ex,
      .setter_ex = led_color_saturation_set_ex
    ),
    NULL
  }
);
homekit_service_t color1_service = HOMEKIT_SERVICE_(
  LIGHTBULB,
  .id = 25,
  .primary = false,
  .characteristics = (homekit_characteristic_t*[]) {
    HOMEKIT_CHARACTERISTIC(NAME, "Color 1", .id = 26),
    HOMEKIT_CHARACTERISTIC(ON, true, .id = 27,
      .getter_ex = led_color_on_get_ex,
      .setter_ex = led_color_on_set_ex
    ),
    HOMEKIT_CHARACTERISTIC(HUE, 240, .id = 28,
      .getter_ex = led_color_hue_get_ex,
      .setter_ex = led_color_hue_set_ex
    ),
    HOMEKIT_CHARACTERISTIC(SATURATION, 100, .id = 29,
      .getter_ex = led_color_saturation_get_ex,
      .setter_ex = led_color_saturation_set_ex
    ),
    NULL
  }
);
homekit_service_t color2_service = HOMEKIT_SERVICE_(
  LIGHTBULB,
  .id = 30,
  .primary = false,
  .characteristics = (homekit_characteristic_t*[]) {
    HOMEKIT_CHARACTERISTIC(NAME, "Color 2", .id = 31),
    HOMEKIT_CHARACTERISTIC(ON, true, .id = 32,
      .getter_ex = led_color_on_get_ex,
      .setter_ex = led_color_on_set_ex
    ),
    HOMEKIT_CHARACTERISTIC(HUE, 120, .id = 33,
      .getter_ex = led_color_hue_get_ex,
      .setter_ex = led_color_hue_set_ex
    ),
    HOMEKIT_CHARACTERISTIC(SATURATION, 100, .id = 34,
      .getter_ex = led_color_saturation_get_ex,
      .setter_ex = led_color_saturation_set_ex
    ),
    NULL
  }
);
homekit_service_t color3_service = HOMEKIT_SERVICE_(
  LIGHTBULB,
  .id = 35,
  .primary = false,
  .characteristics = (homekit_characteristic_t*[]) {
    HOMEKIT_CHARACTERISTIC(NAME, "Color 3", .id = 36),
    HOMEKIT_CHARACTERISTIC(ON, true, .id = 37,
      .getter_ex = led_color_on_get_ex,
      .setter_ex = led_color_on_set_ex
    ),
    HOMEKIT_CHARACTERISTIC(HUE, 360, .id = 38,
      .getter_ex = led_color_hue_get_ex,
      .setter_ex = led_color_hue_set_ex
    ),
    HOMEKIT_CHARACTERISTIC(SATURATION, 100, .id = 39,
      .getter_ex = led_color_saturation_get_ex,
      .setter_ex = led_color_saturation_set_ex
    ),
    NULL
  }
);
homekit_service_t color4_service = HOMEKIT_SERVICE_(
  LIGHTBULB,
  .id = 40,
  .primary = false,
  .characteristics = (homekit_characteristic_t*[]) {
    HOMEKIT_CHARACTERISTIC(NAME, "Color 4", .id = 41),
    HOMEKIT_CHARACTERISTIC(ON, true, .id = 42,
      .getter_ex = led_color_on_get_ex,
      .setter_ex = led_color_on_set_ex
    ),
    HOMEKIT_CHARACTERISTIC(HUE, 180, .id = 43,
      .getter_ex = led_color_hue_get_ex,
      .setter_ex = led_color_hue_set_ex
    ),
    HOMEKIT_CHARACTERISTIC(SATURATION, 100, .id = 44,
      .getter_ex = led_color_saturation_get_ex,
      .setter_ex = led_color_saturation_set_ex
    ),
    NULL
  }
);
homekit_service_t color5_service = HOMEKIT_SERVICE_(
  LIGHTBULB,
  .id = 45,
  .primary = false,
  .characteristics = (homekit_characteristic_t*[]) {
    HOMEKIT_CHARACTERISTIC(NAME, "Color 5", .id = 46),
    HOMEKIT_CHARACTERISTIC(ON, true, .id = 47,
      .getter_ex = led_color_on_get_ex,
      .setter_ex = led_color_on_set_ex
    ),
    HOMEKIT_CHARACTERISTIC(HUE, 60, .id = 48,
      .getter_ex = led_color_hue_get_ex,
      .setter_ex = led_color_hue_set_ex
    ),
    HOMEKIT_CHARACTERISTIC(SATURATION, 100, .id = 49,
      .getter_ex = led_color_saturation_get_ex,
      .setter_ex = led_color_saturation_set_ex
    ),
    NULL
  }
);
homekit_service_t color6_service = HOMEKIT_SERVICE_(
  LIGHTBULB,
  .id = 50,
  .primary = false,
  .characteristics = (homekit_characteristic_t*[]) {
    HOMEKIT_CHARACTERISTIC(NAME, "Color 6", .id = 51),
    HOMEKIT_CHARACTERISTIC(ON, true, .id = 52,
      .getter_ex = led_color_on_get_ex,
      .setter_ex = led_color_on_set_ex
    ),
    HOMEKIT_CHARACTERISTIC(HUE, 300, .id = 53,
      .getter_ex = led_color_hue_get_ex,
      .setter_ex = led_color_hue_set_ex
    ),
    HOMEKIT_CHARACTERISTIC(SATURATION, 100, .id = 54,
      .getter_ex = led_color_saturation_get_ex,
      .setter_ex = led_color_saturation_set_ex
    ),
    NULL
  }
);
homekit_service_t control_service = HOMEKIT_SERVICE_(
  LIGHTBULB,
  .id = 8, 
  .primary = true,
  .characteristics = (homekit_characteristic_t*[]) {
    HOMEKIT_CHARACTERISTIC(NAME, "Control", .id = 9),
    HOMEKIT_CHARACTERISTIC(ON, true, .id = 10,
      .getter = led_on_get,
      .setter = led_on_set
    ),
    HOMEKIT_CHARACTERISTIC(BRIGHTNESS, 100, .id = 11,
      .getter = led_brightness_get,
      .setter = led_brightness_set
    ),
    HOMEKIT_CHARACTERISTIC(CUSTOM, .type = UUID_MODE, .id = 12,
      .description = "FX Mode (0-Solid, 1-Chase, 2-Twinkle, 3-Sequence, 4-Stripes, 5-Comets, 6-Fireworks, 7-Cycle)",
      .format = homekit_format_int,
      .permissions = homekit_permissions_paired_read 
                   | homekit_permissions_paired_write,
      .min_value = (float[]) {0},
      .max_value = (float[]) {7},
      .min_step = (float[]) {1},
      .value = HOMEKIT_INT_(MD_SOLID),
      .getter = led_mode_get,
      .setter = led_mode_set
    ),
    HOMEKIT_CHARACTERISTIC(CUSTOM, .type = UUID_SPEED, .id = 14,
      .description = "Speed",
      .format = homekit_format_int,
      .permissions = homekit_permissions_paired_read
                   | homekit_permissions_paired_write,
      .min_value = (float[]) {0},
      .max_value = (float[]) {100},
      .min_step = (float[]) {1},
      .value = HOMEKIT_INT_(100),
      .getter = led_speed_get,
      .setter = led_speed_set
    ),
    HOMEKIT_CHARACTERISTIC(CUSTOM, .type = UUID_REVERSE, .id = 15,
      .description = "Reverse",
      .format = homekit_format_bool,
      .permissions = homekit_permissions_paired_read
                   | homekit_permissions_paired_write,
      .value = HOMEKIT_BOOL_(false),
      .getter = led_reverse_get,
      .setter = led_reverse_set
    ),
    HOMEKIT_CHARACTERISTIC(CUSTOM, .type = UUID_DENSITY, .id = 16,
      .description = "Density",
      .format = homekit_format_int,
      .permissions = homekit_permissions_paired_read
                   | homekit_permissions_paired_write,
      .min_value = (float[]) {1},
      .max_value = (float[]) {100},
      .min_step = (float[]) {1},
      .value = HOMEKIT_INT_(25),
      .getter = led_density_get,
      .setter = led_density_set
    ),
    HOMEKIT_CHARACTERISTIC(CUSTOM, .type = UUID_FADE, .id = 17,
      .description = "Fade",
      .format = homekit_format_int,
      .permissions = homekit_permissions_paired_read
                   | homekit_permissions_paired_write,
      .min_value = (float[]) {1},
      .max_value = (float[]) {100},
      .min_step = (float[]) {1},
      .value = HOMEKIT_INT_(50),
      .getter = led_fade_get,
      .setter = led_fade_set
    ),
    HOMEKIT_CHARACTERISTIC(CUSTOM, .type = UUID_LED_COUNT, .id = 18,
      .description = "LED Count (Reboots on change)",
      .format = homekit_format_int,
      .permissions = homekit_permissions_paired_read
                   | homekit_permissions_paired_write,
      .min_value = (float[]) {1},
      .max_value = (float[]) {9999},
      .min_step = (float[]) {1},
      .value = HOMEKIT_INT_(300),
      .getter = led_count_get,
      .setter = led_count_set
    ),
    HOMEKIT_CHARACTERISTIC(CUSTOM, .type = UUID_DATA_ORDER, .id = 19,
      .description = "Strip Data Order (0-GRB, 1-RGB)",
      .format = homekit_format_int, 
      .permissions = homekit_permissions_paired_read
                   | homekit_permissions_paired_write,
      .min_value = (float[]) {0},
      .max_value = (float[]) {1},
      .min_step = (float[]) {1},
      .value = HOMEKIT_INT_(OT_GRB),
      .getter = led_data_order_get,
      .setter = led_data_order_set
    ),
    NULL
  },
  .linked = (homekit_service_t*[]) {
    &color0_service,
    &color1_service,
    &color2_service,
    &color3_service,
    &color4_service,
    &color5_service,
    &color6_service,
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
        &control_service,
        &color0_service,
        &color1_service,
        &color2_service,
        &color3_service,
        &color4_service,
        &color5_service,
        &color6_service,
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

uint8_t find_color_index(const homekit_characteristic_t *ch) {
  for (uint8_t i = 0; i < 7; i++) {
    // allowing each service to occupy a 5 id space
    uint8_t min = hk_colors_service_id[i];
    uint8_t max = hk_colors_service_id[i] + 5;
    if (ch->id > min && ch->id < max) return i;
  }
  return 0;  
}

char* color_key(uint8_t number, char* suffix) {
  uint8_t len = 14 + sizeof(suffix);
  char *key_name = malloc(len);
  snprintf(key_name, len, "ws281x_color%d_%s", number, suffix);
  return key_name;
}

void updateColors() {
  uint8_t tmp_colors_enabled = 0;
  for (uint8_t i = 0; i < 7; i++) {
    if (hk_colors_on[i]) tmp_colors_enabled++;
  }
  info("updateColors: color_count: %d", tmp_colors_enabled);
  rgb_t *tmp_colors = (rgb_t*) malloc(tmp_colors_enabled * sizeof(rgb_t));
  
  uint8_t color_idx = 0;
  for (uint8_t i = 0; i < 7; i++) {
    if (hk_colors_on[i]) {
      hs2rgb(hk_colors_hue[i],
             percent(hk_colors_saturation[i]),
             &tmp_colors[color_idx].red,
             &tmp_colors[color_idx].green,
             &tmp_colors[color_idx].blue
            );
      info("updateColors: color: %d %02x%02x%02x", 
        color_idx, tmp_colors[color_idx].red, tmp_colors[color_idx].green, tmp_colors[color_idx].blue);
      color_idx++;
    }
  }
  rgb_t *old = colors;
  colors_enabled = tmp_colors_enabled;
  colors = tmp_colors;
  free(old);
}

void setPixel(uint16_t index, rgb_t color, float brightnessMod) {
  working_pixel_t *wp = &working_pixels[index];
  wp->rgb = color;
  wp->brightness = brightnessMod;
  wp->increasing = false;
}

void fadePixel(uint16_t index, float brightnessMod) {
  working_pixel_t *wp = &working_pixels[index];
  if (wp->increasing) brightnessMod = 1.0f / brightnessMod;
  float tmp_brt = wp->brightness * brightnessMod;
  if (tmp_brt > 1.0f) tmp_brt = 1.0f;
  else if (tmp_brt < (1.0f - percent(hk_fade)) * 0.25f) tmp_brt = 0.0f;
  wp->brightness = tmp_brt;
}

void updatePixels() {
  ws2812_pixel_t *pixel_buffer = (ws2812_pixel_t*) malloc(hk_led_count * sizeof(ws2812_pixel_t));
  for (uint16_t i = 0; i < hk_led_count; i++) {
    uint16_t targetIndex = hk_reverse ? (hk_led_count - 1) - i : i;
    working_pixel_t wp = working_pixels[i];
    ws2812_pixel_t *p = &pixel_buffer[targetIndex];
    if (hk_data_order == OT_RGB) {
      // underlying library assumes ws2812 which is GRB bit ordering
      p->green = wp.rgb.red;
      p->red = wp.rgb.green;
      p->blue = wp.rgb.blue;
    }		
    else {
      // default GRB bit ordering
      p->green = wp.rgb.green;
      p->red = wp.rgb.red;
      p->blue = wp.rgb.blue;
    }
    float adjust = wp.brightness * percent(hk_strip_brightness);
    p->green *= adjust;
    p->red *= adjust;
    p->blue *= adjust;
  }
  ws2812_i2s_update(pixel_buffer, PIXEL_RGB);
  free(pixel_buffer);
}

void solid() {
  for (uint16_t i = 0; i < hk_led_count; i++) {
    setPixel(i, colors[0], 1.0f);
  }
  updatePixels();
}

uint8_t mix(uint8_t a, uint8_t b, float ratio) {
  return a + (b - a) * ratio;
}

void cycle() {
  position = (position + 1) % hk_density;
  float secondary = 0;
  uint8_t partial_density = hk_density * percent(hk_fade);
  if (position > partial_density) {
    secondary = (float)(position - partial_density) / partial_density;
  }
  if (position == 0) color_index = (color_index + 1) % colors_enabled;
  uint8_t next = (color_index + 1) % colors_enabled;
  rgb_t color;
  color.red = mix(colors[color_index].red, colors[next].red, secondary);
  color.green = mix(colors[color_index].green, colors[next].green, secondary);
  color.blue = mix(colors[color_index].blue, colors[next].blue, secondary);
  debug("cycle: index: %d next: %d mix: %.2f color: %02x%02x%02x", color_index, next, secondary, color.red, color.green, color.blue);
  for (uint16_t i = 0; i < hk_led_count; i++) {
    setPixel(i, color, 1.0f);
  }
  updatePixels();
}

void chase() {
  float fade_step = (1.0f - percent(hk_fade)) * 0.25f;
  position = (position + 1) % colors_enabled;
  for (uint16_t i = 0; i < hk_led_count; i++) {
    uint16_t mod = i % colors_enabled;
    setPixel(i, colors[i % colors_enabled], 1 / pow(2, floor(abs(position - mod) / (colors_enabled * fade_step))));
  }
  updatePixels();
}

void twinkle() {
  float fade_step = (1.0f - percent(hk_fade)) * 0.25f;
  for (uint16_t index = 0; index < hk_led_count; index++) {
    if (working_pixels[index].brightness == 1.0f) {
      working_pixels[index].increasing = false;
    }
    fadePixel(index, 1.0f - percent(hk_fade));
  }
  for (uint16_t i = hk_led_count * percent(hk_density); i > 0; i--) {
    uint16_t index = rand() % hk_led_count;
    if (working_pixels[index].brightness == 0.0f) {
      setPixel(index, colors[index % colors_enabled], fade_step);
      working_pixels[index].increasing = true;
    }
  }
  updatePixels();
}

void rotation(uint16_t width) {
  position = (position + 1) % (colors_enabled * width);
  uint16_t colorIndex = (position / width) % colors_enabled;
  for (uint16_t i = 0; i < hk_led_count; i++) {
    uint16_t mod = (i + position) % width;
    if (mod == 0 && i > 0) colorIndex = (colorIndex + 1) % colors_enabled;

    setPixel(i, colors[colorIndex], 1.0f);
  }
  updatePixels();
}

void comets() {
  float fade_step = (1.0f - percent(hk_fade)) * 0.25f;
  uint16_t width = hk_led_count * percent(hk_density);
  position = (position + 1) % (colors_enabled * width);
  uint16_t colorIndex = (position / width) % colors_enabled;
  for (uint16_t i = 0; i < hk_led_count; i++) {
    uint16_t mod = (i + position) % width;
    if (mod == 0 && i > 0) colorIndex = (colorIndex + 1) % colors_enabled;
    if (mod == 0) {
      setPixel(i, (rgb_t) {255,255,255}, 1.0f); // white
    }
    else {
      setPixel(i, colors[colorIndex], 1 / pow(2, floor(mod / (width * fade_step))));
    }
  }
  updatePixels();
}

void fireworks() {
  for (uint16_t i = 0; i < hk_led_count; i++) {
    fadePixel(i, 1.0f - percent(hk_fade));
  }
  for (uint16_t i = hk_led_count * percent(hk_density); i > 0; i--) {
    uint16_t index = rand() % hk_led_count;
    setPixel(index, colors[rand() % colors_enabled], 1.0f);
  }
  updatePixels();
}

void ws281x_service(void *_args) {
  info("ws281x_service: starting service task...");
  blink_status(1, 4);
  uint32_t last_call_time = 0;

  while (true) {
    if (hk_strip_on && colors_enabled > 0) {
      // max delay 250ms, min delay 1ms (min due to while pause)
      float delay = hk_speed * -2.5f + 250;
      float delay_factor = 1.0f;
      uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
      
      if (now - last_call_time > delay * delay_factor) {
        last_call_time = now;

        switch (hk_mode) {
          case MD_SOLID:
            delay_factor = 1.0;
            solid();
            break;
          case MD_CHASE:
            delay_factor = 2.0;
            chase();
            break;
          case MD_TWINKLE:
            delay_factor = 2.0;
            twinkle();
            break;
          case MD_SEQUENCE:
            delay_factor = 1.5f;
            rotation(1);
            break;
          case MD_STRIPES:
            delay_factor = 1.0;
            rotation(hk_led_count * percent(hk_density));
            break;
          case MD_COMETS:
            delay_factor = 2.0;
            comets();
            break;
          case MD_FIREWORKS:
            delay_factor = 2.0;
            fireworks();
            break;
          case MD_CYCLE:
            delay_factor = 1.0;
            cycle();
            break;
          default:
            solid();
        }
      }
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
  info("ws281x_service: exiting service task");
}

void init_ws281x() {
  debug_heap("begin init_ws281x");
  blink_status(1, 3);

  working_pixels = (working_pixel_t*) malloc(hk_led_count * sizeof(working_pixel_t));

  ws2812_i2s_init(hk_led_count, PIXEL_RGB);

  debug_heap("after ws2812_i2s_init");

  updateColors();

  xTaskCreate(ws281x_service, "ws281xService", 256, NULL, 2, NULL);
}

homekit_value_t led_on_get() {
  return HOMEKIT_BOOL(hk_strip_on);
}

void led_on_set(const homekit_value_t value) {
  if (hk_strip_on != value.bool_value) {
    hk_strip_on = value.bool_value;
    sysparam_set_bool("ws281x_on", hk_strip_on);

    if (!hk_strip_on) {
      // black out
      for (uint16_t i = 0; i < hk_led_count; i++) {
        setPixel(i, (rgb_t) {0,0,0}, 1.0f);
      }
      updatePixels();
    }

    // notify all the colors due to compound state
    homekit_characteristic_notify(color0_service.characteristics[1], HOMEKIT_BOOL(hk_strip_on && hk_colors_on[0]));
    homekit_characteristic_notify(color1_service.characteristics[1], HOMEKIT_BOOL(hk_strip_on && hk_colors_on[1]));
    homekit_characteristic_notify(color2_service.characteristics[1], HOMEKIT_BOOL(hk_strip_on && hk_colors_on[2]));
    homekit_characteristic_notify(color3_service.characteristics[1], HOMEKIT_BOOL(hk_strip_on && hk_colors_on[3]));
    homekit_characteristic_notify(color4_service.characteristics[1], HOMEKIT_BOOL(hk_strip_on && hk_colors_on[4]));
    homekit_characteristic_notify(color5_service.characteristics[1], HOMEKIT_BOOL(hk_strip_on && hk_colors_on[5]));
    homekit_characteristic_notify(color6_service.characteristics[1], HOMEKIT_BOOL(hk_strip_on && hk_colors_on[6]));
  }
}

homekit_value_t led_brightness_get() {
  return HOMEKIT_INT(hk_strip_brightness);
}

void led_brightness_set(homekit_value_t value) {
  hk_strip_brightness = value.int_value;
  sysparam_set_int8("ws281x_brightness", hk_strip_brightness);
}

homekit_value_t led_color_on_get_ex(const homekit_characteristic_t *ch) {
  uint8_t index = find_color_index(ch);
  return HOMEKIT_BOOL(hk_strip_on && hk_colors_on[index]);
}

void led_color_on_set_ex(homekit_characteristic_t *ch, const homekit_value_t value) {
  uint8_t index = find_color_index(ch);
  hk_colors_on[index] = value.bool_value;
  char* key_name = color_key(index, "on");
  sysparam_set_bool(key_name, hk_colors_on[index]);
  free(key_name);
  updateColors();
}

homekit_value_t led_color_hue_get_ex(const homekit_characteristic_t *ch) {
  uint8_t index = find_color_index(ch);
  return HOMEKIT_FLOAT(hk_colors_hue[index]);
}

void led_color_hue_set_ex(homekit_characteristic_t *ch, const homekit_value_t value) {
  uint8_t index = find_color_index(ch);
  hk_colors_hue[index] = value.float_value;
  char* key_name = color_key(index, "hue");
  sysparam_set_int32(key_name, hk_colors_hue[index]);
  free(key_name);
  updateColors();
}

homekit_value_t led_color_saturation_get_ex(const homekit_characteristic_t *ch) {
  uint8_t index = find_color_index(ch);
  return HOMEKIT_FLOAT(hk_colors_saturation[index]);
}

void led_color_saturation_set_ex(homekit_characteristic_t *ch, const homekit_value_t value) {
  uint8_t index = find_color_index(ch);
  hk_colors_saturation[index] = value.float_value;
  char* key_name = color_key(index, "saturation");
  sysparam_set_int8(key_name, hk_colors_saturation[index]);
  free(key_name);
  updateColors();
}

homekit_value_t led_mode_get() {
  return HOMEKIT_INT(hk_mode);
}

void led_mode_set(homekit_value_t value) {
  hk_mode = value.int_value;
  sysparam_set_int8("ws281x_mode", hk_mode);
}

homekit_value_t led_speed_get() {
  return HOMEKIT_INT(hk_speed);
}

void led_speed_set(homekit_value_t value) {
  hk_speed = value.int_value;
  sysparam_set_int8("ws281x_speed", hk_speed);
}

homekit_value_t led_reverse_get() {
  return HOMEKIT_BOOL(hk_reverse);
}

void led_reverse_set(homekit_value_t value) {
  hk_reverse = value.bool_value;
  sysparam_set_bool("ws281x_reverse", hk_reverse);
}

homekit_value_t led_density_get() {
  return HOMEKIT_INT(hk_density);
}

void led_density_set(homekit_value_t value) {
  hk_density = value.int_value;
  sysparam_set_int8("ws281x_density", hk_density);
}

homekit_value_t led_fade_get() {
  return HOMEKIT_INT(hk_fade);
}

void led_fade_set(homekit_value_t value) {
  hk_fade = value.int_value;
  sysparam_set_int8("ws281x_fade", hk_fade);
}

homekit_value_t led_count_get() {
  return HOMEKIT_INT(hk_led_count);
}

void led_count_set(homekit_value_t value) {
  if (hk_led_count != value.int_value) {
    hk_led_count = value.int_value;
    sysparam_set_int32("ws281x_led_count", hk_led_count);
    // have to force the underlying libraries to restart...
    info("Forcing system restart to allow memory reinitialization with new LED Count: %d", hk_led_count);
    sdk_system_restart();
  }
}

homekit_value_t led_data_order_get() {
  return HOMEKIT_INT(hk_data_order);
}

void led_data_order_set(homekit_value_t value) {
  hk_data_order = value.int_value;
  sysparam_set_int8("ws281x_data_order", hk_data_order);
}

void on_wifi_connected() {
  debug_heap("before homekit");
  blink_status(1, 2);
  homekit_server_init(&config);
}

void on_homekit_verified() {
  if (working_pixels == 0) init_ws281x();
}

void user_init(void) {
  common_init(HOMEKIT_NAME, &info_service);

  info("Checking for stored state...");
  char *ws281x_version = NULL;
  sysparam_get_string("ws281x_version", &ws281x_version);
  if (ws281x_version) {
    sysparam_get_bool("ws281x_on", &hk_strip_on);
    sysparam_get_int8("ws281x_brightness", &hk_strip_brightness);
    sysparam_get_int8("ws281x_mode", &hk_mode);
    sysparam_get_int8("ws281x_speed", &hk_speed);
    sysparam_get_bool("ws281x_reverse", &hk_reverse);
    sysparam_get_int8("ws281x_density", &hk_density);
    sysparam_get_int8("ws281x_fade", &hk_fade);
    int32_t led_count;
    sysparam_get_int32("ws281x_led_count", &led_count);
    hk_led_count = led_count;
    sysparam_get_int8("ws281x_data_order", &hk_data_order);
    for (uint8_t i = 0; i < 7; i++) {
      char *key_name = color_key(i, "on");
      sysparam_get_bool(key_name, &hk_colors_on[i]);
      free(key_name);
      key_name = color_key(i, "hue");
      int32_t hue;
      sysparam_get_int32(key_name, &hue);
      hk_colors_hue[i] = hue;
      free(key_name);
      key_name = color_key(i, "saturation");
      sysparam_get_int8(key_name, &hk_colors_saturation[i]);
      free(key_name);
    }
    info("Found state:\n"
           "\tversion:%s\n"
           "\ton:%d\n"
           "\tbrightness:%d\n"
           "\tmode:%d\n"
           "\tspeed:%d\n"
           "\treverse:%d\n"
           "\tdensity:%d\n"
           "\tfade:%d\n"
           "\tled_count:%d\n"
           "\tdata_order:%d\n"
           "\tcolor0_on:%d\n"
           "\tcolor0_hue:%d\n"
           "\tcolor0_saturation:%d\n"
           "\tcolor1_on:%d\n"
           "\tcolor1_hue:%d\n"
           "\tcolor1_saturation:%d\n"
           "\tcolor2_on:%d\n"
           "\tcolor2_hue:%d\n"
           "\tcolor2_saturation:%d\n"
           "\tcolor3_on:%d\n"
           "\tcolor3_hue:%d\n"
           "\tcolor3_saturation:%d\n"
           "\tcolor4_on:%d\n"
           "\tcolor4_hue:%d\n"
           "\tcolor4_saturation:%d\n"
           "\tcolor5_on:%d\n"
           "\tcolor5_hue:%d\n"
           "\tcolor5_saturation:%d\n"
           "\tcolor6_on:%d\n"
           "\tcolor6_hue:%d\n"
           "\tcolor6_saturation:%d\n",
      ws281x_version,
      hk_strip_on,
      hk_strip_brightness,
      hk_mode,
      hk_speed,
      hk_reverse,
      hk_density,
      hk_fade,
      hk_led_count,
      hk_data_order,
      hk_colors_on[0],
      hk_colors_hue[0],
      hk_colors_saturation[0],
      hk_colors_on[1],
      hk_colors_hue[1],
      hk_colors_saturation[1],
      hk_colors_on[2],
      hk_colors_hue[2],
      hk_colors_saturation[2],
      hk_colors_on[3],
      hk_colors_hue[3],
      hk_colors_saturation[3],
      hk_colors_on[4],
      hk_colors_hue[4],
      hk_colors_saturation[4],
      hk_colors_on[5],
      hk_colors_hue[5],
      hk_colors_saturation[5],
      hk_colors_on[6],
      hk_colors_hue[6],
      hk_colors_saturation[6]
    );
  }
  else {
    info("Unable to find last stored state. Initializing...");
    sysparam_set_string("ws281x_version", "1.0");
    sysparam_set_bool("ws281x_on", hk_strip_on);
    sysparam_set_int8("ws281x_brightness", hk_strip_brightness);
    sysparam_set_int8("ws281x_mode", hk_mode);
    sysparam_set_int8("ws281x_speed", hk_speed);
    sysparam_set_bool("ws281x_reverse", hk_reverse);
    sysparam_set_int8("ws281x_density", hk_density);
    sysparam_set_int8("ws281x_fade", hk_fade);
    sysparam_set_int32("ws281x_led_count", hk_led_count);
    sysparam_set_int8("ws281x_data_order", hk_data_order);
    for (uint8_t i = 0; i < 7; i++) {
      char *key_name = color_key(i, "on");
      sysparam_set_bool(key_name, hk_colors_on[i]);
      free(key_name);
      key_name = color_key(i, "hue");
      sysparam_set_int32(key_name, hk_colors_hue[i]);
      free(key_name);
      key_name = color_key(i, "saturation");
      sysparam_set_int8(key_name, hk_colors_saturation[i]);
      free(key_name);
    }
  }

  control_service.characteristics[1]->value = HOMEKIT_BOOL(hk_strip_on);
  control_service.characteristics[2]->value = HOMEKIT_INT(hk_strip_brightness);
  control_service.characteristics[3]->value = HOMEKIT_INT(hk_mode);
  control_service.characteristics[4]->value = HOMEKIT_INT(hk_speed);
  control_service.characteristics[5]->value = HOMEKIT_BOOL(hk_reverse);
  control_service.characteristics[6]->value = HOMEKIT_INT(hk_density);
  control_service.characteristics[7]->value = HOMEKIT_INT(hk_fade);
  control_service.characteristics[8]->value = HOMEKIT_INT(hk_led_count);
  control_service.characteristics[9]->value = HOMEKIT_INT(hk_data_order);
  color0_service.characteristics[1]->value = HOMEKIT_BOOL(hk_colors_on[0]);
  color0_service.characteristics[2]->value = HOMEKIT_FLOAT(hk_colors_hue[0]);
  color0_service.characteristics[3]->value = HOMEKIT_FLOAT(hk_colors_saturation[0]);
  color1_service.characteristics[1]->value = HOMEKIT_BOOL(hk_colors_on[1]);
  color1_service.characteristics[2]->value = HOMEKIT_FLOAT(hk_colors_hue[1]);
  color1_service.characteristics[3]->value = HOMEKIT_FLOAT(hk_colors_saturation[1]);
  color2_service.characteristics[1]->value = HOMEKIT_BOOL(hk_colors_on[2]);
  color2_service.characteristics[2]->value = HOMEKIT_FLOAT(hk_colors_hue[2]);
  color2_service.characteristics[3]->value = HOMEKIT_FLOAT(hk_colors_saturation[2]);
  color3_service.characteristics[1]->value = HOMEKIT_BOOL(hk_colors_on[3]);
  color3_service.characteristics[2]->value = HOMEKIT_FLOAT(hk_colors_hue[3]);
  color3_service.characteristics[3]->value = HOMEKIT_FLOAT(hk_colors_saturation[3]);
  color4_service.characteristics[1]->value = HOMEKIT_BOOL(hk_colors_on[4]);
  color4_service.characteristics[2]->value = HOMEKIT_FLOAT(hk_colors_hue[4]);
  color4_service.characteristics[3]->value = HOMEKIT_FLOAT(hk_colors_saturation[4]);
  color5_service.characteristics[1]->value = HOMEKIT_BOOL(hk_colors_on[5]);
  color5_service.characteristics[2]->value = HOMEKIT_FLOAT(hk_colors_hue[5]);
  color5_service.characteristics[3]->value = HOMEKIT_FLOAT(hk_colors_saturation[5]);
  color6_service.characteristics[1]->value = HOMEKIT_BOOL(hk_colors_on[6]);
  color6_service.characteristics[2]->value = HOMEKIT_FLOAT(hk_colors_hue[6]);
  color6_service.characteristics[3]->value = HOMEKIT_FLOAT(hk_colors_saturation[6]);
}
