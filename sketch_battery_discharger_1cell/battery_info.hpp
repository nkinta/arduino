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

enum class DisChargeMode : uint8_t
{
  DischargeNormal,
  DischargeStop,
  Max,
};

enum class BatteryConfigSettingMode : uint8_t
{
  DischargeVSetting,
  DischargeISetting,
  ModeChangeSetting,
  Max,
};

void setDisplayTuneMenu(DrawAdafruit &adafruit, String &&title, std::vector<String> &menuList, std::vector<String> &valueList, int targetIndex);

class ValueCounter
{
  unsigned long totalValue{0};

  int count{0};

public:
  void readVolt(int inVolt)
  {
    totalValue += inVolt;
    count += 1;
  }

  void reset()
  {
    totalValue = 0;
    count = 0;
  }

  unsigned long calcValue()
  {
    if (count == 0)
    {
      return 0;
    }
    unsigned long result{totalValue / count};
    reset();
    return result;
  }
};

struct SaveBattery
{

  static constexpr float TARGET_V_MAX{1.60f};
  static constexpr float TARGET_V_MIN{0.9f};
  static constexpr float TARGET_I_MAX{2.0f}; // 2SK4017 MOSFETの特性上 0.1Aの時に2.24V 0.5Aの時に2.6V 1.0Aの時に2.8V 2Aの時にMOSのゲート電圧が3.2V
  static constexpr float TARGET_I_MIN{0.4f};

  float targetV{1.4f};
  float targetI{1.f};
  DisChargeMode disChargeMode{DisChargeMode::DischargeNormal};
  bool padding{true};

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

  unsigned long loopCount{0};

  // チカチカ表示用
  mutable unsigned long displayCount{0};

  TimeStatus currentTimeStatus{TimeStatus::Active};

  static float calcI(const float targetI, const float V, const float targetV, const DisChargeMode disChargeMode);

  static int calcPWMValue(float ampere, float active_rate, float calibI);

public:
  BatteryInfo(uint8_t inReadPin, uint8_t inWritePin, uint8_t inBatteryIndex)
      : readPin{inReadPin}, writePin{inWritePin}, batteryIndex{inBatteryIndex} {};

  void read();

  void setup();

  void loopSubPushDischarge();

  void loopSubNormalDischarge();

  void writePinReset() const;

  void miniReset()
  {
    tunedI = -1;
    I = 0;
    loopCount = 0;
  };

  void reset()
  {
    miniReset();
    milliAmpereHour = 0.f;
    startMillis = millis();
    endMillis = millis();
    startSeconds = 0;
    endSeconds = 0;
    dischargedCount = 0;
    milliAmpereHour = 0;
    ohm = 0;
  }

  void pushOn(float inI)
  {
    if (tunedI == 0.f)
    {
      startMillis = millis();
    }
    tunedI = inI;
  }

  void pushOff()
  {
    tunedI = 0.f;
  }

  void changeActive(int shift)
  {
    if (activeFlag)
    {
      activeFlag = false;
    }
    else
    {
      reset();
      activeFlag = true;
    }
  }

  void setDisplayPushData() const;

  void setDisplayData() const;

  void setDisplayVoltOnly() const;

  void setDisplayDetail() const;

  ValueCounter valueCounter{};

  BatteryStatus currentBatteryStatus{BatteryStatus::None};
  BatteryStatus nextBatteryStatus{BatteryStatus::None};

  bool displayFlag{false};
  bool activeFlag{false};

  uint8_t batteryIndex{0};
  uint8_t readPin{0};
  uint8_t writePin{0};
  unsigned long startMillis{0}; // 放電開始時
  unsigned long endMillis{0}; // 放電終了時
  int startSeconds{0};
  int endSeconds{0};

  int dischargedCount{0}; // 放電完了した回数
  float V{0.f};
  float sleepV{0.f};
  float targetV{1.40f};
  float I{0.0f};
  float tunedI{1.f};
  float ohm{0.f};
  float targetI{0.2f};
  float milliAmpereHour{0.0f};
  unsigned long ampereHourTime{0};
  DisChargeMode disChargeMode{DisChargeMode::DischargeNormal};
 
  SaveBattery* saveBattery{nullptr};

  const BatteryController* batteryController{nullptr};


};
