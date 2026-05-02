#pragma once

#include <cstdint>

class DrawAdafruit;

enum class ConfigSettingMode : uint8_t
{
  tuneVolt00Setting, // 0.0V付近の電圧値のキャリブレーション
  tuneVolt05Setting, // 0.5V付近の電圧値のキャリブレーション
  tuneVolt10Setting, // 1.0V付近の電圧値のキャリブレーション
  tuneVolt15Setting, // 1.5V付近の電圧値のキャリブレーション
  tuneVolt20Setting, // 2.0V付近の電圧値のキャリブレーション
  LedOnSetting,      // 放電機能が正常時にLEDをチカチカさせる
  discISetting,      // 放電用電流
  tuneISetting,      // 電流値のキャリブレーション
  decimalSetting,    // 小数点何桁まで表示するか
  Max,
};

struct SaveConfigData
{
  static constexpr int SAVEDATA_ID{0xABCE};
  static constexpr int8_t VOLT_DATA_SIZE{5};
  static constexpr int SAVEDATA_ADDRESS{0X100};
  static constexpr int VOLT_RANGE{100};
  static int voltClamp(int value);

  int _id{SAVEDATA_ID};
  int _ver{5};
  int _voltDatas[VOLT_DATA_SIZE] = {-10, 0, 0, 0, 0}; // 電圧キャリブレーション
  uint8_t _ledOnFlag{0};
  float _dischargeI{2.f};
  float _calibI{1.f};
  int _decimal{3};

  void shiftParam(const ConfigSettingMode &configMode, int shift);

  void setDisplayConfig(DrawAdafruit &drawAdafruit, ConfigSettingMode settingMode) const;

  // DrawAdafruit &adafruit,
};
