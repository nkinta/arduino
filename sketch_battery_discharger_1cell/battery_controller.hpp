#pragma once

#include "battery_info.hpp"
#include "discharger_define.hpp"
#include "config.hpp"

static constexpr float FPS{30.f};
static constexpr float SEC{1000.f};
static constexpr float ONE_FRAME_MS{(1.f / FPS) * SEC};

#undef OLD_PCB

enum class MainMode : uint8_t
{
  DischargerMode,
  PushDischargerMode,
  ConfigMode,
  BatteryConfigMode,
  Max,
};

enum class ConfigSettingMode : uint8_t
{
  Volt00Setting,
  Volt05Setting,
  Volt10Setting,
  Volt15Setting,
  Volt20Setting,
  LedOnSetting,
  discISetting,
  tuneISetting,
  decimalSetting,
  Max,
};

struct ButtonStatus
{

  static constexpr int LONG_PUSH_MILLIS{1000};

  void init(const int inPinID)
  {
    pinID = inPinID;
  }

  int check()
  {
    int val = digitalRead(pinID);
    if (val == LOW)
    {
      buttonFlag = true;
    }
    else
    {
      buttonFlag = false;
    }

    bool releaseFlag{false};

    if (buttonFlag && (buttonOldFlag != buttonFlag))
    {
      pushedMillis = millis();
    }
    else if (!buttonFlag && (buttonOldFlag != buttonFlag))
    {
      releaseFlag = true;
    }
    buttonOldFlag = buttonFlag;

    if (releaseFlag)
    {
      if ((millis() - pushedMillis) > LONG_PUSH_MILLIS)
      {
        return 2;
      }
      else
      {
        return 1;
      }
    }
    else if (buttonFlag)
    {
      if ((millis() - pushedMillis) > LONG_PUSH_MILLIS)
      {
        return 3;
      }
      else
      {
        return 4;
      }
    }

    return 0;
  }

  int pinID{0};
  bool buttonFlag{false};
  bool buttonOldFlag{false};
  unsigned long pushedMillis{0};
};

class BatteryController
{

  static constexpr uint8_t XIAO_READ_BAT{PD4};
  static constexpr uint8_t XIAO_READ_BAT_SWITCH{PD3};

#if OLD_PCB
  static constexpr uint8_t READ1_PIN{0};
  static constexpr uint8_t READ2_PIN{1};
#else
  static constexpr uint8_t READ1_PIN{18};
  static constexpr uint8_t READ2_PIN{17};
#endif
  static constexpr uint8_t READ3_PIN{6};
  static constexpr uint8_t READ4_PIN{7};

  static constexpr uint8_t WRITE1_PIN{2};
  static constexpr uint8_t WRITE2_PIN{3};
  static constexpr uint8_t WRITE3_PIN{8};
  static constexpr uint8_t WRITE4_PIN{9};

#if OLD_PCB
  static constexpr int PUSH_BUTTON_L{15};
  static constexpr int PUSH_BUTTON_D{14};
  static constexpr int PUSH_BUTTON_U{13}; // Arduino15\packages\SiliconLabs\hardware\silabs\3.0.0\variants\xiao_mg24\pins_arduino.h // DEEP_SLEEP_ESCAPE_PIN
  static constexpr int PUSH_BUTTON_R{12}; // 12(SAND11_RX) or 10
  static constexpr int PUSH_BUTTON_C{11};

  static constexpr int WAKE_UP_PIN{PUSH_BUTTON_D};

  ButtonStatus buttonLStatus{};
  ButtonStatus buttonRStatus{};
  ButtonStatus buttonUStatus{};
  ButtonStatus buttonDStatus{};
  ButtonStatus buttonCStatus{};
#else
  static constexpr int PUSH_BUTTON_L{15}; // 1
  static constexpr int PUSH_BUTTON_D{0};  //
  static constexpr int PUSH_BUTTON_U{10}; //
  static constexpr int PUSH_BUTTON_R{13}; // 3 // Arduino15\packages\SiliconLabs\hardware\silabs\3.0.0\variants\xiao_mg24\pins_arduino.h // DEEP_SLEEP_ESCAPE_PIN
  static constexpr int PUSH_BUTTON_C{14}; // 2
  static constexpr int PUSH_BUTTON_4{16}; // 4
  static constexpr int PUSH_BUTTON_ON{1};

  static constexpr int WAKE_UP_PIN{PUSH_BUTTON_L};

  ButtonStatus buttonLStatus{};
  ButtonStatus buttonRStatus{};
  ButtonStatus buttonUStatus{};
  ButtonStatus buttonDStatus{};
  ButtonStatus buttonCStatus{};

  ButtonStatus buttonONStatus{};
  ButtonStatus button4Status{};
#endif

  std::vector<BatteryInfo> batteryStatuses{
      BatteryInfo{READ1_PIN, WRITE1_PIN, 0},
      BatteryInfo{READ2_PIN, WRITE2_PIN, 1},
      BatteryInfo{READ3_PIN, WRITE3_PIN, 2},
      BatteryInfo{READ4_PIN, WRITE4_PIN, 3}};

  ConfigSettingMode configSettingMode{ConfigSettingMode::Volt00Setting};
  BatteryConfigSettingMode batteryConfigSettingMode{BatteryConfigSettingMode::DischargeVSetting};

  unsigned long loopSubMillis{0};

  size_t currentBatteryIndex{0};

  size_t currentBatterySettingIndex{0};

  unsigned long loopSubCount{0};

  size_t batteryConfigNum{2};

  uint8_t ledOnFlag{0};

  float dischargeI{2.f};

  float customAmpTune{1.f};

  bool xiaoVoltFlag{true};

  struct SaveBatteryConfigData
  {
    static constexpr int SAVEDATA_ADDRESS{0X400};
    int id{SAVEDATA_ID};
    int ver{1};
    SaveBattery battery[4];
  };

public:
  BatteryController()
  {
    batteryStatuses[0].saveBattery = &(saveBatteryConfigData.battery[0]);
    batteryStatuses[1].saveBattery = &(saveBatteryConfigData.battery[0]);
    batteryStatuses[2].saveBattery = &(saveBatteryConfigData.battery[1]);
    batteryStatuses[3].saveBattery = &(saveBatteryConfigData.battery[1]);

    for (auto &batteryStatus : batteryStatuses)
    {
      batteryStatus.saveConfigData = &saveConfigData;
    }

    batteryStatuses[currentBatteryIndex].displayFlag = true;
  }

  SaveBatteryConfigData saveBatteryConfigData{};

  SaveConfigData saveConfigData{};

  MainMode mainMode{MainMode::DischargerMode};

private:
  float readAndDrawXiaoBattery();

  void saveConfig();

  void saveMain();

  void loadConfig();

  void loadMain();

public:
  void clearEEPROM();

void setup();

  void updateBatterySaveData();

  void updateConfigSaveData();

  void loopMain()
  {
    for (auto &batteryStatus : batteryStatuses)
    {
      batteryStatus.read();
    }
  };

  void setDisplayConfig();

  void setDisplayBatteryConfig()
  {
    saveBatteryConfigData.battery[currentBatterySettingIndex].setDisplayBatteryConfig(currentBatterySettingIndex, batteryConfigSettingMode);
  }

  void setDisplayPushDischarge();

  void setDisplayData();

  void goDeepSleep();

  void updateButtonStatus();

  void changeTargetBatterySetting(int shift)
  {
    currentBatterySettingIndex = (currentBatterySettingIndex + shift) % batteryConfigNum;
  }

  void shiftTargetBattery(int shift);

  void changeTargetBattery(int batteryIndex);

  void changeSettingMode(int shift);

  void changeActive(int shift)
  {
    batteryStatuses[currentBatteryIndex].changeActive(shift);
  };

    void shiftParam(int shift);

    void loopSub();

  void loopWhile()
  {

    loopMain();

    const unsigned long tempMillis{millis()};
    if (tempMillis - loopSubMillis > ONE_FRAME_MS)
    {
      loopSub();
      loopSubMillis = tempMillis;
    }
  };
};

