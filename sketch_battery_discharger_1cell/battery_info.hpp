#pragma once

#include <Arduino.h>
#include <vector>

#include "discharger_define.hpp"

class DrawAdafruit;
class SaveConfigData;
class BatteryController;

enum class TimeStatus : uint8_t
{
  None = 0,
  Active = 1,
  SleepStart = 2,
  SleepStartRead = 3,
  SleepEnd = 4,
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

enum class BatteryConfigSettingMode : uint8_t
{
  DischargeVSetting, // 電圧値
  DischargeISetting, // 放電電流値
  ModeChangeSetting, // 放電モード
  ReduceModeChangeSetting, // 絞り放電の絞り強さ
  HoldMinSetting, // 放電後の、電圧保持する時間（分）
  Max,
};

void setDisplayTuneMenu(DrawAdafruit &adafruit, String &&title, std::vector<String> &menuList, std::vector<String> &valueList, int targetIndex);

class ValueCounter
{
  unsigned long _totalValue{0};

  int _count{0};

public:
  void readVolt(int inVolt)
  {
    _totalValue += inVolt;
    _count += 1;
  }

  void reset()
  {
    _totalValue = 0;
    _count = 0;
  }

  unsigned long calcValue()
  {
    if (_count == 0)
    {
      return 0;
    }
    unsigned long result{_totalValue / _count};
    reset();
    return result;
  }
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

  void setDisplayBatteryConfig(int index, BatteryConfigSettingMode settingMode) const;
};

enum class BatteryStatus : uint8_t
{
  None,
  Active,
  Sleep,
  Stop,
  NoBat,
  Max,
};

class BatteryInfo
{
  static constexpr int START_LINE{1};
  static constexpr int SETTING_MENU_START_COL{7};
  static constexpr int SETTING_MENU_OFFSET_COL{5};

  static constexpr float ACTIVE_RATE{3.f / 4.f};

  static constexpr int8_t DISCHARGE_MODE_LOOPS[] = {
    2, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    3, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

    4, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

    1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

    1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 };

  static constexpr int8_t NONE_MODE_LOOPS[] = {
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0,

    1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0,

    1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0,

    1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 };

  unsigned long _loopCount{0};

  // 点滅表示用
  mutable unsigned long _displayCount{0};

  TimeStatus _currentTimeStatus{TimeStatus::Active};

  static float calcI(const float targetI, const float v, const float targetV, const ReduceMode reduceMode);

  static int calcPWMValue(float ampere, float activeRate, float calibI);

public:
  BatteryInfo(uint8_t inReadPin, uint8_t inWritePin, uint8_t inBatteryIndex)
      : _batteryIndex{inBatteryIndex}, _readPin{inReadPin}, _writePin{inWritePin} {};

  void read();

  void setup();

  void loopSubPushDischarge();

  void loopSubNormalDischarge();

  void writePinReset() const;

  void miniReset()
  {
    _tunedI = -1;
    _i = 0;
    _loopCount = 0;
  };

  void reset()
  {
    miniReset();
    _milliAmpereHour = 0.f;
    _startMillis = millis();
    _endMillis = millis();
    _startSeconds = 0;
    _endSeconds = 0;
    _dischargedCount = 0;
    _milliAmpereHour = 0;
    _ohm = 0;
  }

  void pushOn(float inI)
  {
    if (_tunedI == 0.f)
    {
      _startMillis = millis();
    }
    _tunedI = inI;
  }

  void pushOff()
  {
    _tunedI = 0.f;
  }

  void changeActive(int shift)
  {
    if (_activeFlag)
    {
      _activeFlag = false;
    }
    else
    {
      _nextBatteryStatus = BatteryStatus::None;
      reset();
      _activeFlag = true;
    }
  }

  void setDisplayPushData() const;

  void setDisplayData() const;

  void setDisplayVoltOnly() const;

  void setDisplayDetail() const;

  ValueCounter _valueCounter{};

  BatteryStatus _currentBatteryStatus{BatteryStatus::None};
  BatteryStatus _nextBatteryStatus{BatteryStatus::None};

  bool _displayFlag{false};
  bool _activeFlag{false};

  uint8_t _batteryIndex{0};
  uint8_t _readPin{0};
  uint8_t _writePin{0};
  unsigned long _startMillis{0}; // 放電開始時刻
  unsigned long _endMillis{0}; // 放電終了時刻
  int _startSeconds{0};
  int _endSeconds{0};

  int _dischargedCount{0}; // 放電停止した回数
  float _v{0.f};
  float _sleepV{0.f};
  float _targetV{1.40f};
  float _i{0.0f};
  float _tunedI{1.f};
  float _ohm{0.f};
  float _targetI{0.2f};
  float _milliAmpereHour{0.0f};
  unsigned long _ampereHourTime{0};
  DisChargeMode _disChargeMode{DisChargeMode::DischargeHold};
  ReduceMode _reduceMode{ReduceMode::Normal};
  int _holdMin{30};

  SaveBattery* _saveBattery{nullptr};

  const BatteryController* _batteryController{nullptr};
};
