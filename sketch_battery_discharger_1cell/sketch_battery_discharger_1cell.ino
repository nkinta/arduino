#include <SPI.h>
#include <Wire.h>
#include <vector>
#include <EEPROM.h>
#include <stdio.h>
#include <string>
#include "battery_info.h"
#include "discharger_define.h"

// #define DEEP_SLEEP_ESCAPE_PIN   D13
#include <ArduinoLowPower.h>

#undef SERIAL_DEBUG_ON
// #define SERIAL_DEBUG_ON

#include "display/draw_adafruit.h"

#undef OLD_PCB

DrawAdafruit drawAdafruit;

void callback()
{
  int count{};
  count++;
}

VoltageMapping voltageMapping;

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

static constexpr float FPS{30.f};
static constexpr float SEC{1000.f};
static constexpr float ONE_FRAME_MS{(1.f / FPS) * SEC};

int modeIndex = 0;

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
  float readAndDrawXiaoBattery()
  {
    constexpr static float R_RATE{2.f};
    const int readValue{analogRead(XIAO_READ_BAT)};
    const float xiaoVolt{(static_cast<float>(readValue) / 4096.f) * R_RATE * VOLT3_3};
    // const float xiaoVolt{ (2048.f / 4096.f) * R_RATE * VOLT3_3 };
    drawAdafruit.drawBat(xiaoVolt);

    return xiaoVolt;
  }

  template <typename T>
  void saveCustomData(byte *p)
  {
    for (int i = 0; i < sizeof(T); i++)
    {
      EEPROM.write(T::SAVEDATA_ADDRESS + i, *p);
      p++;
    }
  }

  template <typename T>
  void loadCustomData(byte *p)
  {
    for (int i = 0; i < sizeof(T); i++)
    {
      byte b = EEPROM.read(T::SAVEDATA_ADDRESS + i);
      *p = b;
      p++;
    }
  }

  void saveConfig()
  {
    saveConfigData.id = SAVEDATA_ID;
    saveCustomData<SaveConfigData>((byte *)&saveConfigData);
  }

  void saveMain()
  {
    saveBatteryConfigData.id = SAVEDATA_ID;
    saveCustomData<SaveBatteryConfigData>((byte *)&saveBatteryConfigData);
  }

  void loadConfig()
  {
    SaveConfigData tempData;
    loadCustomData<SaveConfigData>((byte *)&tempData);
    if (tempData.id != SAVEDATA_ID)
    {
      return;
    }
    if (tempData.ver != saveConfigData.ver)
    {
      return;
    }
    saveConfigData = tempData;
  }

  void loadMain()
  {
    SaveBatteryConfigData tempData;
    loadCustomData<SaveBatteryConfigData>((byte *)&tempData);
    if (tempData.id != SAVEDATA_ID)
    {
      return;
    }
    if (tempData.ver != saveBatteryConfigData.ver)
    {
      return;
    }
    saveBatteryConfigData = tempData;
  };

public:
  void clearEEPROM()
  {
    const uint8_t clearSize{64};
    for (int i = 0; i < clearSize; ++i)
    {
      EEPROM.write(i, 0xFF);
    }
  }

  void setup()
  {

    // BatteryReadSetting
    pinMode(XIAO_READ_BAT_SWITCH, OUTPUT);
    digitalWrite(XIAO_READ_BAT_SWITCH, HIGH);
    pinMode(XIAO_READ_BAT, INPUT);

    // Button
    pinMode(PUSH_BUTTON_L, INPUT_PULLUP);
    pinMode(PUSH_BUTTON_D, INPUT_PULLUP);
    pinMode(PUSH_BUTTON_U, INPUT_PULLUP);
    pinMode(PUSH_BUTTON_R, INPUT_PULLUP);
    pinMode(PUSH_BUTTON_C, INPUT_PULLUP);

    /*
    pinMode(READ1_PIN, INPUT);
    pinMode(READ2_PIN, INPUT);
    pinMode(READ3_PIN, INPUT);
    pinMode(READ4_PIN, INPUT);

    pinMode(WRITE1_PIN, OUTPUT);
    pinMode(WRITE2_PIN, OUTPUT);
    pinMode(WRITE3_PIN, OUTPUT);
    pinMode(WRITE4_PIN, OUTPUT);
    */
    digitalWrite(PA6, HIGH); // FLASH

    buttonLStatus.init(PUSH_BUTTON_L);
    buttonRStatus.init(PUSH_BUTTON_R);
    buttonUStatus.init(PUSH_BUTTON_U);
    buttonDStatus.init(PUSH_BUTTON_D);
    buttonCStatus.init(PUSH_BUTTON_C);

#if OLD_PCB
    int val{HIGH};
    val = digitalRead(PUSH_BUTTON_D);
    if (val != LOW) // val == LOW)
    {
      loadMain();
      loadConfig();
    }

#else
    int val{HIGH};
    val = digitalRead(PUSH_BUTTON_C);
    if (val != LOW) // val == LOW)
    {
      loadMain();
      loadConfig();
    }

    pinMode(PUSH_BUTTON_ON, INPUT_PULLUP);
    pinMode(PUSH_BUTTON_4, INPUT_PULLUP);

    buttonONStatus.init(PUSH_BUTTON_ON);
    button4Status.init(PUSH_BUTTON_4);
#endif

    updateBatterySaveData();
    updateConfigSaveData();
  };

  void updateBatterySaveData()
  {
    for (int i{0}; i < batteryStatuses.size(); ++i)
    {
      auto &batteryStatus{batteryStatuses[i]};

      const static SaveBattery defaultSaveBattery{};
      auto *saveBattery{batteryStatus.saveBattery};
      if (saveBatteryConfigData.id != SAVEDATA_ID)
      {
        *saveBattery = defaultSaveBattery;
      }

      batteryStatus.targetI = saveBattery->targetI;
      batteryStatus.targetV = saveBattery->targetV;
      batteryStatus.disChargeMode = saveBattery->disChargeMode;
      // batteryStatus.activeFlag = saveBattery->activeFlag;

      batteryStatus.setup();
    }
  }

  void updateConfigSaveData()
  {

    const static SaveConfigData defaultSaveConfigData{};
    if (saveConfigData.id != SAVEDATA_ID)
    {
      saveConfigData = defaultSaveConfigData;
    }

    std::vector<int> customMappingData{};
    for (uint8_t i{0}; i < SaveConfigData::VOLT_DATA_SIZE; ++i)
    {
      customMappingData.push_back(saveConfigData.voltDatas[i]);
    }
    voltageMapping.initMapping(customMappingData);

    ledOnFlag = saveConfigData.ledOnFlag;
    customAmpTune = saveConfigData.customAmpTune;
    dischargeI = saveConfigData.dischargeI;
  }

  void loopMain()
  {
    for (auto &batteryStatus : batteryStatuses)
    {
      batteryStatus.read();
    }
  };

  void setDisplayConfig()
  {
    std::vector<String> menuList{"0.0V", "0.5V", "1.0V", "1.5V", "2.0V", "LedOn", "DiscI", "AmpTune", "Decimal"};

    std::vector<String> valueList{
        String(saveConfigData.voltDatas[0]),
        String(saveConfigData.voltDatas[1]),
        String(saveConfigData.voltDatas[2]),
        String(saveConfigData.voltDatas[3]),
        String(saveConfigData.voltDatas[4]),
        String(saveConfigData.ledOnFlag == 0 ? false : true),
        String(saveConfigData.dischargeI),
        String(saveConfigData.customAmpTune),
        String(saveConfigData.decimal),
    };

    setDisplayTuneMenu(drawAdafruit, "Config", menuList, valueList, static_cast<int>(configSettingMode));
  }

  void setDisplayBatteryConfig()
  {
    saveBatteryConfigData.battery[currentBatterySettingIndex].setDisplayBatteryConfig(currentBatterySettingIndex, batteryConfigSettingMode);
  }

  void setDisplayPushDischarge()
  {

    for (int i = 0; i < 6; ++i)
    {
      drawAdafruit.drawFillLine(i);
    }
    drawAdafruit.drawStringC(String("Discharge ") + String(dischargeI) + String("A"), 0);
    for (auto &batteryStatus : batteryStatuses)
    {
      batteryStatus.setDisplayPushData();
    }

    int line{4};
    int vir_offset{0};
    if (0)
    {
      float lV{batteryStatuses[0].V + batteryStatuses[1].V};
      float rV{batteryStatuses[2].V + batteryStatuses[3].V};

      vir_offset += 10;
      drawAdafruit.drawFloatR(lV, vir_offset, line, 4, saveConfigData.decimal);
      drawAdafruit.drawString("V", vir_offset, line);
      vir_offset += 10;
      drawAdafruit.drawFloatR(rV, vir_offset, line, 4, saveConfigData.decimal);
      drawAdafruit.drawString("V", vir_offset, line);
    }

    line += 1;
    vir_offset = 18;
    const BatteryInfo *targetBatteryStatus{nullptr};
    for (auto &batteryStatus : batteryStatuses)
    {
      if (batteryStatus.tunedI > 0.f)
      {
        targetBatteryStatus = &batteryStatus;
        continue;
      }
    }
    if (targetBatteryStatus)
    {
      drawAdafruit.drawFloatR(targetBatteryStatus->ohm, vir_offset, line, 4, 1);
      static constexpr char CHAR_DATA_OHM[] = {0x6D, 0xe9, 0x00};
      drawAdafruit.drawChar(&CHAR_DATA_OHM[0], vir_offset, line);
    }
  }

  void setDisplayData()
  {

    drawAdafruit.drawFillLine(0);
    for (auto &batteryStatus : batteryStatuses)
    {
      batteryStatus.setDisplayData();
    }
  };

  void goDeepSleep()
  {
    drawAdafruit.clearDisplay();
    drawAdafruit.display();
    LowPower.attachInterruptWakeup(WAKE_UP_PIN, callback, RISING);

    LowPower.deepSleep(20 * 1000); // 7 * 24 * 3600 * 1000 // one week
  }

  void updateButtonStatus()
  {
    int checkFlag{0};

    bool buttonUFlag{false};
    bool buttonDFlag{false};
    int numBattery{0};

    checkFlag = buttonLStatus.check();
    numBattery = 0;
    if (checkFlag == 1)
    {
      if (mainMode == MainMode::DischargerMode)
      {
        shiftTargetBattery(-1);
      }
      else if (mainMode == MainMode::ConfigMode || mainMode == MainMode::BatteryConfigMode)
      {
        shiftParam(-1);
      }
    }
    else if (checkFlag == 4)
    {
      if (mainMode == MainMode::PushDischargerMode)
      {
        batteryStatuses[numBattery].pushOn(dischargeI);
      }
    }
    else if (checkFlag == 3)
    {
      if (mainMode == MainMode::ConfigMode || mainMode == MainMode::BatteryConfigMode)
      {
        shiftParam(-1);
      }
    }
    else if (checkFlag == 0)
    {
      if (mainMode == MainMode::PushDischargerMode)
      {
        batteryStatuses[numBattery].pushOff();
      }
    }

    checkFlag = buttonRStatus.check();
    numBattery = 2;
    if (checkFlag == 1)
    {
      if (mainMode == MainMode::DischargerMode)
      {
        shiftTargetBattery(1);
      }
      else if (mainMode == MainMode::ConfigMode || mainMode == MainMode::BatteryConfigMode)
      {
        shiftParam(1);
      }
    }
    else if (checkFlag == 4)
    {
      if (mainMode == MainMode::PushDischargerMode)
      {
        batteryStatuses[numBattery].pushOn(dischargeI);
      }
    }
    else if (checkFlag == 3)
    {
      if (mainMode == MainMode::ConfigMode || mainMode == MainMode::BatteryConfigMode)
      {
        shiftParam(1);
      }
    }
    else if (checkFlag == 0)
    {
      if (mainMode == MainMode::PushDischargerMode)
      {
        batteryStatuses[numBattery].pushOff();
      }
    }

    checkFlag = buttonUStatus.check();
    if (checkFlag == 1)
    {
      changeSettingMode(-1);
    }
    else if (checkFlag == 3 || checkFlag == 4)
    {
      buttonUFlag = true;
    }

    checkFlag = buttonDStatus.check();
    if (checkFlag == 1)
    {
      changeSettingMode(1);
    }
    else if (checkFlag == 2)
    {
    }
    else if (checkFlag == 3 || checkFlag == 4)
    {
      buttonDFlag = true;
    }

#if OLD_PCB
#else
    checkFlag = buttonONStatus.check();
    if (checkFlag == 1)
    {
      if (mainMode == MainMode::PushDischargerMode)
      {
        mainMode = MainMode::DischargerMode;
      }
      else
      {
        mainMode = MainMode::PushDischargerMode;
      }
    }
    else if (checkFlag == 2)
    {
      goDeepSleep();
    }

    checkFlag = button4Status.check();
    numBattery = 3;
    if (checkFlag == 1)
    {
    }
    else if (checkFlag == 2)
    {
    }
    else if (checkFlag == 4)
    {
      if (mainMode == MainMode::PushDischargerMode)
      {
        batteryStatuses[numBattery].pushOn(dischargeI);
      }
    }
    else if (checkFlag == 0)
    {
      if (mainMode == MainMode::PushDischargerMode)
      {
        batteryStatuses[numBattery].pushOff();
      }
    }

#endif

    checkFlag = buttonCStatus.check();
    numBattery = 1;
    if (checkFlag == 1)
    {
      if (mainMode == MainMode::DischargerMode)
      {
        changeActive(1);
      }
      else if (mainMode == MainMode::ConfigMode)
      {
      }
      else if (mainMode == MainMode::BatteryConfigMode)
      {
        changeTargetBatterySetting(1);
        shiftTargetBattery(1);
      }
    }
    else if (checkFlag == 2)
    {
      if (mainMode == MainMode::DischargerMode)
      {
        if (batteryConfigNum == 2)
        {
          if (currentBatteryIndex == 0 || currentBatteryIndex == 1)
          {
            currentBatterySettingIndex = 0;
          }
          else if (currentBatteryIndex == 2 || currentBatteryIndex == 3)
          {
            currentBatterySettingIndex = 1;
          }
        }
        mainMode = MainMode::BatteryConfigMode;
      }
      else if (mainMode == MainMode::BatteryConfigMode)
      {
        updateBatterySaveData();
        saveMain();
        mainMode = MainMode::DischargerMode;
      }
      else if (mainMode == MainMode::ConfigMode)
      {
        updateConfigSaveData();
        saveConfig();
        for (auto &batteryStatus : batteryStatuses)
        {
          batteryStatus.reset();
        }
        mainMode = MainMode::DischargerMode;
      }
    }
    else if (checkFlag == 4)
    {
      if (mainMode == MainMode::PushDischargerMode)
      {
        batteryStatuses[numBattery].pushOn(dischargeI);
      }
    }
    else if (checkFlag == 0)
    {
      if (mainMode == MainMode::PushDischargerMode)
      {
        batteryStatuses[numBattery].pushOff();
      }
    }

    if ((buttonUFlag == true) && (buttonDFlag == true))
    {
      if (mainMode == MainMode::DischargerMode || mainMode == MainMode::PushDischargerMode)
      {
        mainMode = MainMode::ConfigMode;
      }
    }
  }

  void changeTargetBatterySetting(int shift)
  {
    currentBatterySettingIndex = (currentBatterySettingIndex + shift) % batteryConfigNum;
  }

  void shiftTargetBattery(int shift)
  {
    currentBatteryIndex = (currentBatteryIndex + shift) % batteryStatuses.size();

    for (size_t index{0}; index < batteryStatuses.size(); ++index)
    {
      if (index == currentBatteryIndex)
      {
        batteryStatuses[index].displayFlag = true;
      }
      else
      {
        batteryStatuses[index].displayFlag = false;
      }
    }
  };

  void changeTargetBattery(int batteryIndex)
  {
    currentBatteryIndex = batteryIndex;

    for (size_t index{0}; index < batteryStatuses.size(); ++index)
    {
      if (index == currentBatteryIndex)
      {
        batteryStatuses[index].displayFlag = true;
      }
      else
      {
        batteryStatuses[index].displayFlag = false;
      }
    }
  };

  void changeSettingMode(int shift)
  {

    if (mainMode == MainMode::BatteryConfigMode)
    {
      const int nextModeIndex{(static_cast<int>(BatteryConfigSettingMode::Max) + static_cast<int>(batteryConfigSettingMode) + shift) % static_cast<int>(BatteryConfigSettingMode::Max)};
      batteryConfigSettingMode = static_cast<BatteryConfigSettingMode>(nextModeIndex);
    }
    else if (mainMode == MainMode::ConfigMode)
    {
      const int nextModeIndex{(static_cast<int>(ConfigSettingMode::Max) + static_cast<int>(configSettingMode) + shift) % static_cast<int>(ConfigSettingMode::Max)};
      configSettingMode = static_cast<ConfigSettingMode>(nextModeIndex);
    }
  };

  void changeActive(int shift)
  {
    batteryStatuses[currentBatteryIndex].changeActive(shift);
  };

  void shiftParam(int shift)
  {
    if (mainMode == MainMode::BatteryConfigMode)
    {
      saveBatteryConfigData.battery[currentBatterySettingIndex].shiftParam(batteryConfigSettingMode, shift);
    }
    else if (mainMode == MainMode::ConfigMode)
    {
      if (configSettingMode == ConfigSettingMode::Volt00Setting)
      {
        saveConfigData.voltDatas[0] = SaveConfigData::voltClamp(saveConfigData.voltDatas[0] + shift);
      }
      else if (configSettingMode == ConfigSettingMode::Volt05Setting)
      {
        saveConfigData.voltDatas[1] = SaveConfigData::voltClamp(saveConfigData.voltDatas[1] + shift);
      }
      else if (configSettingMode == ConfigSettingMode::Volt10Setting)
      {
        saveConfigData.voltDatas[2] = SaveConfigData::voltClamp(saveConfigData.voltDatas[2] + shift);
      }
      else if (configSettingMode == ConfigSettingMode::Volt15Setting)
      {
        saveConfigData.voltDatas[3] = SaveConfigData::voltClamp(saveConfigData.voltDatas[3] + shift);
      }
      else if (configSettingMode == ConfigSettingMode::Volt15Setting)
      {
        saveConfigData.voltDatas[4] = SaveConfigData::voltClamp(saveConfigData.voltDatas[4] + shift);
      }
      else if (configSettingMode == ConfigSettingMode::LedOnSetting)
      {
        saveConfigData.ledOnFlag = ((saveConfigData.ledOnFlag == 0) ? 1 : 0);
      }
      else if (configSettingMode == ConfigSettingMode::discISetting)
      {
        saveConfigData.dischargeI = std::clamp(saveConfigData.dischargeI + (shift * 0.1f), 0.f, 2.4f);
      }
      else if (configSettingMode == ConfigSettingMode::tuneISetting)
      {
        saveConfigData.customAmpTune = std::clamp(saveConfigData.customAmpTune + (shift * 0.1f), 0.8f, 1.2f);
      }
      else if (configSettingMode == ConfigSettingMode::decimalSetting)
      {
        saveConfigData.decimal = std::clamp(saveConfigData.decimal + shift, 2, 3);
      }
    }
  };

  void loopSub()
  {
    ++loopSubCount;
    if ((loopSubCount % 60) == 0)
    {
      const float xiaoVolt{readAndDrawXiaoBattery()};

      if (xiaoVoltFlag)
      {
        if (xiaoVolt < (VOLT3_3 + 0.1f))
        {
          xiaoVoltFlag = false;
        }
      }
      else
      {
        if (xiaoVolt > (VOLT3_3 + 0.2f))
        {
          xiaoVoltFlag = true;
        }
      }
    }

    if (!xiaoVoltFlag)
    {
      drawAdafruit.display();
      return;
    }

    if (ledOnFlag > 0)
    {
      digitalWrite(LED_BUILTIN, ((loopSubCount / 12) % 2));
    }
    else
    {
      digitalWrite(LED_BUILTIN, 1);
    }

#ifdef SERIAL_DEBUG_ON
    if ((loopSubCount % 12) == 0)
    {
      Serial.printf("serial print test. %d\n", loopSubCount);
    }
#endif
    updateButtonStatus();

    if (mainMode == MainMode::DischargerMode)
    {
      for (auto &batteryStatus : batteryStatuses)
      {
        batteryStatus.loopSubNormalDischarge();
      }

      if ((loopSubCount % 3) == 0)
      {
        setDisplayData();
        drawAdafruit.display();
      }
    }
    else if (mainMode == MainMode::BatteryConfigMode)
    {
      if ((loopSubCount % 3) == 0)
      {
        setDisplayBatteryConfig();
        drawAdafruit.display();
      }
    }
    else if (mainMode == MainMode::ConfigMode)
    {
      if ((loopSubCount % 3) == 0)
      {
        setDisplayConfig();
        drawAdafruit.display();
      }
    }
    else if (mainMode == MainMode::PushDischargerMode)
    {
      for (auto &batteryStatus : batteryStatuses)
      {
        batteryStatus.loopSubPushDischarge();
      }

      if ((loopSubCount % 3) == 0)
      {
        setDisplayPushDischarge();
        drawAdafruit.display();
      }
    }
  };

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

BatteryController controller;

// the setup function runs once when you press reset or power the board
void setup()
{
  pinMode(LED_BUILTIN, OUTPUT);

#ifdef SERIAL_DEBUG_ON
  Serial.begin(115200);
  // while (!Serial);
  Serial.print("Start!");
#endif

  drawAdafruit.setupDisplay();
  controller.setup();
}

// the loop function runs over and over again forever
void loop()
{
#if 0
  digitalWrite(LED_BUILTIN, HIGH); // turn the LED on (HIGH is the voltage level)
  delay(100);                      // wait for a second
  digitalWrite(LED_BUILTIN, LOW);  // turn the LED off by making the voltage LOW
  delay(100);    

  return;
#endif

  drawAdafruit.display();

  while (true)
  {
    controller.loopWhile();
  }

  // wait for a second
}