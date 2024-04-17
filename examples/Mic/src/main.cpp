#if defined( ARDUINO )
#include <Arduino.h>
#include <SD.h>
#include <SPIFFS.h>
#endif

#include <M5Unified.h>

#define SDU_APP_PATH "/M5ImageAvatarLiteMic.bin" // title for SD-Updater UI
#define SDU_APP_NAME "Image Avater Lite Mic Example" // title for SD-Updater UI
#include <M5StackUpdater.h> // https://github.com/tobozo/M5Stack-SD-Updater/

#include "M5ImageAvatarLite.h"
#include "ImageAvatarSystemConfig.h" 

#include "fft.hpp"
#include <cinttypes>


M5GFX &gfx( M5.Lcd ); // aliasing is better than spawning two instances of LGFX

// YAMLファイルとBMPファイルを置く場所を切り替え
// 開発時はSPIFFS上に置いてUploadするとSDカードを抜き差しする手間が省けます。
fs::FS yaml_fs = SD; // JSONファイルの収納場所(SPIFFS or SD)
fs::FS bmp_fs  = SD; // BMPファイルの収納場所(SPIFFS or SD)

using namespace m5imageavatar;


ImageAvatarSystemConfig system_config;
const char* avatar_system_yaml = "/yaml/M5AvatarLiteSystem.yaml"; // ファイル名は32バイトを超えると不具合が起きる場合あり
uint8_t avatar_count = 0;
uint8_t expression = 0;

ImageAvatarLite avatar(yaml_fs, bmp_fs);

// auto poweroff 
uint32_t auto_power_off_time = 0;  // USB給電が止まった後自動で電源OFFするまでの時間（msec）。0は電源OFFしない。
uint32_t last_discharge_time = 0;  // USB給電が止まったときの時間(msec)
uint32_t power_check_interval = 10000; // 充電状態をチェックする時間
uint32_t last_power_check_time = 0; // 最後にチェックした時間
uint32_t last_lipsync_max_msec = 0; // 
uint32_t last_rotation_msec = 0;

// Multi Threads Settings
TaskHandle_t lipsyncTaskHandle;
SemaphoreHandle_t xMutex = NULL;

/// set M5Speaker virtual channel (0-7)
static constexpr uint8_t m5spk_virtual_channel = 0;

// ---------- Mic sampling ----------

#define READ_LEN    (2 * 256)
#define LIPSYNC_LEVEL_MAX 10.0f

int16_t *adcBuffer = NULL;
static fft_t fft;
static constexpr size_t WAVE_SIZE = 256 * 2;

static constexpr const size_t record_samplerate = 16000; // M5StickCPlus2だと48KHzじゃないと動かない。(M5Unified 0.1.13で解消)
static int16_t *rec_data;

// setupの最初の方の機種判別で書き換えている場合があります。そちらもチェックしてください。（マイクの性能が異なるため）
uint8_t lipsync_shift_level = 11; // リップシンクのデータをどのくらい小さくするか設定。口の開き方が変わります。
float lipsync_max =LIPSYNC_LEVEL_MAX;  // リップシンクの単位ここを増減すると口の開き方が変わります。



// Start----- Task functions ----------

void lipsync(void *args) {
  uint8_t angle_count = 0;
  for (;;) { 
    size_t bytesread;
    uint64_t level = 0;
    if ( M5.Mic.record(rec_data, WAVE_SIZE, record_samplerate)) {
      fft.exec(rec_data);
      for (size_t bx=5;bx<=60;++bx) {
        int32_t f = fft.get(bx);
        level += abs(f);
      }
    }
    uint32_t temp_level = level >> lipsync_shift_level;
    //M5_LOGI("level:%" PRId64 "\n", level) ;         // lipsync_maxを調整するときはこの行をコメントアウトしてください。
    //M5_LOGI("temp_level:%d\n", temp_level) ;         // lipsync_maxを調整するときはこの行をコメントアウトしてください。
    float ratio = (float)(temp_level / lipsync_max);
    //M5_LOGI("ratio:%f\n", ratio);
    if (ratio <= 0.01f) {
      ratio = 0.0f;
      if ((millis() - last_lipsync_max_msec) > 500) {
        // 0.5秒以上無音の場合リップシンク上限をリセット
        last_lipsync_max_msec = millis();
        lipsync_max = LIPSYNC_LEVEL_MAX;
      }
    } else {
      if (ratio > 1.3f) {
        if (ratio > 1.5f) {
          // リップシンク上限を大幅に超えた場合、上限を上げていく。
          lipsync_max += 10.0f;
        }
        ratio = 1.3f;
      }
      last_lipsync_max_msec = millis(); // 無音でない場合は更新
    }
    if (system_config.getAvatarSwingInterval() > 0) {
      if ((millis() - last_rotation_msec) > system_config.getAvatarSwingInterval()) {
        float direction = 2 * sin(angle_count);
        avatar.setRotation(direction * 10 * ratio);
        last_rotation_msec = millis();
        angle_count++;
      }
    }
    avatar.setMouthOpenRatio(ratio);
  }
}


void setup() {
  auto cfg = M5.config();
  cfg.output_power = true;
  cfg.internal_mic = true;
#ifdef ARDUINO_M5STACK_FIRE
  cfg.internal_imu = false; // サーボの誤動作防止(Fireは21,22を使うので干渉するため)
#endif
  M5.begin(cfg);

  xMutex = xSemaphoreCreateMutex();
  SPIFFS.begin();
  SD.begin(GPIO_NUM_4, SPI, 25000000);
  
  system_config.loadConfig(yaml_fs, avatar_system_yaml);
  system_config.printAllParameters();
  M5.Speaker.setVolume(system_config.getVolume());
  M5.Power.setExtOutput(!system_config.getTakaoBase());  // TakaoBaseの後ろ給電を使う場合、falseにしないとバッテリーが充電されない。
  M5_LOGI("SystemVolume: %d\n", M5.Speaker.getVolume());
  M5.Speaker.setChannelVolume(m5spk_virtual_channel, system_config.getVolume());
  M5_LOGI("ChannelVolume: %d\n", M5.Speaker.getChannelVolume(m5spk_virtual_channel));

  M5.Lcd.setBrightness(system_config.getLcdBrightness());

  
  auto_power_off_time = system_config.getAutoPowerOffTime();
  String avatar_filename = system_config.getAvatarYamlFilename(avatar_count);
  M5_LOGI("ImageAvatar init");
  avatar.init(&gfx, avatar_filename.c_str(), false, 0);
  avatar.start();
  last_power_check_time = millis();
  M5.Speaker.tone(600, 100);
  rec_data = (typeof(rec_data))heap_caps_malloc(WAVE_SIZE * sizeof(int16_t), MALLOC_CAP_8BIT);
  memset(rec_data, 0 , WAVE_SIZE * sizeof(int16_t));
  M5.Speaker.end();
  M5.Mic.begin();
  avatar.addTask(lipsync, "lipsync", 1, 2048, &lipsyncTaskHandle, APP_CPU_NUM);

}

void loop() {
  // SpeakerとMicは排他利用のため、Speaker.tone()の前後でMic.end(), Speaker.begin()とMic.begin()を実行しています。
  M5.update();
  if (M5.BtnA.wasClicked()) {
    // アバターを変更します。
    avatar_count++;
    if (avatar_count >= system_config.getAvatarMaxCount()) {
      avatar_count = 0;
    }
    Serial.printf("Avatar No:%d\n", avatar_count);
    M5.Mic.end();
    M5.Speaker.begin();
    M5.Speaker.tone(600, 100);
    delay(100);
    avatar.changeAvatar(system_config.getAvatarYamlFilename(avatar_count).c_str());
    M5.Speaker.end();
    M5.Mic.begin();
  }

  if (M5.BtnB.wasClicked()) {
    // ウィンクします。
    M5.Mic.end();
    M5.Speaker.begin();
    M5.Speaker.tone(3000, 100);
    delay(100);
    M5.Speaker.end();
    M5.Mic.begin();
    avatar.leftWink(true);
    avatar.setRotation(10.0f);
    delay(1000);
    avatar.leftWink(false);
    avatar.setRotation(0.0f);
  }

  if (M5.BtnC.wasClicked()) {
    // 表情を切り替え
    M5.Mic.end();
    M5.Speaker.begin();
    M5.Speaker.tone(1000, 100);
    delay(500);
    M5.Speaker.tone(600, 100);
    expression++;
    if (expression >= avatar.getExpressionMax()) {
      expression = 0;
    }
    avatar.setExpression(system_config.getAvatarYamlFilename(avatar_count).c_str(), expression);
    M5.Speaker.end();
    M5.Mic.begin();
  }
}