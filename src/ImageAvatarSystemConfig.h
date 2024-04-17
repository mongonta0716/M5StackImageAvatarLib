#ifndef _IMAGEAVATAR_SYSTEM_CONFIG_H_
#define _IMAGEAVATAR_SYSTEM_CONFIG_H_

#include <FS.h>
#include <M5Unified.h>
#include <ArduinoJson.h>
#include <ArduinoYaml.h>

#define MAX_AVATAR_NUM 20  // 最大20個のAvatar

class ImageAvatarSystemConfig {
    protected:
        uint8_t _volume;                                       // Initial volume
        uint8_t _lcd_brightness;                               // brightness of lcd
        uint8_t _avatar_count;                                 // avatar count
        String _avatar_yamlfiles[MAX_AVATAR_NUM];              // json filename
        String _bluetooth_device_name;                         // bluetooth_device_name
        bool _bluetooth_reconnect;                             // 起動時にBluetoothを再接続するかどうか
        String _servo_yamlfile;                                // servo json filename
        bool _servo_random_mode;                               // servo random mode flag
        uint32_t _auto_power_off_time;                         // USB給電が停止後、電源OFFするまでの時間(msec)。0だと電源OFFしない。
        uint8_t _led_lr;                                       // LEDの音源を左右どちらにするか(0:stereo, 1:left_only, 2:right_only)
        bool _takao_base;                                      // TakaoBaseの後ろ給電をするかどうか。
        uint32_t _avatar_swing_interval;                       // 0以外を指定するとAvatarが一定間隔(msec)で左右に傾きます。
        void setSystemConfig(DynamicJsonDocument doc);
    public:
        ImageAvatarSystemConfig();
        ~ImageAvatarSystemConfig();
        void loadConfig(fs::FS& fs, const char *json_filename);

        void printAllParameters();
        uint8_t getVolume() { return _volume; }
        uint8_t getLcdBrightness() { return _lcd_brightness; };
        uint8_t getAvatarMaxCount() { return _avatar_count; }
        String getBluetoothDeviceName() { return _bluetooth_device_name; }
        bool getBluetoothReconnect() { return _bluetooth_reconnect; }
        String getAvatarYamlFilename(uint8_t avatar_no) { return _avatar_yamlfiles[avatar_no]; }
        String getServonYamlFilename() { return _servo_yamlfile; }
        bool getServoRandomMode() { return _servo_random_mode; }
        uint32_t getAutoPowerOffTime() { return _auto_power_off_time; }
        uint8_t getLedLR() { return _led_lr; }
        bool getTakaoBase() { return _takao_base; }
        uint32_t getAvatarSwingInterval() { return _avatar_swing_interval; }
};

#endif // _IMAGEAVATAR_SERVO_CONFIG_
