#include "battery_controller.hpp"

// #define DEEP_SLEEP_ESCAPE_PIN   D14
#include <ArduinoLowPower.h>
#include <EEPROM.h>
#include "display/draw_adafruit.hpp"
#include "voltage_mapping.hpp"

extern DrawAdafruit drawAdafruit;
extern VoltageMapping voltageMapping;

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

void callback()
{
    int count{};
    count++;
}

void BatteryController::saveConfig()
{
    saveConfigData.id = SAVEDATA_ID;
    saveCustomData<SaveConfigData>((byte *)&saveConfigData);
}

void BatteryController::saveMain()
{
    saveBatteryConfigData.id = SAVEDATA_ID;
    saveCustomData<SaveBatteryConfigData>((byte *)&saveBatteryConfigData);
}

void BatteryController::loadConfig()
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

void BatteryController::loadMain()
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

void BatteryController::clearEEPROM()
{
    const uint8_t clearSize{64};
    for (int i = 0; i < clearSize; ++i)
    {
        EEPROM.write(i, 0xFF);
    }
}

void BatteryController::setup()
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
    pinMode(PUSH_BUTTON_A, INPUT_PULLUP);

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
    buttonAStatus.init(PUSH_BUTTON_A);

#if defined(V1_PCB) || defined(V2_PCB)
    int val{HIGH};
    val = digitalRead(PUSH_BUTTON_A);
    if (val != LOW) // val == LOW)
    {
        loadMain();
        loadConfig();
    }
    else
    {
        saveMain();
        saveConfig();
    }

    pinMode(PUSH_BUTTON_ON, INPUT_PULLUP);
    pinMode(PUSH_BUTTON_B, INPUT_PULLUP);

    buttonONStatus.init(PUSH_BUTTON_ON);
    buttonBStatus.init(PUSH_BUTTON_B);
#else

    int val{HIGH};
    val = digitalRead(PUSH_BUTTON_D);
    if (val != LOW) // val == LOW)
    {
        loadMain();
        loadConfig();
    }
    else
    {
        saveMain();
        saveConfig();
    }

#endif

    updateBatterySaveData();
    updateConfigSaveData();
};

void BatteryController::updateConfigSaveData()
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
    calibI = saveConfigData.calibI;
    decimal = saveConfigData.decimal;
    dischargeI = saveConfigData.dischargeI;
}

void BatteryController::updateBatterySaveData()
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
        batteryStatus.reduceMode = saveBattery->reduceMode;
        batteryStatus.holdMin = saveBattery->holdMin;
        // batteryStatus.activeFlag = saveBattery->activeFlag;

        batteryStatus.setup();
    }
}

float BatteryController::readAndDrawXiaoBattery()
{
    constexpr static float R_RATE{2.f};
    const int readValue{analogRead(XIAO_READ_BAT)};
    const float xiaoVolt{(static_cast<float>(readValue) / 4096.f) * R_RATE * VOLT3_3};
    // const float xiaoVolt{ (2048.f / 4096.f) * R_RATE * VOLT3_3 };
    drawAdafruit.drawBat(xiaoVolt);

    return xiaoVolt;
}

void BatteryController::setDisplayConfig() const
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
        String(saveConfigData.calibI),
        String(saveConfigData.decimal),
    };

    setDisplayTuneMenu(drawAdafruit, "Config", menuList, valueList, static_cast<int>(configSettingMode));
}

void BatteryController::setDisplayPushDischarge() const
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

void BatteryController::setDisplayNone() const
{
    // int line{0};
    drawAdafruit.clearDisplay();
}

void BatteryController::setDisplayData() const
{
    drawAdafruit.drawFillLine(0);
    for (auto &batteryStatus : batteryStatuses)
    {
        batteryStatus.setDisplayData();
    }
};

void BatteryController::writePinReset()
{
    for (auto &batteryStatus : batteryStatuses)
    {
        batteryStatus.writePinReset();
    }
}

void BatteryController::goDeepSleep()
{
    drawAdafruit.clearDisplay();
    drawAdafruit.display();
    LowPower.attachInterruptWakeup(WAKE_UP_PIN, callback, RISING);

    writePinReset();

    LowPower.deepSleep(365 * 24 * 3600 * 1000); // 7 * 24 * 3600 * 1000 // one week
}

void BatteryController::shiftTargetBattery(int shift)
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

void BatteryController::changeTargetBattery(int batteryIndex)
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

void BatteryController::shiftParam(int shift)
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
            saveConfigData.dischargeI = std::clamp(saveConfigData.dischargeI + (shift * 0.1f), 0.4f, 3.f);
        }
        else if (configSettingMode == ConfigSettingMode::tuneISetting)
        {
            saveConfigData.calibI = std::clamp(saveConfigData.calibI + (shift * 0.1f), 0.8f, 1.2f);
        }
        else if (configSettingMode == ConfigSettingMode::decimalSetting)
        {
            saveConfigData.decimal = std::clamp(saveConfigData.decimal + shift, 2, 3);
        }
    }
};

void BatteryController::changeSettingMode(int shift)
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

void BatteryController::updateButtonStatus()
{
    buttonLStatus.update();
    buttonRStatus.update();
    buttonUStatus.update();
    buttonDStatus.update();
    buttonAStatus.update();
    buttonBStatus.update();
    buttonONStatus.update();

    MainMode nextMode{mainMode};
    if (mainMode == MainMode::DischargerMode)
    {
        PushType pushType{0};
        pushType = buttonLStatus.getVal();
        if (pushType == PushType::ReleaseShort)
        {
            shiftTargetBattery(-1);
        }
        pushType = buttonRStatus.getVal();
        if (pushType == PushType::ReleaseShort)
        {
            shiftTargetBattery(1);
        }
        pushType = buttonAStatus.getVal();
        if (pushType == PushType::ReleaseShort)
        {
            changeActive(1);
        }
        pushType = buttonBStatus.getVal();
        if (pushType == PushType::ReleaseShort)
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
            writePinReset();
            nextMode = MainMode::BatteryConfigMode;
        }

        pushType = buttonONStatus.getVal();
        if (pushType == PushType::ReleaseShort)
        {
            for (auto &batteryStatus : batteryStatuses)
            {
                batteryStatus.nextBatteryStatus = BatteryStatus::None;
                batteryStatus.activeFlag = false;
                batteryStatus.reset();
            }
            nextMode = MainMode::PushDischargerMode;
        }

    }
    else if (mainMode == MainMode::BatteryConfigMode)
    {
        PushType pushType{0};
        pushType = buttonLStatus.getVal();
        if (pushType == PushType::ReleaseShort || pushType == PushType::PushLong)
        {
            shiftParam(-1);
        }
        pushType = buttonRStatus.getVal();
        if (pushType == PushType::ReleaseShort || pushType == PushType::PushLong)
        {
            shiftParam(1);
        }
        pushType = buttonUStatus.getVal();
        if (pushType == PushType::ReleaseShort)
        {
            changeSettingMode(-1);
        }
        pushType = buttonDStatus.getVal();
        if (pushType == PushType::ReleaseShort)
        {
            changeSettingMode(1);
        }
        pushType = buttonAStatus.getVal();
        if (pushType == PushType::ReleaseShort)
        {
            changeTargetBatterySetting(1);
            shiftTargetBattery(1);
        }
        pushType = buttonBStatus.getVal();
        if (pushType == PushType::ReleaseShort)
        {
            updateBatterySaveData();
            saveMain();
            for (auto &batteryStatus : batteryStatuses)
            {
                batteryStatus.nextBatteryStatus = BatteryStatus::None;
                batteryStatus.activeFlag = false;
                batteryStatus.reset();
            }
            nextMode = MainMode::DischargerMode;
        }
    }
    else if (mainMode == MainMode::ConfigMode)
    {
        PushType pushType{0};
        pushType = buttonLStatus.getVal();
        if (pushType == PushType::ReleaseShort || pushType == PushType::PushLong)
        {
            shiftParam(-1);
        }
        pushType = buttonRStatus.getVal();
        if (pushType == PushType::ReleaseShort || pushType == PushType::PushLong)
        {
            shiftParam(1);
        }
        pushType = buttonUStatus.getVal();
        if (pushType == PushType::ReleaseShort)
        {
            changeSettingMode(-1);
        }
        pushType = buttonDStatus.getVal();
        if (pushType == PushType::ReleaseShort)
        {
            changeSettingMode(1);
        }
        pushType = buttonBStatus.getVal();
        if (pushType == PushType::ReleaseShort)
        {
            updateConfigSaveData();
            saveConfig();
            for (auto &batteryStatus : batteryStatuses)
            {
                batteryStatus.nextBatteryStatus = BatteryStatus::None;
                batteryStatus.activeFlag = false;
                batteryStatus.reset();
            }
            nextMode = cachedMainMode;
        }
    }
    else if (mainMode == MainMode::PushDischargerMode)
    {
        PushType pushType{PushType::None};
        int numBattery{0};
        numBattery = 0;
        pushType = buttonLStatus.getVal();
        if (pushType == PushType::PushLong || pushType == PushType::PushShort)
        {
            batteryStatuses[numBattery].pushOn(dischargeI);
        }
        else if(pushType == PushType::None)
        {
            batteryStatuses[numBattery].pushOff();
        }

#if defined(V1_PCB)
        numBattery = 2;
#else
        numBattery = 1;
#endif
        pushType = buttonRStatus.getVal();
        if (pushType == PushType::PushLong || pushType == PushType::PushShort)
        {
            batteryStatuses[numBattery].pushOn(dischargeI);
        }
        else if(pushType == PushType::None)
        {
            batteryStatuses[numBattery].pushOff();
        }

#if defined(V1_PCB)
        numBattery = 1;
#else
        numBattery = 2;
#endif
        pushType = buttonAStatus.getVal();
        if (pushType == PushType::PushLong || pushType == PushType::PushShort)
        {
            batteryStatuses[numBattery].pushOn(dischargeI);
        }
        else if(pushType == PushType::None)
        {
            batteryStatuses[numBattery].pushOff();
        }

        numBattery = 3;
        pushType = buttonBStatus.getVal();
        if (pushType == PushType::PushLong || pushType == PushType::PushShort)
        {
            batteryStatuses[numBattery].pushOn(dischargeI);
        }
        else if(pushType == PushType::None)
        {
            batteryStatuses[numBattery].pushOff();
        }

        pushType = buttonONStatus.getVal();
        if (pushType == PushType::ReleaseShort)
        {
            nextMode = MainMode::DischargerMode;
        }
    }

    if ((buttonUStatus.getVal() == PushType::PushShort || buttonUStatus.getVal() == PushType::PushLong) && (buttonDStatus.getVal() == PushType::PushShort || buttonDStatus.getVal() == PushType::PushLong))
    {
        if (mainMode == MainMode::DischargerMode || mainMode == MainMode::PushDischargerMode)
        {
            writePinReset();
            cachedMainMode = mainMode;
            nextMode = MainMode::ConfigMode;
        }
    }

    if (buttonONStatus.getVal() == PushType::PushLong)
    {
        clearDisplayFlag = true;
    }
    else if (buttonONStatus.getVal() == PushType::ReleaseLong)
    {
        goDeepSleep();
    }

    mainMode = nextMode;

}

void BatteryController::loopSub()
{
    ++loopSubCount;
    if ((loopSubCount % 60) == 0)
    {
        const float xiaoVolt{readAndDrawXiaoBattery()};

        if (xiaoVoltFlag)
        {
            if (xiaoVolt < (VOLT3_3 + 0.2f))
            {
                xiaoVoltFlag = false;
            }
        }
        else
        {
            if (xiaoVolt > (VOLT3_3 + 0.3f))
            {
                xiaoVoltFlag = true;
            }
        }
    }

    if (!xiaoVoltFlag)
    {
        drawAdafruit.display();
        // goDeepSleep();
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

    if (clearDisplayFlag)
    {
        setDisplayNone();
        drawAdafruit.display();
    }
    else
    {
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
    }

};
