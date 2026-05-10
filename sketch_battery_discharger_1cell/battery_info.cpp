#include <string>
#include "battery_info.hpp"
#include "src/display/adafruit_gfx_utility.hpp"
#include "voltage_mapping.hpp"
#include "save_config_data.hpp"
#include "battery_controller.hpp"

#include "display/fonts/BBHBogle-Regular_9.h"
#include "display/fonts/BBHBogle-Regular_12.h"

const std::vector<String> DISC_MODE_NAMES{String("Keep"), String("KeepMin"), String("Stop")};

const std::vector<String> REDUCE_MODE_NAMES{String("Mild"), String("Normal"), String("Hard"), String("None")};

void printMinuteSecond(int sec, char *str)
{
    int min{sec / 60.f};
    sprintf(str, "%02d:%02d", min, sec % 60);
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
    static const int MAX_PWM{0xFF};
    static const float MAX_PWM_F{static_cast<float>(MAX_PWM)};
    static const float REG{0.1f};
    static const float REG_RATE{(RES_A + RES_B + RES_C) / RES_C};
    static const float I_TO_V{REG * REG_RATE};
    static const float TO_V_RATE{I_TO_V / VOLT3_3};
    static const float AMP_TUNE{1.04f};

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
            _ohm = ((_sleepV - _v) * 1000.f) * (1.f / _tunedI);
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
                _ohm = ((_sleepV - _v) * 1000.f) * (ACTIVE_RATE / _tunedI);
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
                else if (_disChargeMode == DisChargeMode::DischargeHoldMin)
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

void BatteryInfo::setDisplayVoltOnly(Adafruit_SSD1306 &display) const
{
    static constexpr int DISPLAY_MENU_START_COL{3};
    static constexpr int DISPLAY_MENU_OFFSET_COL{5};
    int virOffset{0};
    int line{START_LINE};

    AdafruitGfxUtility::drawFillLine(display, line);
    AdafruitGfxUtility::drawFillLine(display, line + 1);
    AdafruitGfxUtility::drawFillLine(display, line + 2);
    AdafruitGfxUtility::drawFillLine(display, line + 3);
    AdafruitGfxUtility::drawFillLine(display, line + 4);

    ++line;
    display.setFont(&BBHBogle_Regular12pt7b);

    display.setCursor(32, 36);
    display.print(String(_sleepV, 3) + String("V"));

    display.setFont(nullptr);
}

void BatteryInfo::setDisplayDetailOld(Adafruit_SSD1306 &display) const
{
    static constexpr int DISPLAY_MENU_START_COL{3};
    static constexpr int DISPLAY_MENU_OFFSET_COL{5};
    int virOffset{0};
    int line{START_LINE};
    AdafruitGfxUtility::drawFillLine(display, line);

    virOffset = DISPLAY_MENU_START_COL;
    AdafruitGfxUtility::drawStringC(display, DISC_MODE_NAMES[static_cast<uint8_t>(_disChargeMode)], line);

    ++line;
    AdafruitGfxUtility::drawFillLine(display, line);

    virOffset = DISPLAY_MENU_START_COL;
    virOffset += DISPLAY_MENU_OFFSET_COL;
    AdafruitGfxUtility::drawFloatR(display, _sleepV, virOffset, line, DISPLAY_MENU_OFFSET_COL, 3);
    AdafruitGfxUtility::drawString(display, "V", virOffset, line);

    virOffset += 2;
    if ((_tunedI > 0.f) && (_displayCount % 2))
    {
    }
    else
    {
        AdafruitGfxUtility::drawChar(display, &DisplayConst::CHAR_DATA_ARROW_NEW[0], virOffset, line);
    }

    virOffset += 2;
    virOffset += DISPLAY_MENU_OFFSET_COL;
    AdafruitGfxUtility::drawFloatR(display, _targetV, virOffset, line, DISPLAY_MENU_OFFSET_COL, 3);
    AdafruitGfxUtility::drawString(display, "V", virOffset, line);

    ++line;
    AdafruitGfxUtility::drawFillLine(display, line);

    virOffset = DISPLAY_MENU_START_COL;
    virOffset += DISPLAY_MENU_OFFSET_COL;
    float displayI{std::max(0.f, _tunedI)};
    AdafruitGfxUtility::drawFloatR(display, displayI, virOffset, line, DISPLAY_MENU_OFFSET_COL, 3);
    AdafruitGfxUtility::drawString(display, "A", virOffset, line);

    virOffset += 2;

    AdafruitGfxUtility::drawChar(display, &DisplayConst::CHAR_DATA_ARROW_NEW[0], virOffset, line);

    virOffset += 2;
    virOffset += SETTING_MENU_OFFSET_COL;
    AdafruitGfxUtility::drawFloatR(display, _targetI, virOffset, line, SETTING_MENU_OFFSET_COL, 3);
    AdafruitGfxUtility::drawString(display, "A", virOffset, line);

    ++line;
    AdafruitGfxUtility::drawFillLine(display, line);

    virOffset = DISPLAY_MENU_START_COL + 1;

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

    AdafruitGfxUtility::drawChar(display, startSecondsStr, virOffset, line);
    AdafruitGfxUtility::drawChar(display, endSecondsStr, virOffset + 8, line);

    if (0)
    {
        ++line;
        AdafruitGfxUtility::drawFillLine(display, line);
        AdafruitGfxUtility::drawInt(display, calcPWMValue(_targetI, ACTIVE_RATE, _batteryController->_calibI), SETTING_MENU_START_COL, line);
        AdafruitGfxUtility::drawString(display, "PWM", SETTING_MENU_START_COL + SETTING_MENU_OFFSET_COL, line);
    }

    if (0)
    {
        ++line;
        AdafruitGfxUtility::drawFillLine(display, line);
        virOffset = DISPLAY_MENU_START_COL;
        virOffset += DISPLAY_MENU_OFFSET_COL;
        AdafruitGfxUtility::drawFloatR(display, _sleepV, virOffset, line, DISPLAY_MENU_OFFSET_COL, 3);
        AdafruitGfxUtility::drawString(display, "V", virOffset, line);
    }

    if (0)
    {
        ++line;
        AdafruitGfxUtility::drawFillLine(display, line);
        virOffset = DISPLAY_MENU_START_COL;
        virOffset += DISPLAY_MENU_OFFSET_COL;
        AdafruitGfxUtility::drawFloatR(display, _i, virOffset, line, DISPLAY_MENU_OFFSET_COL, 3);
        AdafruitGfxUtility::drawString(display, "A", virOffset, line);
    }

    ++line;
    AdafruitGfxUtility::drawFillLine(display, line);
    {
        virOffset = DISPLAY_MENU_START_COL;
        virOffset += DISPLAY_MENU_OFFSET_COL;
        virOffset -= 1;
        AdafruitGfxUtility::drawFloatR(display, _ohm, virOffset, line, DISPLAY_MENU_OFFSET_COL, 1);

        static constexpr char CHAR_DATA_OHM[] = {0x6D, 0xe9, 0x00};
        AdafruitGfxUtility::drawChar(display, &CHAR_DATA_OHM[0], virOffset, line);

        virOffset += 5;
        virOffset += DISPLAY_MENU_OFFSET_COL;
        AdafruitGfxUtility::drawFloatR(display, displayMilliAmpereHour, virOffset, line, DISPLAY_MENU_OFFSET_COL, 1, 3);
        AdafruitGfxUtility::drawString(display, "mAh", virOffset, line);
    }

    if (0)
    {
        ++line;
        AdafruitGfxUtility::drawFillLine(display, line);
        virOffset = DISPLAY_MENU_START_COL;
        virOffset += DISPLAY_MENU_OFFSET_COL;
        AdafruitGfxUtility::drawInt(display, _loopCount % sizeof(DISCHARGE_MODE_LOOPS), virOffset, line);
        static std::vector<String> modeNames{String("None"), String("Active"), String("Sleep"), String("Stop"), String("NoBat"), String("Max")};
        AdafruitGfxUtility::drawString(display, modeNames[(uint8_t)_currentBatteryStatus], virOffset + 5, line);
    }
}

void BatteryInfo::setDisplayDetail(Adafruit_SSD1306 &display) const
{
    int line{START_LINE};
    float displayI{std::max(0.f, _tunedI)};
    float displayMilliAmpereHour{0.f};
    if (_currentBatteryStatus != BatteryStatus::NoBat)
    {
        displayMilliAmpereHour = _milliAmpereHour;
    }

    AdafruitGfxUtility::drawFillLine(display, line);
    AdafruitGfxUtility::drawFillLine(display, line + 1);
    AdafruitGfxUtility::drawFillLine(display, line + 2);
    AdafruitGfxUtility::drawFillLine(display, line + 3);
    AdafruitGfxUtility::drawFillLine(display, line + 4);

    AdafruitGfxUtility::drawStringC(display, DISC_MODE_NAMES[static_cast<uint8_t>(_disChargeMode)], line);
    display.drawFastHLine(12, 17, 104, SSD1306_WHITE);

    const String voltageArrow{((_tunedI > 0.f) && (_displayCount % 2)) ? String("  ") : String("->")};
    const String voltageText{
        AdafruitGfxUtility::formatFloatZeroPad(_sleepV, 1, 3) +
        voltageArrow +
        AdafruitGfxUtility::formatFloatZeroPad(_targetV, 1, 3) +
        String("V")
    };

    display.setFont(&BBHBogle_Regular12pt7b);
    int16_t voltageX{0};
    int16_t voltageY{0};
    uint16_t voltageW{0};
    uint16_t voltageH{0};
    display.getTextBounds(voltageText, 0, 0, &voltageX, &voltageY, &voltageW, &voltageH);
    display.setCursor((AdafruitGfxUtility::SCREEN_WIDTH - voltageW) / 2 - voltageX, 36);
    display.print(voltageText);
    display.setFont(nullptr);

    const String currentText{
        AdafruitGfxUtility::formatFloatZeroPad(displayI, 1, 3) +
        String("A -> ") +
        AdafruitGfxUtility::formatFloatZeroPad(_targetI, 1, 3) +
        String("A")
    };
    AdafruitGfxUtility::drawStringC(display, currentText, START_LINE + 3);

    const String milliAmpereHourText{
        AdafruitGfxUtility::formatFloatZeroPad(displayMilliAmpereHour, 1, 1) +
        String("mAh")
    };
    AdafruitGfxUtility::drawStringC(display, milliAmpereHourText, START_LINE + 4);
}

void BatteryInfo::setDisplayPushData(Adafruit_SSD1306 &display) const
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

            display.setFont(&BBHBogle_Regular9pt7b);

            {
                const std::pair<int, int> &position{positionArray[_batteryIndex]};
                display.setCursor(position.first, position.second);
                display.print(String(_v, _batteryController->_decimal) + String("V"));
            }
            display.setFont(nullptr);
        }
        else
        {
            int batterySetIndex{_batteryIndex / 2};
            int virOffset{10 * batterySetIndex + (_batteryIndex % 2) * 0 + 8};
            int line{(_batteryIndex % 2) + 2};
            AdafruitGfxUtility::drawFloatR(display, _v, virOffset, line, 4, _batteryController->_decimal);
            AdafruitGfxUtility::drawString(display, "V", virOffset, line);
        }
    }

    if (_displayFlag)
    {
    }
};

void BatteryInfo::setDisplayData(Adafruit_SSD1306 &display) const
{
    ++_displayCount;

    if (_displayFlag)
    {
        AdafruitGfxUtility::drawString(display, ">", 5 * _batteryIndex, 0);
    }

    if (_tunedI > 0 && (_displayCount % 2))
    {
    }
    else
    {
        AdafruitGfxUtility::drawFloatR(display, _sleepV, 5 * (_batteryIndex + 1), 0, 4, 2);
    }

    if (_displayFlag)
    {
        if (_activeFlag)
        {
            setDisplayDetailOld(display);
        }
        else
        {
            setDisplayVoltOnly(display);
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
