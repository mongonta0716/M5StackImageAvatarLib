#if defined( ARDUINO )
#include <Arduino.h>
#include <SD.h>
#include <SPIFFS.h>
#endif

#include <M5Unified.h>

#define SDU_APP_PATH "/M5Core2AvatarLiteLib.bin" // title for SD-Updater UI
#define SDU_APP_NAME "Image Avater Lite Example" // title for SD-Updater UI
#include <M5StackUpdater.h> // https://github.com/tobozo/M5Stack-SD-Updater/

#include "M5ImageAvatarLite.h"
#include "ImageAvatarSystemConfig.h" 


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

// Multi Threads Settings
TaskHandle_t lipsyncTaskHandle;
SemaphoreHandle_t xMutex = NULL;

/// set M5Speaker virtual channel (0-7)
static constexpr uint8_t m5spk_virtual_channel = 0;


// Start----- Task functions ----------
void lipsync(void *args) {
  // スレッド内でログを出そうとすると不具合が起きる場合があります。
  DriveContext * ctx = reinterpret_cast<DriveContext *>(args);
  ImageAvatarLite *avatar = ctx->getAvatar();
  for(;;) {
     uint64_t level = 0;
     vTaskDelay(1/portTICK_PERIOD_MS);
  }
}



void setup() {
  auto cfg = M5.config();
  cfg.output_power = true;
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

}

void loop() {
  M5.update();
  if (M5.BtnA.wasClicked()) {
    // アバターを変更します。
    avatar_count++;
    if (avatar_count >= system_config.getAvatarMaxCount()) {
      avatar_count = 0;
    }
    Serial.printf("Avatar No:%d\n", avatar_count);
    M5.Speaker.tone(600, 100);
    avatar.changeAvatar(system_config.getAvatarYamlFilename(avatar_count).c_str());
  }


  if (M5.BtnB.wasClicked()) {
    size_t v = M5.Speaker.getChannelVolume(m5spk_virtual_channel);
    v += 10;
    if (v <= 255) {
      M5.Speaker.setVolume(v);
      M5.Speaker.setChannelVolume(m5spk_virtual_channel, v);
      Serial.printf("Volume: %d\n", v);
      Serial.printf("SystemVolume: %d\n", M5.Speaker.getVolume());
      M5.Speaker.tone(1000, 100);
    }
  }


  if (M5.BtnC.pressedFor(2000)) {
    M5.Speaker.tone(1000, 100);
    delay(500);
    M5.Speaker.tone(600, 100);
    // 表情を切り替え
    expression++;
    Serial.printf("ExpressionMax:%d\n", avatar.getExpressionMax());
    if (expression >= avatar.getExpressionMax()) {
      expression = 0;
    }
    Serial.printf("----------Expression: %d----------\n", expression);
    avatar.setExpression(system_config.getAvatarYamlFilename(avatar_count).c_str(), expression);
    Serial.printf("Resume\n");
  }
  if (M5.BtnC.wasClicked()) {
    size_t v = M5.Speaker.getChannelVolume(m5spk_virtual_channel);
    v -= 10;
    if (v > 0) {
      M5.Speaker.setVolume(v);
      M5.Speaker.setChannelVolume(m5spk_virtual_channel, v);
      Serial.printf("Volume: %d\n", v);
      Serial.printf("SystemVolume: %d\n", M5.Speaker.getVolume());
      M5.Speaker.tone(800, 100);
    }
  }
#ifndef ARDUINO_M5STACK_FIRE // FireはAxp192ではないのとI2Cが使えないので制御できません。
  if ((millis()-last_power_check_time) > power_check_interval) {
    if (M5.Power.Axp192.getACINVolatge() < 3.0f) {
      // USBからの給電が停止したとき
      // Serial.println("USBPowerUnPluged.");
      M5.Power.setLed(0);
      if ((auto_power_off_time > 0) and (last_discharge_time == 0)) {
        M5.Speaker.tone(1000, 100);
        delay(500);
        M5.Speaker.tone(800, 100);
        last_discharge_time = millis();
      } else if ((auto_power_off_time > 0) and ((millis() - last_discharge_time) > auto_power_off_time)) {
        M5.Power.powerOff();
      }
    } else {
      //Serial.println("USBPowerPluged.");
      M5.Power.setLed(80);
      if (last_discharge_time > 0) {
        last_discharge_time = 0;
      }
    }
  }
#endif
  vTaskDelay(50);
}