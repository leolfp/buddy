// Copyright(c) 2023 Takao Akaki


#include <M5Unified.h>
#include <Avatar.h>
#include "fft.hpp"
#include <cinttypes>
#if defined(ARDUINO_M5STACK_CORES3)
  #include <gob_unifiedButton.hpp>
  goblib::UnifiedButton unifiedButton;
#endif
#define USE_MIC

#ifdef USE_MIC
  // ---------- Mic sampling ----------

  #define READ_LEN    (2 * 256)
  #define LIPSYNC_LEVEL_MAX 10.0f

  int16_t *adcBuffer = NULL;
  static fft_t fft;
  static constexpr size_t WAVE_SIZE = 256 * 2;

  static constexpr const size_t record_samplerate = 16000; // M5StickCPlus2 didn't work without 48KHz, but it was fixed in M5Unified0.1.12 and caused issues with M5AtomS2+PDFUnit, so reverted back.
  static int16_t *rec_data;
  
  // This may be overwritten in the device detection at the beginning of setup. Please check there as well. (Due to different microphone performance)
  uint8_t lipsync_shift_level = 11; // Setting for how much to reduce lipsync data. Changes mouth opening behavior.
  float lipsync_max =LIPSYNC_LEVEL_MAX;  // Lipsync unit - increasing/decreasing this changes mouth opening behavior.

#endif

using namespace m5avatar;

Avatar avatar;
ColorPalette cps = ColorPalette();

uint32_t last_rotation_msec = 0;
uint32_t last_lipsync_max_msec = 0;

void lipsync() {
  
  size_t bytesread;
  uint64_t level = 0;
#ifndef SDL_h_
  if ( M5.Mic.record(rec_data, WAVE_SIZE, record_samplerate)) {
    fft.exec(rec_data);
    for (size_t bx=5;bx<=60;++bx) {
      int32_t f = fft.get(bx);
      level += abs(f);
    }
  }
  uint32_t temp_level = level >> lipsync_shift_level;
  //M5_LOGI("level:%" PRId64 "\n", level) ;         // Uncomment this line when adjusting lipsync_max.
  //M5_LOGI("temp_level:%d\n", temp_level) ;         // Uncomment this line when adjusting lipsync_max.
  float ratio = (float)(temp_level / lipsync_max);
  //M5_LOGI("ratio:%f\n", ratio);
  if (ratio <= 0.01f) {
    ratio = 0.0f;
    if ((lgfx::v1::millis() - last_lipsync_max_msec) > 500) {
      // Reset lipsync upper limit if silent for more than 0.5 seconds
      last_lipsync_max_msec = lgfx::v1::millis();
      lipsync_max = LIPSYNC_LEVEL_MAX;
    }
  } else {
    if (ratio > 1.3f) {
      if (ratio > 1.5f) {
        // If significantly exceeding lipsync upper limit, increase the upper limit.
        lipsync_max += 10.0f;
      }
      ratio = 1.3f;
    }
    last_lipsync_max_msec = lgfx::v1::millis(); // Update when not silent
  }

  if ((lgfx::v1::millis() - last_rotation_msec) > 350) {
    int direction = random(-2, 2);
    avatar.setRotation(direction * 10 * ratio);
    last_rotation_msec = lgfx::v1::millis();
  }
#else
  float ratio = 0.0f;
#endif
  avatar.setMouthOpenRatio(ratio);
  
}


void setup()
{
  auto cfg = M5.config();
  cfg.internal_mic = true;
  M5.begin(cfg);
#if defined( ARDUINO_M5STACK_CORES3 )
  unifiedButton.begin(&M5.Display, goblib::UnifiedButton::appearance_t::transparent_all);
#endif
  M5.Log.setLogLevel(m5::log_target_display, ESP_LOG_NONE);
  M5.Log.setLogLevel(m5::log_target_serial, ESP_LOG_INFO);
  M5.Log.setEnableColor(m5::log_target_serial, false);
  M5_LOGI("Avatar Start");
  M5.Log.printf("M5.Log avatar Start\n");
  float scale = 0.0f;
  int8_t position_top = 0;
  int8_t position_left = 0;
  uint8_t display_rotation = 1; // Display orientation (0-3)
  uint8_t first_cps = 0;
  auto mic_cfg = M5.Mic.config();

#ifndef SDL_h_
  rec_data = (typeof(rec_data))heap_caps_malloc(WAVE_SIZE * sizeof(int16_t), MALLOC_CAP_8BIT);
  memset(rec_data, 0 , WAVE_SIZE * sizeof(int16_t));
  M5.Mic.begin();
#endif
  M5.Speaker.end();

  scale = 1.0f;
  position_top = 0;
  position_left = 0;
  display_rotation = 1;
  M5.Display.setRotation(display_rotation);
  avatar.setScale(scale);
  avatar.setPosition(position_top, position_left);

  avatar.init(1); // start drawing
  cps.set(COLOR_PRIMARY, TFT_BLACK);
  M5_LOGI("Primary color set");
  cps.set(COLOR_BACKGROUND, TFT_YELLOW);
  M5_LOGI("Background color set");

  avatar.setColorPalette(cps);
  M5_LOGI("Palette set");
  //avatar.addTask(lipsync, "lipsync");
  last_rotation_msec = lgfx::v1::millis();
  M5_LOGI("Setup end");
}

uint32_t count = 0;

void loop()
{
  M5.update();

#if defined( ARDUINO_M5STACK_CORES3 )
  unifiedButton.update();
#endif
  if (M5.BtnA.wasPressed()) {
    M5_LOGI("Push BtnA");
  }
  if (M5.BtnA.wasDoubleClicked()) {
    M5.Display.setRotation(3);
  }
  if (M5.BtnPWR.wasClicked()) {
#ifdef ARDUINO
    esp_restart();
#endif
  } 
//  if ((millis() - last_rotation_msec) > 100) {
    //float angle = 10 * sin(count);
    //avatar.setRotation(angle);
    //last_rotation_msec = millis();
    //count++;
  //}

  // avatar's face updates in another thread
  // so no need to loop-by-loop rendering
  lipsync();
  lgfx::v1::delay(1);
}
