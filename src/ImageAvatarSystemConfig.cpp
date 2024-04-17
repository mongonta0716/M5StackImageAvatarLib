#include "ImageAvatarSystemConfig.h"


ImageAvatarSystemConfig::ImageAvatarSystemConfig() {
    _volume = 100;
    _lcd_brightness = 50;
    _avatar_count = 0;
    _bluetooth_reconnect = false;
    _servo_random_mode = true;
    _auto_power_off_time = 0;
    _led_lr = 0;
};

ImageAvatarSystemConfig::~ImageAvatarSystemConfig() {};

void ImageAvatarSystemConfig::loadConfig(fs::FS& fs, const char *yaml_filename) {
    Serial.printf("----- ImageAvatarSystemConfig::loadConfig:%s\n", yaml_filename);
    File file = fs.open(yaml_filename);
    int res = file.available();
    Serial.printf("file:available:%d\n", res);
    DynamicJsonDocument doc(1024);
    auto error = deserializeYml(doc, file);
    if (error) {
        Serial.printf("yaml file read error: %s\n", yaml_filename);
        Serial.printf("error%s\n", error.c_str());
    }
    serializeJsonPretty(doc, Serial);
    setSystemConfig(doc);

    file.close();
#ifdef DEBUG
     printAllParameters();
#endif
}

void ImageAvatarSystemConfig::setSystemConfig(DynamicJsonDocument doc) {

    _volume = doc["volume"];
    _lcd_brightness = doc["lcd_brightness"];
    JsonArray avatar_yaml = doc["avatar_yaml"];
    _avatar_count = avatar_yaml.size();

    for (int i=0; i<_avatar_count; i++) {
        _avatar_yamlfiles[i] = avatar_yaml[i].as<String>();
    }
    _bluetooth_device_name = doc["bluetooth_device_name"].as<String>();
    _bluetooth_reconnect = doc["bluetooth_reconnect"];
    _servo_yamlfile = doc["servo_yaml"].as<String>(); 
    _servo_random_mode = doc["servo_random_mode"];
    _auto_power_off_time = doc["auto_power_off_time"];
    _led_lr = doc["audio_led_lr"];
    _takao_base = doc["use_takao_base"];
    _avatar_swing_interval = doc["mic_swing_interval"];
}

void ImageAvatarSystemConfig::printAllParameters() {
    Serial.printf("Volume: %d\n", _volume);
    Serial.printf("Brightness: %d\n", _lcd_brightness);
    Serial.printf("Avatar Max Count: %d\n", _avatar_count);
    for (int i=0; i<_avatar_count; i++) {
        Serial.printf("Avatar Json FileName:%d :%s\n", i, (const char *)_avatar_yamlfiles[i].c_str());
    }
    Serial.printf("Bluetooth Device Name: %s\n", _bluetooth_device_name);
    Serial.printf("Bluetooth Reconnect :%s\n", _bluetooth_reconnect ? "true" : "false");
    Serial.printf("Servo Json FileName: %s\n", (const char *)_servo_yamlfile.c_str());
    Serial.printf("Servo Random Mode:%s\n", _servo_random_mode ? "true" : "false");
    Serial.printf("AutoPowerOffTime: %d\n", _auto_power_off_time);
    Serial.printf("LED LR: %d\n", _led_lr);
}