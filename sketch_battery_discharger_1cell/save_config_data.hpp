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
  int id{SAVEDATA_ID};
  int ver{4};
  int voltDatas[VOLT_DATA_SIZE] = {0, 0, 0, 0, 0};
  uint8_t ledOnFlag{0};
  float dischargeI{2.f};
  float calibI{1.f};
  int decimal{3};
};

struct SaveBatteryConfigData
{
  static constexpr int SAVEDATA_ADDRESS{0X400};
  int id{SAVEDATA_ID};
  int ver{1};
  SaveBattery battery[4];
};
