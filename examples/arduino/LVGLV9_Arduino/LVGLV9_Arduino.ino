#ifndef BOARD_HAS_PSRAM
#error "Error: This program requires PSRAM enabled, please enable PSRAM option in 'Tools' menu of Arduino IDE"
#endif

#include <Arduino_GFX_Library.h>
#include "displays_config.h"
#include "gt911.h"
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <lvgl.h>
#include "lv_conf.h"

static esp_lcd_touch_handle_t tp_handle = NULL;
#define MAX_TOUCH_POINTS 5

Arduino_ESP32DSIPanel *dsipanel = new Arduino_ESP32DSIPanel(
  display_cfg.hsync_pulse_width,
  display_cfg.hsync_back_porch,
  display_cfg.hsync_front_porch,
  display_cfg.vsync_pulse_width,
  display_cfg.vsync_back_porch,
  display_cfg.vsync_front_porch,
  display_cfg.prefer_speed,
  display_cfg.lane_bit_rate);

Arduino_DSI_Display *gfx = new Arduino_DSI_Display(
  display_cfg.width,
  display_cfg.height,
  dsipanel,
  0,
  true,
  display_cfg.lcd_rst,
  display_cfg.init_cmds,
  display_cfg.init_cmds_size);

#define LVGL_TICK_PERIOD 5  // ms
#define DRAW_BUF_HEIGHT 50
static lv_display_t *lv_display;
static lv_indev_t *indev_touchpad;
static lv_color_t *lv_draw_buf1;
static lv_color_t *lv_draw_buf2;
static uint16_t touch_x[MAX_TOUCH_POINTS] = { 0 };
static uint16_t touch_y[MAX_TOUCH_POINTS] = { 0 };
static uint16_t touch_strength[MAX_TOUCH_POINTS] = { 0 };
static uint8_t touch_cnt = 0;
static bool touch_pressed = false;

void halt_setup(const char *message) {
  Serial.println(message);
  while (true) {
    delay(1000);
  }
}

void create_demo_ui(void) {
  lv_obj_t *screen = lv_screen_active();
  lv_obj_set_style_bg_color(screen, lv_color_hex(0x101820), LV_PART_MAIN);

  lv_obj_t *title = lv_label_create(screen);
  lv_label_set_text(title, "Waveshare ESP32-P4");
  lv_obj_align(title, LV_ALIGN_CENTER, 0, -100);

  lv_obj_t *subtitle = lv_label_create(screen);
  lv_label_set_text(subtitle, "Arduino GFX + LVGL 9");
  lv_obj_align(subtitle, LV_ALIGN_CENTER, 0, -55);

  lv_obj_t *slider = lv_slider_create(screen);
  lv_obj_set_width(slider, 420);
  lv_slider_set_value(slider, 65, LV_ANIM_OFF);
  lv_obj_align(slider, LV_ALIGN_CENTER, 0, 25);

  lv_obj_t *button = lv_button_create(screen);
  lv_obj_set_size(button, 220, 70);
  lv_obj_align(button, LV_ALIGN_CENTER, 0, 120);

  lv_obj_t *button_label = lv_label_create(button);
  lv_label_set_text(button_label, "Touch ready");
  lv_obj_center(button_label);
}

void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);

  gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)px_map, w, h);
  lv_display_flush_ready(disp);
}

void my_touchpad_read(lv_indev_t *indev, lv_indev_data_t *data) {
  (void)indev;
  touch_cnt = 0;
  if (esp_lcd_touch_read_data(tp_handle) != ESP_OK) {
    data->state = LV_INDEV_STATE_RELEASED;
    return;
  }
  touch_pressed = esp_lcd_touch_get_coordinates(
    tp_handle, touch_x, touch_y, touch_strength, &touch_cnt, MAX_TOUCH_POINTS);

  if (touch_pressed && touch_cnt > 0) {
    data->point.x = touch_x[0];
    data->point.y = touch_y[0];
    data->state = LV_INDEV_STATE_PRESSED;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

void lvglTick(void *param) {
  (void)param;
  lv_tick_inc(LVGL_TICK_PERIOD);
}

void setup(void) {
  Serial.begin(115200);
  Serial.println("Arduino GFX + LVGL Integration");

  delay(1000);

  DEV_I2C_Port port = DEV_I2C_Init();

  tp_handle = touch_gt911_init(port);
  if (!tp_handle) {
    halt_setup("GT911 initialization failed!");
  }

  if (!gfx->begin()) {
    halt_setup("gfx->begin() failed!");
  }

  lv_init();

  size_t draw_buf_pixels = display_cfg.width * DRAW_BUF_HEIGHT;
  size_t draw_buf_bytes = draw_buf_pixels * sizeof(lv_color_t);
  lv_draw_buf1 = (lv_color_t *)heap_caps_malloc(draw_buf_bytes, MALLOC_CAP_DMA);
  if (!lv_draw_buf1) {
    halt_setup("LVGL draw buffer 1 allocation failed!");
  }

  lv_draw_buf2 = (lv_color_t *)heap_caps_malloc(draw_buf_bytes, MALLOC_CAP_DMA);
  if (!lv_draw_buf2) {
    heap_caps_free(lv_draw_buf1);
    lv_draw_buf1 = NULL;
    halt_setup("LVGL draw buffer 2 allocation failed!");
  }

  lv_display = lv_display_create(display_cfg.width, display_cfg.height);
  if (!lv_display) {
    halt_setup("LVGL display creation failed!");
  }
  lv_display_set_flush_cb(lv_display, my_disp_flush);
  lv_display_set_buffers(lv_display, lv_draw_buf1, lv_draw_buf2, draw_buf_bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);

  indev_touchpad = lv_indev_create();
  if (!indev_touchpad) {
    halt_setup("LVGL input-device creation failed!");
  }
  lv_indev_set_type(indev_touchpad, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev_touchpad, my_touchpad_read);

  esp_timer_create_args_t lvgl_timer_args = {};
  lvgl_timer_args.callback = &lvglTick;
  lvgl_timer_args.name = "lvgl_timer";
  esp_timer_handle_t lvgl_timer;
  if (esp_timer_create(&lvgl_timer_args, &lvgl_timer) != ESP_OK ||
      esp_timer_start_periodic(lvgl_timer, LVGL_TICK_PERIOD * 1000) != ESP_OK) {
    halt_setup("LVGL tick timer initialization failed!");
  }

  lv_display_set_dpi(lv_display, 150);
  create_demo_ui();

  Serial.println("Setup complete");
}

void loop() {
  lv_timer_handler();
  delay(5);
}
