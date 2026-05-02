#include <string>
#include "battery_info.hpp"
#include "display/draw_adafruit.hpp"
#include "voltage_mapping.hpp"
#include "save_config_data.hpp"
#include "battery_controller.hpp"

#include "display/fonts/BBHBogle-Regular_9.h"
#include "display/fonts/BBHBogle-Regular_12.h"

#include "display/fonts/Quicksand-VariableFont_wght_12.h"
#include "display/fonts/Quicksand-VariableFont_wght_9.h"

#include "display/fonts/Roboto-VariableFont_wdth_wght_12.h"
#include "display/fonts/Roboto-VariableFont_wdth_wght_9.h"
#include "display/fonts/Roboto-VariableFont_wdth_wght_6.h"

extern DrawAdafruit drawAdafruit;

const std::vector<String> DISC_MODE_NAMES{String("Keep"), String("KeepMin"), String("Stop")};

const std::vector<String> REDUCE_MODE_NAMES{String("Mild"), String("Normal"), String("Hard"), String("None")};

void printMinuteSecond(int sec, char *str)
{
    int min{sec / 60.f};
    sprintf(str, "%02d:%02d", min, sec % 60);
}

void setDisplayTuneMenu(DrawAdafruit &adafruit, String &&title, std::vector<String> &menuList, std::vector<String> &valueList, int targetIndex)
{
    int virOffset1{0};
    int virOffset2{0};
    int line{0};
    adafruit.drawFillLine(line);
    adafruit.drawStringC(title, line);
    ++line;

    virOffset1 = 2;
    virOffset2 = 12;

    constexpr int MAX_DISPLAY_LINE{4};
    int startIndex = std::max(0, targetIndex - MAX_DISPLAY_LINE);

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
        adafruit.drawString(menuList[i], virOffset1, line);
        adafruit.drawString(valueList[i], virOffset2, line);
        ++index;
        ++line;
    }

    while (line < 7)
    {
        adafruit.drawFillLine(line);
        line++;
    }
}

void SaveBattery::shiftParam(BatteryConfigSettingMode settingMode, int shift)
{
    if (settingMode == BatteryConfigSettingMode::ModeChangeSetting)
    {
        const int nextModeIndex{(static_cast<int>(DisChargeMode::Max) + static_cast<int>(_disChargeMode) + shift) % static_cast<int>(DisChargeMode::Max)};
        _disChargeMode = static_cast<DisChargeMode>(nextModeIndex);
    }
    else if (settingMode == BatteryConfigSettingMode::DischargeVSetting)
    {
        _targetV = std::clamp(_targetV + 0.001f * static_cast<float>(shift), TARGET_V_MIN, TARGET_V_MAX);
    }
    else if (settingMode == BatteryConfigSettingMode::DischargeISetting)
    {
        _targetI = std::clamp(_targetI + 0.1f * static_cast<float>(shift), TARGET_I_MIN, TARGET_I_MAX);
    }
    else if (settingMode == BatteryConfigSettingMode::HoldMinSetting)
    {
        _holdMin = std::clamp(_holdMin + shift, HOLDMIN_MIN, HOLDMIN_MAX);
    }
    else if (settingMode == BatteryConfigSettingMode::ApatureChangeSetting)
    {
        const int nextModeIndex{(static_cast<int>(ReduceMode::Max) + static_cast<int>(_reduceMode) + shift) % static_cast<int>(ReduceMode::Max)};
        _reduceMode = static_cast<ReduceMode>(nextModeIndex);
    }
}

void SaveBattery::setDisplayBatteryConfig(int index, BatteryConfigSettingMode settingMode) const
{
    std::vector<String> menuList{"TargetV", "TargetI", "DiscMode", "ReduceI", "KeepMin"};

    const String &disChargeModeString{DISC_MODE_NAMES[static_cast<uint8_t>(_disChargeMode)]};
    const String &reduceModeString{REDUCE_MODE_NAMES[static_cast<uint8_t>(_reduceMode)]};

    std::vector<String> valueList{String(_targetV, 3), String(_targetI), disChargeModeString, reduceModeString, String(_holdMin)};

    String title{"Battery Pair."};
    title += String(index + 1);
    setDisplayTuneMenu(drawAdafruit, std::move(title), menuList, valueList, static_cast<int>(settingMode));
}

float BatteryInfo::calcI(const float targetI, const float v, const float targetV, const ReduceMode reduceMode)
{
    float constexpr MIN_CURRENT{0.2};
    float constexpr MIDDLE_MIN_CURRENT{0.4};
    float resultI{0};
    if (reduceMode == ReduceMode::Normal)
    {
        if (v - targetV > 0.01f)
        {
            resultI = std::max(targetI, MIDDLE_MIN_CURRENT);
        }
        else if (v - targetV > 0.004f)
        {
            resultI = std::max(targetI * 0.5f, MIDDLE_MIN_CURRENT);
        }
        else if (v - targetV > 0)
        {
            resultI = std::max(targetI * 0.2f, MIN_CURRENT);
        }
        else
        {
            resultI = 0.f;
        }
    }
    else if (reduceMode == ReduceMode::Soft)
    {
        if (v - targetV > 0.02f)
        {
            resultI = std::max(targetI, MIDDLE_MIN_CURRENT);
        }
        else if (v - targetV > 0.008f)
        {
            resultI = std::max(targetI * 0.5f, MIDDLE_MIN_CURRENT);
        }
        else if (v - targetV > 0)
        {
            resultI = std::max(targetI * 0.2f, MIN_CURRENT);
        }
        else
        {
            resultI = 0.f;
        }
    }
    else if (reduceMode == ReduceMode::Hard)
    {
        if (v - targetV > 0.004f)
        {
            resultI = std::max(targetI, MIDDLE_MIN_CURRENT);
        }
        else if (v - targetV > 0)
        {
            resultI = std::max(targetI * 0.3f, MIN_CURRENT);
        }
        else
        {
            resultI = 0.f;
        }
    }
    else if (reduceMode == ReduceMode::None)
    {
        resultI = targetI;
    }

    return resultI;
};

int BatteryInfo::calcPWMValue(float ampere, float activeRate, float calibI)
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
    static const float AMP_TUNE{1.08f};

    const float voltRate{ampere * TO_V_RATE};

    return std::clamp(static_cast<int>(voltRate * MAX_PWM_F * ((AMP_TUNE * calibI) / activeRate)), 0, MAX_PWM);
};

void BatteryInfo::loopSubPushDischarge()
{
    unsigned long temp{_valueCounter.calcValue()};
    if (_tunedI > 0.01f)
    {
        _v = _batteryController->_voltageMapping.getVoltage(temp);

        if ((_tunedI > 0.f) && (_sleepV - _v) && ((millis() - _startMillis) < 1000))
        {
            _ohm = ((_sleepV - _v) * 1000.f * ACTIVE_RATE) / _tunedI;
        }
    }
    else
    {
        _sleepV = _batteryController->_voltageMapping.getVoltage(temp);
        _v = _sleepV;
    }

    _i = std::max(0.f, _tunedI);
    int intValue = calcPWMValue(_i, 1.f, _batteryController->_calibI);
    analogWrite(_writePin, intValue);
}

void BatteryInfo::writePinReset() const
{
    analogWrite(_writePin, 0);
}

void BatteryInfo::loopSubNormalDischarge()
{
    _currentBatteryStatus = _nextBatteryStatus;

    if (!_activeFlag)
    {
        _currentTimeStatus = static_cast<TimeStatus>(NONE_MODE_LOOPS[(++_loopCount) % sizeof(NONE_MODE_LOOPS)]);
        unsigned long temp{_valueCounter.calcValue()};
        _sleepV = _batteryController->_voltageMapping.getVoltage(temp);
        _v = _sleepV;
        _tunedI = 0;
        _i = 0;
    }
    else
    {
        _currentTimeStatus = static_cast<TimeStatus>(DISCHARGE_MODE_LOOPS[(++_loopCount) % sizeof(DISCHARGE_MODE_LOOPS)]);

        unsigned long tempMillis{millis()};
        static const float RATE{1.f / (60.f * 60.f)};
        _milliAmpereHour += _i * (tempMillis - _ampereHourTime) * RATE;
        _ampereHourTime = tempMillis;

        if (_currentTimeStatus == TimeStatus::None)
        {
        }
        else if (_currentTimeStatus == TimeStatus::Active)
        {
            unsigned long temp{_valueCounter.calcValue()};
            _v = _batteryController->_voltageMapping.getVoltage(temp);
            _i = std::max(0.f, _tunedI);
            if ((_tunedI > 0.f) && (_sleepV - _v))
            {
                _ohm = ((_sleepV - _v) * 1000.f * ACTIVE_RATE) / _tunedI;
            }
        }
        else if (_currentTimeStatus == TimeStatus::SleepStart)
        {
            unsigned long temp{_valueCounter.calcValue()};
            _v = _batteryController->_voltageMapping.getVoltage(temp);
            _i = 0;
        }
        else if (_currentTimeStatus == TimeStatus::SleepStartRead)
        {
            unsigned long temp{_valueCounter.calcValue()};
            _i = 0;
        }
        else if (_currentTimeStatus == TimeStatus::SleepEnd)
        {
            bool stopContinueFlag{false};
            if (_currentBatteryStatus == BatteryStatus::Stop)
            {
                if (_disChargeMode == DisChargeMode::DischargeStop)
                {
                    stopContinueFlag = true;
                }
                else if (_disChargeMode == DisChargeMode::DischargeHoldP)
                {
                    if (_endSeconds > (_holdMin * 60))
                    {
                        stopContinueFlag = true;
                    }
                }
            }

            unsigned long temp{_valueCounter.calcValue()};
            _sleepV = _batteryController->_voltageMapping.getVoltage(temp);
            if (stopContinueFlag)
            {
                _tunedI = 0;
            }
            else
            {
                _tunedI = calcI(_targetI, _sleepV, _targetV, _reduceMode);
            }

            _i = std::max(0.f, _tunedI);
        }
    }

    if (_currentBatteryStatus == BatteryStatus::Active || _currentBatteryStatus == BatteryStatus::Stop)
    {
        if (_v > 0.1f)
        {
            if (_tunedI > 0)
            {
                _nextBatteryStatus = BatteryStatus::Active;
                if (_nextBatteryStatus != _currentBatteryStatus)
                {
                    miniReset();
                }
                if (_dischargedCount == 0)
                {
                    _startSeconds = (millis() - _startMillis) / 1000;
                }
                else
                {
                    _endSeconds = (millis() - _endMillis) / 1000;
                }
            }
            else if (_tunedI == 0)
            {
                _nextBatteryStatus = BatteryStatus::Stop;
                if (_nextBatteryStatus != _currentBatteryStatus)
                {
                    if (_dischargedCount == 0)
                    {
                        _endMillis = millis();
                    }
                    ++_dischargedCount;
                }
                if (_dischargedCount > 0)
                {
                    _endSeconds = (millis() - _endMillis) / 1000;
                }
            }
        }
    }
    else if (_currentBatteryStatus == BatteryStatus::NoBat || _currentBatteryStatus == BatteryStatus::None)
    {
        if (_v > 0.1f)
        {
            _nextBatteryStatus = BatteryStatus::Active;
            reset();
        }
    }

    int intValue = calcPWMValue(_i, ACTIVE_RATE, _batteryController->_calibI);
    analogWrite(_writePin, intValue);
};

void BatteryInfo::setDisplayVoltOnly() const
{
    static constexpr int DISPLAY_MENU_START_COL{3};
    static constexpr int DISPLAY_MENU_OFFSET_COL{5};
    int virOffset{0};
    int line{START_LINE};

    drawAdafruit.drawFillLine(line);
    drawAdafruit.drawFillLine(line + 1);
    drawAdafruit.drawFillLine(line + 2);
    drawAdafruit.drawFillLine(line + 3);
    drawAdafruit.drawFillLine(line + 4);

    ++line;
    drawAdafruit.setFont(&BBHBogle_Regular12pt7b);

    drawAdafruit.setCursor(32, 36);
    drawAdafruit.printString(String(_sleepV, 3) + String("V"));

    drawAdafruit.removeFont();
}

void BatteryInfo::setDisplayDetail() const
{
    static constexpr int DISPLAY_MENU_START_COL{3};
    static constexpr int DISPLAY_MENU_OFFSET_COL{5};
    int virOffset{0};
    int line{START_LINE};
    drawAdafruit.drawFillLine(line);

    virOffset = DISPLAY_MENU_START_COL;
    drawAdafruit.drawStringC(DISC_MODE_NAMES[static_cast<uint8_t>(_disChargeMode)], line);

    ++line;
    drawAdafruit.drawFillLine(line);

    virOffset = DISPLAY_MENU_START_COL;
    virOffset += DISPLAY_MENU_OFFSET_COL;
    drawAdafruit.drawFloatR(_sleepV, virOffset, line, DISPLAY_MENU_OFFSET_COL, 3);
    drawAdafruit.drawString("V", virOffset, line);

    virOffset += 2;
    if ((_tunedI > 0.f) && (_displayCount % 2))
    {
    }
    else
    {
        drawAdafruit.drawChar(&DisplayConst::CHAR_DATA_ARROW_NEW[0], virOffset, line);
    }

    virOffset += 2;
    virOffset += DISPLAY_MENU_OFFSET_COL;
    drawAdafruit.drawFloatR(_targetV, virOffset, line, DISPLAY_MENU_OFFSET_COL, 3);
    drawAdafruit.drawString("V", virOffset, line);

    ++line;
    drawAdafruit.drawFillLine(line);

    virOffset = DISPLAY_MENU_START_COL;
    virOffset += DISPLAY_MENU_OFFSET_COL;
    float displayI{std::max(0.f, _tunedI)};
    drawAdafruit.drawFloatR(displayI, virOffset, line, DISPLAY_MENU_OFFSET_COL, 3);
    drawAdafruit.drawString("A", virOffset, line);

    virOffset += 2;

    drawAdafruit.drawChar(&DisplayConst::CHAR_DATA_ARROW_NEW[0], virOffset, line);

    virOffset += 2;
    virOffset += SETTING_MENU_OFFSET_COL;
    drawAdafruit.drawFloatR(_targetI, virOffset, line, SETTING_MENU_OFFSET_COL, 3);
    drawAdafruit.drawString("A", virOffset, line);

    ++line;
    drawAdafruit.drawFillLine(line);

    virOffset = DISPLAY_MENU_START_COL;

    float displayStartSeconds{_startSeconds};
    float displayEndSeconds{_endSeconds};
    float displayMilliAmpereHour{0.f};
    if (_currentBatteryStatus != BatteryStatus::NoBat)
    {
        displayMilliAmpereHour = _milliAmpereHour;
    }

    char startSecondsStr[128];
    printMinuteSecond(displayStartSeconds, startSecondsStr);
    char endSecondsStr[128];
    printMinuteSecond(displayEndSeconds, endSecondsStr);

    drawAdafruit.drawChar(startSecondsStr, virOffset, line);
    drawAdafruit.drawChar(endSecondsStr, virOffset + 9, line);

    if (0)
    {
        ++line;
        drawAdafruit.drawFillLine(line);
        drawAdafruit.drawInt(calcPWMValue(_targetI, ACTIVE_RATE, _batteryController->_calibI), SETTING_MENU_START_COL, line);
        drawAdafruit.drawString("PWM", SETTING_MENU_START_COL + SETTING_MENU_OFFSET_COL, line);
    }

    if (0)
    {
        ++line;
        drawAdafruit.drawFillLine(line);
        virOffset = DISPLAY_MENU_START_COL;
        virOffset += DISPLAY_MENU_OFFSET_COL;
        drawAdafruit.drawFloatR(_sleepV, virOffset, line, DISPLAY_MENU_OFFSET_COL, 3);
        drawAdafruit.drawString("V", virOffset, line);
    }

    if (0)
    {
        ++line;
        drawAdafruit.drawFillLine(line);
        virOffset = DISPLAY_MENU_START_COL;
        virOffset += DISPLAY_MENU_OFFSET_COL;
        drawAdafruit.drawFloatR(_i, virOffset, line, DISPLAY_MENU_OFFSET_COL, 3);
        drawAdafruit.drawString("A", virOffset, line);
    }

    ++line;
    drawAdafruit.drawFillLine(line);
    {
        virOffset = DISPLAY_MENU_START_COL;
        virOffset += DISPLAY_MENU_OFFSET_COL;
        drawAdafruit.drawFloatR(_ohm, virOffset, line, DISPLAY_MENU_OFFSET_COL, 1);

        static constexpr char CHAR_DATA_OHM[] = {0x6D, 0xe9, 0x00};
        drawAdafruit.drawChar(&CHAR_DATA_OHM[0], virOffset, line);

        virOffset += 2;
        virOffset += DISPLAY_MENU_OFFSET_COL;
        drawAdafruit.drawFloatR(displayMilliAmpereHour, virOffset, line, DISPLAY_MENU_OFFSET_COL, 1);
        drawAdafruit.drawString("mah", virOffset, line);
    }

    if (0)
    {
        ++line;
        drawAdafruit.drawFillLine(line);
        virOffset = DISPLAY_MENU_START_COL;
        virOffset += DISPLAY_MENU_OFFSET_COL;
        drawAdafruit.drawInt(_loopCount % sizeof(DISCHARGE_MODE_LOOPS), virOffset, line);
        static std::vector<String> modeNames{String("None"), String("Active"), String("Sleep"), String("Stop"), String("NoBat"), String("Max")};
        drawAdafruit.drawString(modeNames[(uint8_t)_currentBatteryStatus], virOffset + 5, line);
    }
}

void BatteryInfo::setDisplayPushData() const
{
    ++_displayCount;

    if (_tunedI > 0 && (_displayCount % 2))
    {
    }
    else
    {
        if (true)
        {
            constexpr std::pair<int, int> positionArray[4] = {{12, 28}, {12, 46}, {72, 28}, {72, 46}};

            drawAdafruit.setFont(&BBHBogle_Regular9pt7b);

            {
                const std::pair<int, int> &position{positionArray[_batteryIndex]};
                drawAdafruit.setCursor(position.first, position.second);
                drawAdafruit.printString(String(_v, _batteryController->_decimal) + String("V"));
            }
            drawAdafruit.removeFont();
        }
        else
        {
            int batterySetIndex{_batteryIndex / 2};
            int virOffset{10 * batterySetIndex + (_batteryIndex % 2) * 0 + 8};
            int line{(_batteryIndex % 2) + 2};
            drawAdafruit.drawFloatR(_v, virOffset, line, 4, _batteryController->_decimal);
            drawAdafruit.drawString("V", virOffset, line);
        }
    }

    if (_displayFlag)
    {
    }
};

void BatteryInfo::setDisplayData() const
{
    ++_displayCount;

    if (_displayFlag)
    {
        drawAdafruit.drawString(">", 5 * _batteryIndex, 0);
    }

    if (_tunedI > 0 && (_displayCount % 2))
    {
    }
    else
    {
        drawAdafruit.drawFloatR(_sleepV, 5 * (_batteryIndex + 1), 0, 4, 2);
    }

    if (_displayFlag)
    {
        if (_activeFlag)
        {
            setDisplayDetail();
        }
        else
        {
            setDisplayVoltOnly();
        }
    }
};

void BatteryInfo::read()
{
    constexpr int MIN_VOLT{10};
    const int volt{analogRead(_readPin)};
    _valueCounter.readVolt(volt);
    if (volt < MIN_VOLT)
    {
        _nextBatteryStatus = BatteryStatus::NoBat;
        if (_nextBatteryStatus != _currentBatteryStatus)
        {
            reset();
        }
        _v = 0;
    }
};

void BatteryInfo::setup()
{
    pinMode(_readPin, INPUT);
    pinMode(_writePin, OUTPUT);
    reset();
};
