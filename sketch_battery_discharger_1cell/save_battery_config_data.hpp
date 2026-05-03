#pragma once

#include <cstdint>

class Adafruit_SSD1306;

enum class BatteryConfigSettingMode : uint8_t
{
  DischargeVSetting, // 電圧値
  DischargeISetting, // 放電電流値
  ModeChangeSetting, // 放電モード
  ReduceModeChangeSetting, // 絞り放電の絞り強さ
  HoldMinSetting, // 放電後の、電圧保持する時間（分）
  Max,
};

// 放電モード
enum class DisChargeMode : uint8_t
{
  DischargeHold, // 放電後、電圧値を保持する
  DischargeHoldMin, // 放電後、（指定分）電圧値を保持する
  DischargeStop, // 放電後、電圧を保持せず終了する
  Max,
};

// 絞り放電
enum class ReduceMode : uint8_t
{
  Soft, // 絞り、緩やか 
  Normal, // 絞り、標準
  Hard, // 絞り、急
  None, // 絞らない
  Max,
};

struct SaveBattery
{
    static constexpr float TARGET_V_MAX{1.60f};
    static constexpr float TARGET_V_MIN{0.9f};
    static constexpr float TARGET_I_MAX{2.0f}; // 2SK4017 MOSFET の特性上、0.1A で約 2.24V、0.5A で約 2.6V、1.0A で約 2.8V、2A でゲート電圧は約 3.2V
    static constexpr float TARGET_I_MIN{0.4f};
    static constexpr int HOLDMIN_MIN{1};
    static constexpr int HOLDMIN_MAX{180};

    float _targetV{1.4f};
    float _targetI{1.f};
    DisChargeMode _disChargeMode{DisChargeMode::DischargeHold};
    ReduceMode _reduceMode{ReduceMode::Normal};
    int _holdMin{30};

    bool _padding{true};

    void shiftParam(BatteryConfigSettingMode settingMode, int shift);

    void setDisplayBatteryConfig(Adafruit_SSD1306 &display, int index, BatteryConfigSettingMode settingMode) const;

};

struct SaveBatteryConfigData
{
    static constexpr int SAVEDATA_ID{0xABCE};
    static constexpr int SAVEDATA_ADDRESS{0X400};
    int _id{SAVEDATA_ID};
    int _ver{2};
    SaveBattery _battery[4];
};
