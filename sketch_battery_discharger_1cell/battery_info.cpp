#include "battery_info.hpp"
#include "display/draw_adafruit.hpp"
#include "voltage_mapping.hpp"
#include "save_config_data.hpp"
#include "battery_controller.hpp"

extern VoltageMapping voltageMapping;
extern DrawAdafruit drawAdafruit;

const std::vector<String> DISC_MODE_NAMES{String("Cont"), String("Stop")};

void setDisplayTuneMenu(DrawAdafruit &adafruit, String &&title, std::vector<String> &menuList, std::vector<String> &valueList, int targetIndex)
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

void SaveBattery::setDisplayBatteryConfig(int index, BatteryConfigSettingMode settingMode) const
{
    std::vector<String> menuList{"TargetV", "TargetI", "DiscMode"};

    // String valueStr{String(value, decimal)};
    String mode{DISC_MODE_NAMES[(uint8_t)disChargeMode]};
    std::vector<String> valueList{String(targetV, 3), String(targetI), mode};

    String Title{"Battery Pair."};
    Title += String(index + 1);
    setDisplayTuneMenu(drawAdafruit, std::move(Title), menuList, valueList, static_cast<int>(settingMode));
    // displayDischargeFloat(drawAdafruit, "TargetA", "A", saveBattery->targetI);
}

float BatteryInfo::calcI(const float targetI, const float V, const float targetV, const DisChargeMode disChargeMode)
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

int BatteryInfo::calcPWMValue(float ampere, float active_rate, float calibI)
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

    return std::clamp(static_cast<int>(voltRate * MAX_PWM_F * ((AMP_TUNE * calibI) / active_rate)), 0, MAX_PWM);
};

void BatteryInfo::loopSubPushDischarge()
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
    int intValue = calcPWMValue(I, 1.f, batteryController->calibI);
    analogWrite(writePin, intValue);
}

void BatteryInfo::writePinReset() const
{
    analogWrite(writePin, 0);
}

void BatteryInfo::loopSubNormalDischarge()
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
            // 放電中
            if (tunedI > 0)
            {
                nextBatteryStatus = BatteryStatus::Active;
                if (nextBatteryStatus != currentBatteryStatus)
                {
                    // 停止中、初期からの、放電再開
                    reset();
                }
            }
            // 放電完了
            else if (tunedI == 0)
            {
                nextBatteryStatus = BatteryStatus::Stop;
            }
        }
    }
    else if (currentBatteryStatus == BatteryStatus::NoBat)
    {
        if (V > 0.1f)
        {
            // バッテリーセットからの、放電再開
            nextBatteryStatus = BatteryStatus::Active;
            {
                reset();
                milliAmpereHour = 0.f;
                startTime = millis();
            }
        }
    }

    int intValue = calcPWMValue(I, ACTIVE_RATE, batteryController->calibI);
    analogWrite(writePin, intValue);
};

void BatteryInfo::setDisplayVoltOnly() const
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
};

void BatteryInfo::setDisplayDetail() const
{
    static constexpr int DISPLAY_MENU_START_COL{3};
    static constexpr int DISPLAY_MENU_OFFSET_COL{5};
    int vir_offset{0};
    int line{START_LINE};
    drawAdafruit.drawFillLine(line);

    vir_offset = DISPLAY_MENU_START_COL;
    drawAdafruit.drawStringC(DISC_MODE_NAMES[(uint8_t)disChargeMode], line);

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
    int sec{(millis() - startTime) / 1000.f};
    int min{sec / 60.f};
    // drawAdafruit.drawIntR(sec, vir_offset, line);
    float displayMilliAmpereHour{milliAmpereHour};
    if (currentBatteryStatus == BatteryStatus::NoBat)
    {
        min = 0;
        sec = 0;
        displayMilliAmpereHour = 0.f;
    }
    char str[128];
    sprintf(str, "%02d:%02d", min, sec % 60);
    drawAdafruit.drawChar(str, vir_offset, line);

    if (0)
    {
        ++line;
        drawAdafruit.drawFillLine(line);
        drawAdafruit.drawInt(calcPWMValue(targetI, ACTIVE_RATE, batteryController->calibI), SETTING_MENU_START_COL, line);
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
        drawAdafruit.drawFloatR(displayMilliAmpereHour, vir_offset, line, DISPLAY_MENU_OFFSET_COL, 1);
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

void BatteryInfo::setDisplayPushData() const
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
        drawAdafruit.drawFloatR(V, vir_offset, line, 4, batteryController->decimal);
        drawAdafruit.drawString("V", vir_offset, line);
    }

    if (displayFlag)
    {
        // setDisplayVoltOnly();
    }
};

void BatteryInfo::setDisplayData() const
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
    const int volt{analogRead(readPin)};
    valueCounter.readVolt(volt);
    if (volt < MIN_VOLT)
    {
        nextBatteryStatus = BatteryStatus::NoBat;
        V = 0;
    }
};

void BatteryInfo::setup()
{
    pinMode(readPin, INPUT);
    pinMode(writePin, OUTPUT);
    reset();
    milliAmpereHour = 0.f;
    startTime = millis();
};
