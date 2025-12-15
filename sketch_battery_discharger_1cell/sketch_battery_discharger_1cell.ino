#include <SPI.h>
#include <Wire.h>
#include <vector>
#include <EEPROM.h>
#include <stdio.h>
#include <string>

// #define DEEP_SLEEP_ESCAPE_PIN   D13
#include <ArduinoLowPower.h>

#undef SERIAL_DEBUG_ON

#include "display/draw_adafruit.h"

DrawAdafruit drawAdafruit;

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

struct VoltageMapping
{
  struct VoltPair
  {
    int input{0.f};
    float volt{0};
  };

  const std::vector<VoltPair> defaultMappingData{{0, 0.f}, {621, 0.5f}, {1241, 1.0f}, {1862, 1.5f}, {2482, 2.0f}, {4094, 3.3f}};
  std::vector<VoltPair> mappingData{defaultMappingData};

  float getVoltage(int input)
  {
    const VoltPair *before{nullptr};
    for (const VoltPair &current : mappingData)
    {
      if (before)
      {
        if (before->input < input && input <= current.input)
        {
          return before->volt + ((current.volt - before->volt) / static_cast<float>(current.input - before->input)) * (input - before->input);
        }
      }

      before = &current;
    }
    return 0.f;
  }

  void initMapping(const std::vector<VoltPair>& customMappingData)
  {
    constexpr double epsilon{1e-4};
    mappingData.clear();
    for (const VoltPair& defaultVoltPair : defaultMappingData)
    {
      int customOffset{0};
      for (const VoltPair& customVoltPair : customMappingData)
      {
        if (std::abs(defaultVoltPair.volt - customVoltPair.volt) <= epsilon)
        {
          customOffset = customVoltPair.input;
          break;
        }
      }

      mappingData.push_back({defaultVoltPair.input + customOffset, defaultVoltPair.volt});
    }
  }

};

static VoltageMapping voltageMapping;

enum class DisChargeMode : uint8_t
{
  DischargeNormal,
  DischargeStop,
  Max,
};

enum class MainMode : uint8_t
{
  DischargerMode,
  ConfigMode,
  Max,
};

enum class DisplayMode : uint8_t
{
  None,
  Display,
  Max,
};

enum class MainSettingMode : uint8_t
{
  ModeChangeSetting,
  DischargeVSetting,
  DischargeISetting,
  Max,
};

enum class ConfigSettingMode : uint8_t
{
  Volt00Setting,
  Volt05Setting,
  Volt10Setting,
  Volt15Setting,
  Max,
};


enum class TimeStatus : uint8_t
{
  None = 0,
  Active = 1,
  SleepStart = 2,
  SleepStartRead = 3,
  SleepEnd = 4,
  Max,
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

struct SaveBattery
{

  static constexpr float TARGET_V_MAX{1.55f};
  static constexpr float TARGET_V_MIN{1.0f};
  static constexpr float TARGET_I_MAX{2.0f}; // 2SK4017 MOSFETの特性上 0.1Aの時に2.24V 0.5Aの時に2.6V 1.0Aの時に2.8V 2Aの時にMOSのゲート電圧が3.2V
  static constexpr float TARGET_I_MIN{0.4f};

  float targetV{1.4f};
  float targetI{1.f};
  DisChargeMode disChargeMode{DisChargeMode::DischargeNormal};
  bool activeFlag{true};

  void shiftParam(MainSettingMode settingMode, int shift)
  {
    if (settingMode == MainSettingMode::ModeChangeSetting)
    {
      const int nextModeIndex{(static_cast<int>(DisChargeMode::Max) + static_cast<int>(disChargeMode) + shift) % static_cast<int>(DisChargeMode::Max)};
      disChargeMode = static_cast<DisChargeMode>(nextModeIndex);
    }
    else if (settingMode == MainSettingMode::DischargeVSetting)
    {
      targetV = std::clamp(targetV + 0.001f * static_cast<float>(shift), TARGET_V_MIN, TARGET_V_MAX);
    }
    else if (settingMode == MainSettingMode::DischargeISetting)
    {
      targetI = std::clamp(targetI + 0.1f * static_cast<float>(shift), TARGET_I_MIN, TARGET_I_MAX);
    }
  }
};

namespace DisplayConst {

static constexpr char CHAR_DATA_UP[] = {0x20, 0x20, 0x20, 0x18, 0x00};
static constexpr char CHAR_DATA_DOWN[] = {0x20, 0x20, 0x20, 0x19, 0x00};

static constexpr int START_LINE{1};
static constexpr int SETTING_MENU_START_COL{7};
static constexpr int SETTING_MENU_OFFSET_COL{5};

}

static void displayDischargeInt(DrawAdafruit& adafruit, String&& title , String&& unit, int value)
{
  int vir_offset{0};
  int line{DisplayConst::START_LINE};
  adafruit.drawFillLine(line);
  vir_offset = DisplayConst::SETTING_MENU_START_COL;
  adafruit.drawString(title, vir_offset, line);

  ++line;
  adafruit.drawFillLine(line);
  vir_offset = DisplayConst::SETTING_MENU_START_COL;
  adafruit.drawChar(&DisplayConst::CHAR_DATA_UP[0], vir_offset, line);

  ++line;
  adafruit.drawFillLine(line);
  vir_offset = DisplayConst::SETTING_MENU_START_COL;
  vir_offset += DisplayConst::SETTING_MENU_OFFSET_COL;
  adafruit.drawIntR(value, vir_offset, line);
  adafruit.drawString(unit, vir_offset, line);

  ++line;
  adafruit.drawFillLine(line);
  vir_offset = DisplayConst::SETTING_MENU_START_COL;
  adafruit.drawChar(&DisplayConst::CHAR_DATA_DOWN[0], vir_offset, line);

  ++line;
  adafruit.drawFillLine(line);
  ++line;
  adafruit.drawFillLine(line);
}

static void displayDischargeFloat(DrawAdafruit& adafruit, String&& title , String&& unit, float value)
{
  int vir_offset{0};
  int line{DisplayConst::START_LINE};
  adafruit.drawFillLine(line);
  vir_offset = DisplayConst::SETTING_MENU_START_COL;
  adafruit.drawString(title, vir_offset, line);

  ++line;
  adafruit.drawFillLine(line);
  vir_offset = DisplayConst::SETTING_MENU_START_COL;
  adafruit.drawChar(&DisplayConst::CHAR_DATA_UP[0], vir_offset, line);

  ++line;
  adafruit.drawFillLine(line);
  vir_offset = DisplayConst::SETTING_MENU_START_COL;
  vir_offset += DisplayConst::SETTING_MENU_OFFSET_COL;
  adafruit.drawFloatR(value, vir_offset, line, DisplayConst::SETTING_MENU_OFFSET_COL, 3);
  adafruit.drawString(unit, vir_offset, line);

  ++line;
  adafruit.drawFillLine(line);
  vir_offset = DisplayConst::SETTING_MENU_START_COL;
  adafruit.drawChar(&DisplayConst::CHAR_DATA_DOWN[0], vir_offset, line);

  ++line;
  adafruit.drawFillLine(line);
  ++line;
  adafruit.drawFillLine(line);
}



struct BatteryInfo
{
  static constexpr int START_LINE{1};
  static constexpr int SETTING_MENU_START_COL{7};
  static constexpr int SETTING_MENU_OFFSET_COL{5};

  static constexpr char CHAR_DATA_UP[] = {0x20, 0x20, 0x20, 0x18, 0x00};
  static constexpr char CHAR_DATA_DOWN[] = {0x20, 0x20, 0x20, 0x19, 0x00};

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
  unsigned long displayCount{0};

  TimeStatus currentTimeStatus{TimeStatus::Active};

  static float calcI(const float targetI, const float V, const float targetV, const DisChargeMode disChargeMode)
  {
    float resultI{0};
    if (disChargeMode == DisChargeMode::DischargeNormal)
    {
      if (V - targetV > 0.01f)
      {
        resultI = targetI;
      }
      else if (V - targetV > 0.004f)
      {
        resultI = targetI * 0.5f;
      }
      else if (V - targetV > 0)
      {
        resultI = targetI * 0.2f;
      }
      else
      {
        resultI = 0.f;
      }
    }
    else if (disChargeMode == DisChargeMode::DischargeStop)
    {
      if (V - targetV > 0.01f)
      {
        resultI = targetI;
      }
      else if (V - targetV > 0.004f)
      {
        resultI = targetI * 0.5f;
      }
      else if (V - targetV > 0)
      {
        resultI = targetI * 0.2f;
      }
      else
      {
        resultI = 0.f;
      }
    }

    return resultI;
  };

  static int calcPWMValue(float ampere)
  {
    static const float RA{5.1f};
    static const float RB{5.1f};
    static const float RC{1.f};

    static const int MAX_PWM{0xFF};
    static const float MAX_PWM_F{static_cast<float>(MAX_PWM)};
    static const float REG{0.1f};
    static const float REG_RATE{(RA + RB + RC) / RC};
    static const float I_TO_V{REG * REG_RATE};
    static const float VOLT3_3{3.3f};
    static const float TO_V_RATE{I_TO_V / VOLT3_3};
    static const float AMP_TUNE{1.08}; // 実測との補正
    static const float INV_ACTIVE_RATE{AMP_TUNE / ACTIVE_RATE};

    
    const float voltRate{ampere * TO_V_RATE};

    return std::clamp(static_cast<int>(voltRate * MAX_PWM_F * INV_ACTIVE_RATE), 0, MAX_PWM);
  };

  std::vector<String> modeNames{String(" DiscNorm"), String(" DiscStop")};

public:
  BatteryInfo(uint8_t inReadPin, uint8_t inWritePin, uint8_t inBatteryIndex) : readPin{inReadPin}, writePin{inWritePin}, batteryIndex{inBatteryIndex} {};

  void read()
  {
    int MIN_VOLT{10};
    const int volt{analogRead(readPin)};
    valueCounter.readVolt(volt);
    if (volt < MIN_VOLT)
    {
      nextBatteryStatus = BatteryStatus::NoBat;
      V = 0;
    }
  };

  void setup()
  {
    pinMode(readPin, INPUT);
    pinMode(writePin, OUTPUT);
    reset();
    startTime = millis();
  };

  void loopSub()
  {
    currentBatteryStatus = nextBatteryStatus;

    if (!activeFlag)
    {
      currentTimeStatus = static_cast<TimeStatus>(NONE_MODE_LOOPS[(++loopCount) % sizeof(NONE_MODE_LOOPS)]);
      unsigned long temp{valueCounter.calcValue()};
      sleepV = voltageMapping.getVoltage(temp);
      V = sleepV;
      tunedI = 0;
      I = 0;
    }
    else
    {
      currentTimeStatus = static_cast<TimeStatus>(DISCHARGE_MODE_LOOPS[(++loopCount) % sizeof(DISCHARGE_MODE_LOOPS)]);

      unsigned long tempMillis{millis()};
      static const float RATE{1.f / (60.f * 60.f)};
      milliAmpereHour += I * (tempMillis - ampereHourTime) * RATE;
      ampereHourTime = tempMillis;

      if (currentTimeStatus == TimeStatus::None)
      {
      }
      else if (currentTimeStatus == TimeStatus::Active)
      {
        unsigned long temp{valueCounter.calcValue()};
        V = voltageMapping.getVoltage(temp);
        I = std::max(0.f, tunedI);
        if ((tunedI > 0.f) && (sleepV - V))
        {
          ohm = ((sleepV - V) * 1000.f * ACTIVE_RATE) / tunedI;
        }
      }
      else if (currentTimeStatus == TimeStatus::SleepStart)
      {
        unsigned long temp{valueCounter.calcValue()};
        V = voltageMapping.getVoltage(temp);
        I = 0;
      }
      else if (currentTimeStatus == TimeStatus::SleepStartRead)
      {
        unsigned long temp{valueCounter.calcValue()};
        I = 0;
      }
      else if (currentTimeStatus == TimeStatus::SleepEnd)
      {
        unsigned long temp{valueCounter.calcValue()};
        sleepV = voltageMapping.getVoltage(temp);
        if (currentBatteryStatus == BatteryStatus::Stop && disChargeMode == DisChargeMode::DischargeStop)
        {
          tunedI = 0;
        }
        else
        {
          tunedI = calcI(targetI, sleepV, targetV, disChargeMode);
        }

        I = std::max(0.f, tunedI);
      }
    }

    if (currentBatteryStatus == BatteryStatus::Active || currentBatteryStatus == BatteryStatus::Stop || currentBatteryStatus == BatteryStatus::None)
    {
      if (V > 0.1f)
      {
        if (tunedI > 0)
        {
          nextBatteryStatus = BatteryStatus::Active;
          if (nextBatteryStatus != currentBatteryStatus)
          {
            reset();
          }
        }
        else if (tunedI == 0)
        {
          nextBatteryStatus = BatteryStatus::Stop;
        }
      }
      /*
      else
      {
        nextBatteryStatus = BatteryStatus::NoBat;
      }
      */
    }
    else if (currentBatteryStatus == BatteryStatus::NoBat)
    {
      if (V > 0.1f)
      {
        nextBatteryStatus = BatteryStatus::Active;
        if (nextBatteryStatus != currentBatteryStatus)
        {
          reset();
        }
        startTime = millis();
      }
    }

    int intValue = calcPWMValue(I);
    analogWrite(writePin, intValue);
  };

  void reset()
  {
    tunedI = -1;
    I = 0;
    loopCount = 0;
  };

  void setDisplayData(DisplayMode displayMode, MainSettingMode settingMode)
  {

    ++displayCount;

    if (settingFlag)
    {
      drawAdafruit.drawString(">", 5 * batteryIndex, 0);
    }

    if (tunedI > 0 && (displayCount % 2))
    {

    }
    else
    {
      drawAdafruit.drawFloatR(sleepV, 5 * (batteryIndex + 1), 0, 4, 2);
    }

    if (settingFlag)
    {
      if (displayMode == DisplayMode::Display)
      {
        if (activeFlag)
        {
          displayDetail();
        }
        else
        {
          displayVoltOnly();
        }
      }
      else
      {
        if (settingMode == MainSettingMode::ModeChangeSetting)
        {
          displayModeChangeSetting();
        }
        else if (settingMode == MainSettingMode::DischargeVSetting)
        {
          displayDischargeV();
        }
        else if (settingMode == MainSettingMode::DischargeISetting)
        {
          displayDischargeI();
        }
        else
        {
          displayNone();
        }
      }
    }
  };

  void changeActive(int shift)
  {
    if (activeFlag)
    {
      activeFlag = false;
    }
    else
    {
      activeFlag = true;
    }
  }

  void displayNone()
  {
    int line{START_LINE};
    drawAdafruit.drawFillLine(line);
    ++line;
    drawAdafruit.drawFillLine(line);
    ++line;
    drawAdafruit.drawFillLine(line);
    ++line;
    drawAdafruit.drawFillLine(line);
  }

  void displayModeChangeSetting()
  {
    int vir_offset{0};
    int line{START_LINE};
    drawAdafruit.drawFillLine(line);
    vir_offset = SETTING_MENU_START_COL;
    drawAdafruit.drawString("Setting", vir_offset, line);

    ++line;
    String modeName;
    drawAdafruit.drawFillLine(line);
    vir_offset = SETTING_MENU_START_COL;
    drawAdafruit.drawChar(&CHAR_DATA_UP[0], vir_offset, line);

    ++line;
    drawAdafruit.drawFillLine(line);
    vir_offset = SETTING_MENU_START_COL;
    drawAdafruit.drawString(modeNames[(uint8_t)saveBattery->disChargeMode], SETTING_MENU_START_COL - 1, line);

    ++line;
    drawAdafruit.drawFillLine(line);
    vir_offset = SETTING_MENU_START_COL;
    drawAdafruit.drawChar(&CHAR_DATA_DOWN[0], vir_offset, line);

    ++line;
    drawAdafruit.drawFillLine(line);
    ++line;
    drawAdafruit.drawFillLine(line);
  }

  void displayDischargeI()
  {
    displayDischargeFloat(drawAdafruit, "Setting", "A", saveBattery->targetI);
  }

  void displayDischargeV()
  {
    displayDischargeFloat(drawAdafruit, "Setting", "V", saveBattery->targetV);
  }

  void displayVoltOnly()
  {
    static constexpr int DISPLAY_MENU_START_COL{3};
    static constexpr int DISPLAY_MENU_OFFSET_COL{5};
    int vir_offset{0};
    int line{START_LINE};

    drawAdafruit.drawFillLine(line);
    drawAdafruit.drawFillLine(line + 1);
    drawAdafruit.drawFillLine(line + 2);
    drawAdafruit.drawFillLine(line + 3);
    drawAdafruit.drawFillLine(line + 4);
    // drawAdafruit.drawFillLine(line + 5);

    ++line;
    // drawAdafruit.setTextSize(1);
    drawAdafruit.setFont();
    // drawAdafruit.drawFloat(sleepV, 5, 4, 3);
    // drawAdafruit.drawString("V", 15, 4);

    drawAdafruit.adaDisplay.setCursor(28, 32);
    drawAdafruit.adaDisplay.print(String(sleepV, 3) + String("V"));

    drawAdafruit.removeFont();
    // drawAdafruit.setTextSize(1);
    // drawAdafruit.drawBat();
  }

  void displayDetail()
  {
    static constexpr int DISPLAY_MENU_START_COL{3};
    static constexpr int DISPLAY_MENU_OFFSET_COL{5};
    int vir_offset{0};
    int line{START_LINE};
    drawAdafruit.drawFillLine(line);

    vir_offset = DISPLAY_MENU_START_COL;
    drawAdafruit.drawString(modeNames[(uint8_t)disChargeMode], vir_offset + 3, line);

    ++line;
    drawAdafruit.drawFillLine(line);

    vir_offset = DISPLAY_MENU_START_COL;
    vir_offset += DISPLAY_MENU_OFFSET_COL;
    drawAdafruit.drawFloatR(sleepV, vir_offset, line, DISPLAY_MENU_OFFSET_COL, 3);
    drawAdafruit.drawString("V", vir_offset, line);

    vir_offset += 2;
    if ((tunedI > 0.f) && (displayCount % 2))
    {
    }
    else
    {
      static constexpr char CHAR_DATA_ARROW[] = {0x1A, 0x00};
      drawAdafruit.drawChar(&CHAR_DATA_ARROW[0], vir_offset, line);
    }

    vir_offset += 2;
    vir_offset += DISPLAY_MENU_OFFSET_COL;
    drawAdafruit.drawFloatR(targetV, vir_offset, line, DISPLAY_MENU_OFFSET_COL, 3);
    drawAdafruit.drawString("V", vir_offset, line);

    ++line;
    drawAdafruit.drawFillLine(line);

    vir_offset = DISPLAY_MENU_START_COL;
    vir_offset += DISPLAY_MENU_OFFSET_COL;
    float displayI{std::max(0.f, tunedI)};
    drawAdafruit.drawFloatR(displayI, vir_offset, line, DISPLAY_MENU_OFFSET_COL, 3);
    drawAdafruit.drawString("A", vir_offset, line);

    vir_offset += 2;
    static constexpr char CHAR_DATA_ARROW[] = {0x1A, 0x00};
    drawAdafruit.drawChar(&CHAR_DATA_ARROW[0], vir_offset, line);

    vir_offset += 2;
    vir_offset += SETTING_MENU_OFFSET_COL;
    drawAdafruit.drawFloatR(targetI, vir_offset, line, SETTING_MENU_OFFSET_COL, 3);
    drawAdafruit.drawString("A", vir_offset, line);

    ++line;
    drawAdafruit.drawFillLine(line);

    vir_offset = DISPLAY_MENU_START_COL;
    vir_offset += DISPLAY_MENU_OFFSET_COL;
    const int sec{(millis() - startTime) / 1000};
    const int min{sec / 60};
    // drawAdafruit.drawIntR(sec, vir_offset, line);
    char str[128];
    sprintf(str, "%02d:%02d", min, sec % 60);
    drawAdafruit.drawChar(str, vir_offset, line);

    if (0)
    {
      ++line;
      drawAdafruit.drawFillLine(line);
      drawAdafruit.drawInt(calcPWMValue(targetI), SETTING_MENU_START_COL, line);
      drawAdafruit.drawString("PWM", SETTING_MENU_START_COL + SETTING_MENU_OFFSET_COL, line);
    }

    if (0)
    {
      ++line;
      drawAdafruit.drawFillLine(line);
      vir_offset = DISPLAY_MENU_START_COL;
      vir_offset += DISPLAY_MENU_OFFSET_COL;
      drawAdafruit.drawFloatR(sleepV, vir_offset, line, DISPLAY_MENU_OFFSET_COL, 3);
      drawAdafruit.drawString("V", vir_offset, line);
    }

    if (0)
    {
      ++line;
      drawAdafruit.drawFillLine(line);
      vir_offset = DISPLAY_MENU_START_COL;
      vir_offset += DISPLAY_MENU_OFFSET_COL;
      drawAdafruit.drawFloatR(I, vir_offset, line, DISPLAY_MENU_OFFSET_COL, 3);
      drawAdafruit.drawString("A", vir_offset, line);
    }

    ++line;
    drawAdafruit.drawFillLine(line);
    {
      vir_offset = DISPLAY_MENU_START_COL;
      vir_offset += DISPLAY_MENU_OFFSET_COL;
      drawAdafruit.drawFloatR(ohm, vir_offset, line, DISPLAY_MENU_OFFSET_COL, 1);

      static constexpr char CHAR_DATA_OHM[] = {0x6D, 0xe9, 0x00};
      drawAdafruit.drawChar(&CHAR_DATA_OHM[0], vir_offset, line);

      vir_offset += 2;
      vir_offset += DISPLAY_MENU_OFFSET_COL;
      drawAdafruit.drawFloatR(milliAmpereHour, vir_offset, line, DISPLAY_MENU_OFFSET_COL, 1);
      drawAdafruit.drawString("mah", vir_offset, line);
    }

    if (0)
    {
      ++line;
      drawAdafruit.drawFillLine(line);
      vir_offset = DISPLAY_MENU_START_COL;
      vir_offset += DISPLAY_MENU_OFFSET_COL;
      drawAdafruit.drawInt(loopCount % sizeof(DISCHARGE_MODE_LOOPS), vir_offset, line);
      static std::vector<String> modeNames{String("None"), String("Active"), String("Sleep"), String("Stop"), String("NoBat"), String("Max")};
      drawAdafruit.drawString(modeNames[(uint8_t)currentBatteryStatus], vir_offset + 5, line);
    }
  }

  ValueCounter valueCounter{};

  BatteryStatus currentBatteryStatus{BatteryStatus::None};
  BatteryStatus nextBatteryStatus{BatteryStatus::None};

  bool settingFlag{false};
  bool activeFlag{false};

  uint8_t batteryIndex;
  uint8_t readPin{0};
  uint8_t writePin{0};
  unsigned long startTime{0};

  float V{0.f};
  float sleepV{0.f};
  float targetV{1.40f};
  float I{0.0f};
  float tunedI{1.f};
  float ohm{0.f};
  float targetI{0.2f};
  float milliAmpereHour{0.0f};
  unsigned long ampereHourTime{0};

  SaveBattery *saveBattery{nullptr};

  DisChargeMode disChargeMode{DisChargeMode::DischargeNormal};
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
  static constexpr int SAVEDATA_ID{0xABCD};

  static constexpr uint8_t XIAO_READ_BAT{PD4};
  static constexpr uint8_t XIAO_READ_BAT_SWITCH{PD3};

  static constexpr uint8_t READ1_PIN{0};
  static constexpr uint8_t READ2_PIN{1};
  static constexpr uint8_t READ3_PIN{6};
  static constexpr uint8_t READ4_PIN{7};

  static constexpr uint8_t WRITE1_PIN{2};
  static constexpr uint8_t WRITE2_PIN{3};
  static constexpr uint8_t WRITE3_PIN{8};
  static constexpr uint8_t WRITE4_PIN{9};

  static constexpr int PUSH_BUTTON_L{15};
  static constexpr int PUSH_BUTTON_D{14};
  static constexpr int PUSH_BUTTON_U{13};
  static constexpr int PUSH_BUTTON_R{12}; // 12(SAND11_RX) or 10
  static constexpr int PUSH_BUTTON_C{11};

  static constexpr int WAKE_UP_PIN{PUSH_BUTTON_D};

  ButtonStatus buttonLStatus{};
  ButtonStatus buttonRStatus{};
  ButtonStatus buttonUStatus{};
  ButtonStatus buttonDStatus{};
  ButtonStatus buttonCStatus{};

  std::vector<BatteryInfo> batteryStatuses{
      BatteryInfo{READ1_PIN, WRITE1_PIN, 0},
      BatteryInfo{READ2_PIN, WRITE2_PIN, 1},
      BatteryInfo{READ3_PIN, WRITE3_PIN, 2},
      BatteryInfo{READ4_PIN, WRITE4_PIN, 3}};

  DisplayMode displayMode{DisplayMode::Display};

  MainSettingMode mainSettingMode{MainSettingMode::DischargeVSetting};

  ConfigSettingMode configSettingMode{ConfigSettingMode::Volt00Setting};

  unsigned long loopSubMillis{0};

  size_t currentBatteryIndex{0};

  unsigned long loopSubCount{0};

  struct SaveMainData
  {
    static constexpr int SAVEDATA_ADDRESS{0X400};
    int id{SAVEDATA_ID};
    int ver{1};
    SaveBattery battery[4];
  };

  struct SaveConfigData
  {
    static constexpr int SAVEDATA_ADDRESS{0X100};
    int id{SAVEDATA_ID};
    int ver{1};
    VoltageMapping::VoltPair voltPair[5] = {{0.f, 0}, {0.5f, 0}, {1.f, 0}, {1.5f, 0}, {2.f, 0}};
  }; 

public:
  SaveMainData saveMainData{};

  SaveConfigData saveConfigData{};

  MainMode mainMode{MainMode::DischargerMode};

private:

  void readAndDrawXiaoBattery()
  {
    constexpr static float R_RATE{2.f};
    const int readValue{analogRead(XIAO_READ_BAT)};
    const float xiaoVolt{(static_cast<float>(readValue) / 4096.f) * R_RATE * 3.3f};
    // const float xiaoVolt{ (2048.f / 4096.f) * R_RATE * 3.3f };
    drawAdafruit.drawBat(xiaoVolt);
  }

  template <typename T>
  void saveCustomData(byte *p)
  {
    for (int i = 0; i < sizeof(SaveMainData); i++)
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
    saveCustomData<SaveConfigData>((byte *)&saveConfigData);
  }

  void saveMain()
  {
    saveCustomData<SaveMainData>((byte *)&saveMainData);
  }

  void loadConfig()
  {
    loadCustomData<SaveConfigData>((byte *)&saveConfigData);
  }

  void loadMain()
  {
    loadCustomData<SaveMainData>((byte *)&saveMainData);
  };

public:
  BatteryController()
  {
    batteryStatuses[currentBatteryIndex].settingFlag = true;
  };

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
    /*
    if (digitalRead(PUSH_BUTTON3) == LOW) {
      while (true) {
        digitalWrite(LED_BUILTIN, LOW);
        delay(50);
        digitalWrite(LED_BUILTIN, HIGH);
        delay(50);
      }
    }
    */

    // BatteryReadSetting
    pinMode(XIAO_READ_BAT_SWITCH, OUTPUT);
    digitalWrite(XIAO_READ_BAT_SWITCH, HIGH);
    pinMode(XIAO_READ_BAT, INPUT);

    // Button
    pinMode(PUSH_BUTTON_L, INPUT);
    pinMode(PUSH_BUTTON_D, INPUT);
    pinMode(PUSH_BUTTON_U, INPUT);
    pinMode(PUSH_BUTTON_R, INPUT);
    pinMode(PUSH_BUTTON_C, INPUT);

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
    digitalWrite(PA6, HIGH);  //FLASH

    buttonLStatus.init(PUSH_BUTTON_L);
    buttonRStatus.init(PUSH_BUTTON_R);
    buttonUStatus.init(PUSH_BUTTON_U);
    buttonDStatus.init(PUSH_BUTTON_D);
    buttonCStatus.init(PUSH_BUTTON_C);

    int val{HIGH};
    val = digitalRead(PUSH_BUTTON_D);
    if (val != LOW) // val == LOW)
    {
      loadMain();
      loadConfig();
    }

    val = digitalRead(PUSH_BUTTON_R);
    if (val == LOW)
    {
      mainMode = MainMode::ConfigMode;
    }

    copySaveMainDataToStatusData();

    batteryStatuses[0].saveBattery = &(saveMainData.battery[0]);
    batteryStatuses[1].saveBattery = &(saveMainData.battery[1]);
    batteryStatuses[2].saveBattery = &(saveMainData.battery[2]);
    batteryStatuses[3].saveBattery = &(saveMainData.battery[3]);
  };

  void copySaveMainDataToStatusData()
  {
    for (int i{0}; i < batteryStatuses.size(); ++i)
    {
      auto& batteryStatus{batteryStatuses[i]};

      const static SaveBattery defaultSaveBattery{};
      auto& saveBattery{saveMainData.battery[i]};
      if (saveMainData.id != SAVEDATA_ID)
      {
        saveBattery = defaultSaveBattery;
      }

      batteryStatus.targetI = saveBattery.targetI;
      batteryStatus.targetV = saveBattery.targetV;
      batteryStatus.disChargeMode = saveBattery.disChargeMode;
      batteryStatus.activeFlag = saveBattery.activeFlag;

      batteryStatus.setup();
    }
  }

  void loopMain()
  {
    for (auto &batteryStatus : batteryStatuses)
    {
      batteryStatus.read();
    }
  };

  void setDisplayDataConfigMode()
  {
    if (configSettingMode == ConfigSettingMode::Volt00Setting)
    {
      displayDischargeInt(drawAdafruit, "Offset00V", "", saveConfigData.voltPair[0].input);
    }
    else if (configSettingMode == ConfigSettingMode::Volt05Setting)
    {
      displayDischargeInt(drawAdafruit, "Offset05V", "", saveConfigData.voltPair[1].input);
    }
    else if (configSettingMode == ConfigSettingMode::Volt10Setting)
    {
      displayDischargeInt(drawAdafruit, "Offset10V", "", saveConfigData.voltPair[2].input);
    }
    else if (configSettingMode == ConfigSettingMode::Volt15Setting)
    {
      displayDischargeInt(drawAdafruit, "Offset15V", "", saveConfigData.voltPair[3].input);
    }
  }

  void setDisplayData()
  {
    if (mainMode == MainMode::DischargerMode)
    {
      drawAdafruit.drawFillLine(0);
      for (auto &batteryStatus : batteryStatuses)
      {
        batteryStatus.setDisplayData(displayMode, mainSettingMode);
      }
    }
    else if (mainMode == MainMode::ConfigMode)
    {
      setDisplayDataConfigMode();
    }
  };

  void goDeepSleep()
  {
    drawAdafruit.clearDisplay();
    drawAdafruit.display();
    LowPower.attachInterruptWakeup(WAKE_UP_PIN, nullptr, RISING);
    LowPower.deepSleep(7 * 24 * 3600 * 1000); // one week
  }

  void updateButtonStatus()
  {
    int checkFlag{0};

    if (mainMode == MainMode::DischargerMode)
    {
      checkFlag = buttonLStatus.check();
      if (checkFlag == 1)
      {
        changeTargetBattery(-1);
      }

      checkFlag = buttonRStatus.check();
      if (checkFlag == 1)
      {
        changeTargetBattery(1);
      }
    }

    checkFlag = buttonUStatus.check();
    if (checkFlag == 1)
    {
      shiftParam(1);
    }

    checkFlag = buttonDStatus.check();
    if (checkFlag == 1)
    {
      shiftParam(-1);
    }
    else if (checkFlag == 2)
    {
      goDeepSleep();
    }

    checkFlag = buttonCStatus.check();
    if (checkFlag == 1)
    {
      if (mainMode == MainMode::DischargerMode)
      {
        if (displayMode == DisplayMode::Display)
        {
          changeActive(1);
        }
        else if (displayMode == DisplayMode::None)
        {
          changeSettingMode(1);
        }
      }
      else if (mainMode == MainMode::ConfigMode)
      {
        changeSettingMode(1);
      }
    }
    else if (checkFlag == 2)
    {
      changeDisplayMode(1);
    }
  }

  void changeTargetBattery(int shift)
  {
    currentBatteryIndex = (currentBatteryIndex + shift) % batteryStatuses.size();

    for (size_t index{0}; index < batteryStatuses.size(); ++index)
    {
      if (index == currentBatteryIndex)
      {
        batteryStatuses[index].settingFlag = true;
      }
      else
      {
        batteryStatuses[index].settingFlag = false;
      }
    }
  };

  void changeDisplayMode(int shift)
  {
    const int nextModeIndex{(static_cast<int>(displayMode) + shift) % static_cast<int>(DisplayMode::Max)};

    displayMode = static_cast<DisplayMode>(nextModeIndex);
    if (displayMode == DisplayMode::Display)
    {
      saveMainData.id = SAVEDATA_ID;
      saveMainData.ver = 1;
      copySaveMainDataToStatusData();
      saveMain();
    }
  };

  void changeSettingMode(int shift)
  {

    if (mainMode == MainMode::DischargerMode)
    {
      const int nextModeIndex{(static_cast<int>(mainSettingMode) + shift) % static_cast<int>(MainSettingMode::Max)};
      mainSettingMode = static_cast<MainSettingMode>(nextModeIndex);
    }
    else if (mainMode == MainMode::ConfigMode)
    {
      const int nextModeIndex{(static_cast<int>(configSettingMode) + shift) % static_cast<int>(ConfigSettingMode::Max)};
      configSettingMode = static_cast<ConfigSettingMode>(nextModeIndex);
    }

  };

  void changeActive(int shift)
  {
    batteryStatuses[currentBatteryIndex].changeActive(shift);
  };

  void shiftParam(int shift)
  {
    if (mainMode == MainMode::DischargerMode)
    {
      saveMainData.battery[currentBatteryIndex].shiftParam(mainSettingMode, shift);
    }
    else if (mainMode == MainMode::ConfigMode)
    {
      if (configSettingMode == ConfigSettingMode::Volt00Setting)
      {
        saveConfigData.voltPair[0].input += shift;
      }
      else if (configSettingMode == ConfigSettingMode::Volt05Setting)
      {
        saveConfigData.voltPair[1].input += shift;
      }
      else if (configSettingMode == ConfigSettingMode::Volt10Setting)
      {
        saveConfigData.voltPair[2].input += shift;
      }
      else if (configSettingMode == ConfigSettingMode::Volt15Setting)
      {
        saveConfigData.voltPair[3].input += shift;
      }
    }
  };

  void loopSub()
  {
    ++loopSubCount;
    if ((loopSubCount % 60) == 0)
    {
      readAndDrawXiaoBattery();
    }

    digitalWrite(LED_BUILTIN, ((loopSubCount / 12) % 2));

#ifdef SERIAL_DEBUG_ON
    if ((loopSubCount % 12) == 0)
    {
      Serial.printf("serial print test. %d\n", loopSubCount);
    }
#endif

    if (mainMode == MainMode::DischargerMode)
    {
      for (auto &batteryStatus : batteryStatuses)
      {
        batteryStatus.loopSub();
      }

      updateButtonStatus();

      if ((loopSubCount % 3) == 0)
      {
        setDisplayData();
        drawAdafruit.display();      
      }
    }
    else if (mainMode == MainMode::ConfigMode)
    {
      
      updateButtonStatus();

      if ((loopSubCount % 3) == 0)
      {
        setDisplayData();
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