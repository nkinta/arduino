#pragma once

struct SaveConfigData
{
  static constexpr int8_t VOLT_DATA_SIZE{5};
  static constexpr int SAVEDATA_ADDRESS{0X100};
  static constexpr int VOLT_RANGE{100};
  static int voltClamp(int value)
  {
    return std::clamp(value, -1 * VOLT_RANGE, VOLT_RANGE);
  };
  int _id{SAVEDATA_ID};
  int _ver{5};
  int _voltDatas[VOLT_DATA_SIZE] = {-10, 0, 0, 0, 0};
  uint8_t _ledOnFlag{0};
  float _dischargeI{2.f};
  float _calibI{1.f};
  int _decimal{3};
};

struct SaveBatteryConfigData
{
  static constexpr int SAVEDATA_ADDRESS{0X400};
  int _id{SAVEDATA_ID};
  int _ver{2};
  SaveBattery _battery[4];
};
