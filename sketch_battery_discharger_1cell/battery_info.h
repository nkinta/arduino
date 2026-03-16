#pragma once

#include "discharger_define.h"
#include "display/draw_adafruit.h"
#include "config.h"
#include "voltage_mapping.h"

extern VoltageMapping voltageMapping;
extern DrawAdafruit drawAdafruit;

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

static void setDisplayTuneMenu(DrawAdafruit &adafruit, String &&title, std::vector<String> &menuList, std::vector<String> &valueList, int targetIndex)
{
  int vir_offset1{0};
  int vir_offset2{0};
  int line{0};
  adafruit.drawFillLine(line);
  adafruit.drawStringC(title, line);
  ++line;

  vir_offset1 = 2;
  vir_offset2 = 12;

  constexpr int MAX_DISPLAY_LINE{4};
  int startIndex = std::max(0, targetIndex - MAX_DISPLAY_LINE);
  // int targetLine = targetIndex - startIndex;

  int index{0};
  for (int i{startIndex}; i < menuList.size(); ++i)
  {
    if (index > MAX_DISPLAY_LINE)
    {
      break;
    }

    adafruit.drawFillLine(line);
    if (i == targetIndex)
    {
      drawAdafruit.drawChar(&DisplayConst::CHAR_DATA_ARROW[0], 0, line);
    }
    adafruit.drawString(menuList[i], vir_offset1, line);
    adafruit.drawString(valueList[i], vir_offset2, line);
    ++index;
    ++line;
  }

  /*
  for (i = line; i < MAX_DISPLAY_LINE; ++i)
  {
    adafruit.drawFillLine(i);
  }
  */

  while (line < 7)
  {
    adafruit.drawFillLine(line);
    line++;
  }
}

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

  static constexpr float TARGET_V_MAX{1.55f};
  static constexpr float TARGET_V_MIN{1.0f};
  static constexpr float TARGET_I_MAX{2.0f}; // 2SK4017 MOSFETの特性上 0.1Aの時に2.24V 0.5Aの時に2.6V 1.0Aの時に2.8V 2Aの時にMOSのゲート電圧が3.2V
  static constexpr float TARGET_I_MIN{0.4f};

  float targetV{1.4f};
  float targetI{1.f};
  DisChargeMode disChargeMode{DisChargeMode::DischargeNormal};
  bool padding{true};

  void shiftParam(BatteryConfigSettingMode settingMode, int shift)
  {
    if (settingMode == BatteryConfigSettingMode::ModeChangeSetting)
    {
      const int nextModeIndex{(static_cast<int>(DisChargeMode::Max) + static_cast<int>(disChargeMode) + shift) % static_cast<int>(DisChargeMode::Max)};
      disChargeMode = static_cast<DisChargeMode>(nextModeIndex);
    }
    else if (settingMode == BatteryConfigSettingMode::DischargeVSetting)
    {
      targetV = std::clamp(targetV + 0.001f * static_cast<float>(shift), TARGET_V_MIN, TARGET_V_MAX);
    }
    else if (settingMode == BatteryConfigSettingMode::DischargeISetting)
    {
      targetI = std::clamp(targetI + 0.1f * static_cast<float>(shift), TARGET_I_MIN, TARGET_I_MAX);
    }
  }

  void setDisplayBatteryConfig(int index, BatteryConfigSettingMode settingMode)
  {
    std::vector<String> menuList{"TargetV", "TargetI", "DiscMode"};

    // String valueStr{String(value, decimal)};
    String mode{DisplayConst::DISC_MODE_NAMES[(uint8_t)disChargeMode]};
    std::vector<String> valueList{String(targetV, 3), String(targetI), mode};

    String Title{"Battery No."};
    Title += String(index + 1);
    setDisplayTuneMenu(drawAdafruit, std::move(Title), menuList, valueList, static_cast<int>(settingMode));
    // displayDischargeFloat(drawAdafruit, "TargetA", "A", saveBattery->targetI);
  }
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

struct BatteryInfo
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

  static int calcPWMValue(float ampere, float active_rate, float customAmpTune)
  {
    static const float RA{5.1f};
    static const float RB{5.1f};
    static const float RC{1.f};

    static const int MAX_PWM{0xFF};
    static const float MAX_PWM_F{static_cast<float>(MAX_PWM)};
    static const float REG{0.1f};
    static const float REG_RATE{(RA + RB + RC) / RC};
    static const float I_TO_V{REG * REG_RATE};
    static const float TO_V_RATE{I_TO_V / VOLT3_3};
    static const float AMP_TUNE{1.08f}; // 実測との補正
    // static const float INV_ACTIVE_RATE{ AMP_TUNE / ACTIVE_RATE };

    const float voltRate{ampere * TO_V_RATE};

    return std::clamp(static_cast<int>(voltRate * MAX_PWM_F * ((AMP_TUNE * customAmpTune) / active_rate)), 0, MAX_PWM);
  };

public:
  BatteryInfo(uint8_t inReadPin, uint8_t inWritePin, uint8_t inBatteryIndex)
      : readPin{inReadPin}, writePin{inWritePin}, batteryIndex{inBatteryIndex} {};

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

  void loopSubPushDischarge()
  {
    unsigned long temp{valueCounter.calcValue()};
    if (tunedI > 0.01f)
    {
      V = voltageMapping.getVoltage(temp);

      if ((tunedI > 0.f) && (sleepV - V) && ((millis() - startTime) < 1000))
      {
        ohm = ((sleepV - V) * 1000.f * ACTIVE_RATE) / tunedI;
      }
    }
    else
    {
      sleepV = voltageMapping.getVoltage(temp);
      V = sleepV;
    }

    I = std::max(0.f, tunedI);
    int intValue = calcPWMValue(I, 1.f, saveConfigData->customAmpTune);
    analogWrite(writePin, intValue);
  }

  void loopSubNormalDischarge()
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

    int intValue = calcPWMValue(I, ACTIVE_RATE, saveConfigData->customAmpTune);
    analogWrite(writePin, intValue);
  };

  void reset()
  {
    tunedI = -1;
    I = 0;
    loopCount = 0;
  };

  void pushOn(float inI)
  {
    if (tunedI == 0.f)
    {
      startTime = millis();
    }
    tunedI = inI;
  }

  void pushOff()
  {
    tunedI = 0.f;
  }

  void setDisplayPushData()
  {

    ++displayCount;

    if (tunedI > 0 && (displayCount % 2))
    {
    }
    else
    {
      int batterySetIndex{batteryIndex / 2};
      int vir_offset{10 * batterySetIndex + (batteryIndex % 2) * 0 + 8};
      int line{(batteryIndex % 2) + 2};
      drawAdafruit.drawFloatR(V, vir_offset, line, 4, saveConfigData->decimal);
      drawAdafruit.drawString("V", vir_offset, line);
    }

    if (displayFlag)
    {
      // displayVoltOnly();
    }
  };

  void setDisplayData()
  {

    ++displayCount;

    if (displayFlag)
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

    if (displayFlag)
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

  void setDisplayBatteryConfig(BatteryConfigSettingMode settingMode)
  {
    std::vector<String> menuList{"TargetI", "TargetA", "DiscMode"};

    // String valueStr{String(value, decimal)};
    String mode{DisplayConst::DISC_MODE_NAMES[(uint8_t)saveBattery->disChargeMode]};
    std::vector<String> valueList{String(saveBattery->targetV, 3), String(saveBattery->targetI), mode};

    String Title{"Battery No."};
    Title += String(batteryIndex + 1);
    setDisplayTuneMenu(drawAdafruit, std::move(Title), menuList, valueList, static_cast<int>(settingMode));
    // displayDischargeFloat(drawAdafruit, "TargetA", "A", saveBattery->targetI);
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

    ++line;
    drawAdafruit.setFont();

    drawAdafruit.adaDisplay.setCursor(28, 32);
    drawAdafruit.adaDisplay.print(String(sleepV, 3) + String("V"));

    drawAdafruit.removeFont();
  }

  void displayDetail()
  {
    static constexpr int DISPLAY_MENU_START_COL{3};
    static constexpr int DISPLAY_MENU_OFFSET_COL{5};
    int vir_offset{0};
    int line{START_LINE};
    drawAdafruit.drawFillLine(line);

    vir_offset = DISPLAY_MENU_START_COL;
    drawAdafruit.drawStringC(DisplayConst::DISC_MODE_NAMES[(uint8_t)disChargeMode], line);

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
      // static constexpr char CHAR_DATA_ARROW[] = {0x1A, 0x00};
      drawAdafruit.drawChar(&DisplayConst::CHAR_DATA_ARROW_NEW[0], vir_offset, line);
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

    drawAdafruit.drawChar(&DisplayConst::CHAR_DATA_ARROW_NEW[0], vir_offset, line);

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
      drawAdafruit.drawInt(calcPWMValue(targetI, ACTIVE_RATE, saveConfigData->customAmpTune), SETTING_MENU_START_COL, line);
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

  bool displayFlag{false};
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
  const SaveConfigData *saveConfigData{nullptr};

  DisChargeMode disChargeMode{DisChargeMode::DischargeNormal};
};
