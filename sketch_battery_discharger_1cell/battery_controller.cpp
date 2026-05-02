#include "battery_controller.hpp"

// #define DEEP_SLEEP_ESCAPE_PIN   D14
#include <EEPROM.h>
#include "display/draw_adafruit.hpp"

extern DrawAdafruit drawAdafruit;

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

void BatteryController::saveConfig()
{
    _saveConfigData._id = SAVEDATA_ID;
    saveCustomData<SaveConfigData>((byte *)&_saveConfigData);
}

void BatteryController::saveMain()
{
    _saveBatteryConfigData._id = SAVEDATA_ID;
    saveCustomData<SaveBatteryConfigData>((byte *)&_saveBatteryConfigData);
}

void BatteryController::loadConfig()
{
    SaveConfigData tempData;
    loadCustomData<SaveConfigData>((byte *)&tempData);
    if (tempData._id != SAVEDATA_ID)
    {
        return;
    }
    if (tempData._ver != _saveConfigData._ver)
    {
        return;
    }
    _saveConfigData = tempData;
}

void BatteryController::loadMain()
{
    SaveBatteryConfigData tempData;
    loadCustomData<SaveBatteryConfigData>((byte *)&tempData);
    if (tempData._id != SAVEDATA_ID)
    {
        return;
    }
    if (tempData._ver != _saveBatteryConfigData._ver)
    {
        return;
    }
    _saveBatteryConfigData = tempData;
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
    drawAdafruit.setupDisplay();

    digitalWrite(PA6, HIGH); // FLASH

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
    pinMode(PUSH_BUTTON_B, INPUT_PULLUP);
    pinMode(PUSH_BUTTON_ON, INPUT_PULLUP);

    _buttonLStatus.init(PUSH_BUTTON_L);
    _buttonRStatus.init(PUSH_BUTTON_R);
    _buttonUStatus.init(PUSH_BUTTON_U);
    _buttonDStatus.init(PUSH_BUTTON_D);
    _buttonAStatus.init(PUSH_BUTTON_A);
    _buttonBStatus.init(PUSH_BUTTON_B);
    _buttonOnStatus.init(PUSH_BUTTON_ON);

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

    updateBatterySaveData();
    updateConfigSaveData();
};

void BatteryController::updateConfigSaveData()
{

    const static SaveConfigData defaultSaveConfigData{};
    if (_saveConfigData._id != SAVEDATA_ID)
    {
        _saveConfigData = defaultSaveConfigData;
    }

    std::vector<int> customMappingData{};
    for (uint8_t i{0}; i < SaveConfigData::VOLT_DATA_SIZE; ++i)
    {
        customMappingData.push_back(_saveConfigData._voltDatas[i]);
    }
    _voltageMapping.initMapping(customMappingData);

    _ledOnFlag = _saveConfigData._ledOnFlag;
    _calibI = _saveConfigData._calibI;
    _decimal = _saveConfigData._decimal;
    _dischargeI = _saveConfigData._dischargeI;
}

void BatteryController::updateBatterySaveData()
{
    for (int i{0}; i < _batteryStatuses.size(); ++i)
    {
        auto &batteryStatus{_batteryStatuses[i]};

        const static SaveBattery defaultSaveBattery{};
        auto *saveBattery{batteryStatus._saveBattery};
        if (_saveBatteryConfigData._id != SAVEDATA_ID)
        {
            *saveBattery = defaultSaveBattery;
        }

        batteryStatus._targetI = saveBattery->_targetI;
        batteryStatus._targetV = saveBattery->_targetV;
        batteryStatus._disChargeMode = saveBattery->_disChargeMode;
        batteryStatus._reduceMode = saveBattery->_reduceMode;
        batteryStatus._holdMin = saveBattery->_holdMin;

        batteryStatus.setup();
    }
}

float BatteryController::readAndDrawXiaoBattery()
{
    constexpr static float R_RATE{2.f};
    const int readValue{analogRead(XIAO_READ_BAT)};
    const float xiaoVolt{(static_cast<float>(readValue) / 4096.f) * R_RATE * VOLT3_3};
    drawAdafruit.drawBat(xiaoVolt);

    return xiaoVolt;
}

void BatteryController::setDisplayConfig() const
{
    std::vector<String> menuList{"0.0V", "0.5V", "1.0V", "1.5V", "2.0V", "LedOn", "DiscI", "AmpTune", "Decimal"};

    std::vector<String> valueList{
        String(_saveConfigData._voltDatas[0]),
        String(_saveConfigData._voltDatas[1]),
        String(_saveConfigData._voltDatas[2]),
        String(_saveConfigData._voltDatas[3]),
        String(_saveConfigData._voltDatas[4]),
        String(_saveConfigData._ledOnFlag == 0 ? false : true),
        String(_saveConfigData._dischargeI),
        String(_saveConfigData._calibI),
        String(_saveConfigData._decimal),
    };

    setDisplayTuneMenu(drawAdafruit, "Config", menuList, valueList, static_cast<int>(_configSettingMode));
}

void BatteryController::setDisplayPushDischarge() const
{
    for (int i = 0; i < 6; ++i)
    {
        drawAdafruit.drawFillLine(i);
    }
    drawAdafruit.drawStringC(String("Discharge ") + String(_dischargeI) + String("A"), 0);
    for (auto &batteryStatus : _batteryStatuses)
    {
        batteryStatus.setDisplayPushData();
    }

    int line{4};
    int virOffset{0};
    if (0)
    {
        float lV{_batteryStatuses[0]._v + _batteryStatuses[1]._v};
        float rV{_batteryStatuses[2]._v + _batteryStatuses[3]._v};

        virOffset += 10;
        drawAdafruit.drawFloatR(lV, virOffset, line, 4, _saveConfigData._decimal);
        drawAdafruit.drawString("V", virOffset, line);
        virOffset += 10;
        drawAdafruit.drawFloatR(rV, virOffset, line, 4, _saveConfigData._decimal);
        drawAdafruit.drawString("V", virOffset, line);
    }

    const BatteryInfo *targetBatteryStatus{nullptr};
    for (auto &batteryStatus : _batteryStatuses)
    {
        if (batteryStatus._tunedI > 0.f)
        {
            targetBatteryStatus = &batteryStatus;
            continue;
        }
    }
    virOffset = 16;
    line = 6;
    if (targetBatteryStatus)
    {
        drawAdafruit.drawFillR(virOffset, line, 6);
        drawAdafruit.drawFloatR(targetBatteryStatus->_ohm, virOffset, line, 4, 1);
        static constexpr char CHAR_DATA_OHM[] = {0x6D, 0xe9, 0x00};
        drawAdafruit.drawChar(&CHAR_DATA_OHM[0], virOffset, line);
    }
}

void BatteryController::setDisplayNone() const
{
    drawAdafruit.clearDisplay();
}

void BatteryController::setDisplayData() const
{
    drawAdafruit.drawFillLine(0);
    for (auto &batteryStatus : _batteryStatuses)
    {
        batteryStatus.setDisplayData();
    }
};

void BatteryController::writePinReset()
{
    analogWrite(WRITE1_PIN, 0);
    analogWrite(WRITE2_PIN, 0);
    analogWrite(WRITE3_PIN, 0);
    analogWrite(WRITE4_PIN, 0);
}

void BatteryController::displaySleep()
{
    drawAdafruit.clearDisplay();
    drawAdafruit.display();
    drawAdafruit.displaySleep();
}

void BatteryController::shiftTargetBattery(int shift)
{
    _currentBatteryIndex = (_currentBatteryIndex + shift) % _batteryStatuses.size();

    for (size_t index{0}; index < _batteryStatuses.size(); ++index)
    {
        if (index == _currentBatteryIndex)
        {
            _batteryStatuses[index]._displayFlag = true;
        }
        else
        {
            _batteryStatuses[index]._displayFlag = false;
        }
    }
};

void BatteryController::changeTargetBattery(int batteryIndex)
{
    _currentBatteryIndex = batteryIndex;

    for (size_t index{0}; index < _batteryStatuses.size(); ++index)
    {
        if (index == _currentBatteryIndex)
        {
            _batteryStatuses[index]._displayFlag = true;
        }
        else
        {
            _batteryStatuses[index]._displayFlag = false;
        }
    }
};

void BatteryController::shiftParam(int shift)
{
    if (_mainMode == MainMode::BatteryConfigMode)
    {
        _saveBatteryConfigData._battery[_currentBatterySettingIndex].shiftParam(_batteryConfigSettingMode, shift);
    }
    else if (_mainMode == MainMode::ConfigMode)
    {
        if (_configSettingMode == ConfigSettingMode::Volt00Setting)
        {
            _saveConfigData._voltDatas[0] = SaveConfigData::voltClamp(_saveConfigData._voltDatas[0] + shift);
        }
        else if (_configSettingMode == ConfigSettingMode::Volt05Setting)
        {
            _saveConfigData._voltDatas[1] = SaveConfigData::voltClamp(_saveConfigData._voltDatas[1] + shift);
        }
        else if (_configSettingMode == ConfigSettingMode::Volt10Setting)
        {
            _saveConfigData._voltDatas[2] = SaveConfigData::voltClamp(_saveConfigData._voltDatas[2] + shift);
        }
        else if (_configSettingMode == ConfigSettingMode::Volt15Setting)
        {
            _saveConfigData._voltDatas[3] = SaveConfigData::voltClamp(_saveConfigData._voltDatas[3] + shift);
        }
        else if (_configSettingMode == ConfigSettingMode::Volt15Setting)
        {
            _saveConfigData._voltDatas[4] = SaveConfigData::voltClamp(_saveConfigData._voltDatas[4] + shift);
        }
        else if (_configSettingMode == ConfigSettingMode::LedOnSetting)
        {
            _saveConfigData._ledOnFlag = ((_saveConfigData._ledOnFlag == 0) ? 1 : 0);
        }
        else if (_configSettingMode == ConfigSettingMode::discISetting)
        {
            _saveConfigData._dischargeI = std::clamp(_saveConfigData._dischargeI + (shift * 0.1f), 0.4f, 3.f);
        }
        else if (_configSettingMode == ConfigSettingMode::tuneISetting)
        {
            _saveConfigData._calibI = std::clamp(_saveConfigData._calibI + (shift * 0.1f), 0.8f, 1.2f);
        }
        else if (_configSettingMode == ConfigSettingMode::decimalSetting)
        {
            _saveConfigData._decimal = std::clamp(_saveConfigData._decimal + shift, 2, 3);
        }
    }
};

void BatteryController::changeSettingMode(int shift)
{
    if (_mainMode == MainMode::BatteryConfigMode)
    {
        const int nextModeIndex{(static_cast<int>(BatteryConfigSettingMode::Max) + static_cast<int>(_batteryConfigSettingMode) + shift) % static_cast<int>(BatteryConfigSettingMode::Max)};
        _batteryConfigSettingMode = static_cast<BatteryConfigSettingMode>(nextModeIndex);
    }
    else if (_mainMode == MainMode::ConfigMode)
    {
        const int nextModeIndex{(static_cast<int>(ConfigSettingMode::Max) + static_cast<int>(_configSettingMode) + shift) % static_cast<int>(ConfigSettingMode::Max)};
        _configSettingMode = static_cast<ConfigSettingMode>(nextModeIndex);
    }
};

void BatteryController::updateButtonStatus()
{
    _buttonLStatus.update();
    _buttonRStatus.update();
    _buttonUStatus.update();
    _buttonDStatus.update();
    _buttonAStatus.update();
    _buttonBStatus.update();
    _buttonOnStatus.update();

    MainMode nextMode{_mainMode};
    if (_mainMode == MainMode::DischargerMode)
    {
        PushType pushType{0};
        pushType = _buttonLStatus.getVal();
        if (pushType == PushType::ReleaseShort)
        {
            shiftTargetBattery(-1);
        }
        pushType = _buttonRStatus.getVal();
        if (pushType == PushType::ReleaseShort)
        {
            shiftTargetBattery(1);
        }
        pushType = _buttonAStatus.getVal();
        if (pushType == PushType::ReleaseShort)
        {
            changeActive(1);
        }
        pushType = _buttonBStatus.getVal();
        if (pushType == PushType::ReleaseShort)
        {
            if (_batteryConfigNum == 2)
            {
                if (_currentBatteryIndex == 0 || _currentBatteryIndex == 1)
                {
                    _currentBatterySettingIndex = 0;
                }
                else if (_currentBatteryIndex == 2 || _currentBatteryIndex == 3)
                {
                    _currentBatterySettingIndex = 1;
                }
            }
            writePinReset();
            nextMode = MainMode::BatteryConfigMode;
        }

        pushType = _buttonOnStatus.getVal();
        if (pushType == PushType::ReleaseShort)
        {
            for (auto &batteryStatus : _batteryStatuses)
            {
                batteryStatus._nextBatteryStatus = BatteryStatus::None;
                batteryStatus._activeFlag = false;
                batteryStatus.reset();
            }
            nextMode = MainMode::PushDischargerMode;
        }

    }
    else if (_mainMode == MainMode::BatteryConfigMode)
    {
        PushType pushType{0};
        pushType = _buttonLStatus.getVal();
        if (pushType == PushType::ReleaseShort || pushType == PushType::PushLong)
        {
            shiftParam(-1);
        }
        pushType = _buttonRStatus.getVal();
        if (pushType == PushType::ReleaseShort || pushType == PushType::PushLong)
        {
            shiftParam(1);
        }
        pushType = _buttonUStatus.getVal();
        if (pushType == PushType::ReleaseShort)
        {
            changeSettingMode(-1);
        }
        pushType = _buttonDStatus.getVal();
        if (pushType == PushType::ReleaseShort)
        {
            changeSettingMode(1);
        }
        pushType = _buttonAStatus.getVal();
        if (pushType == PushType::ReleaseShort)
        {
            changeTargetBatterySetting(1);
            shiftTargetBattery(1);
        }
        pushType = _buttonBStatus.getVal();
        if (pushType == PushType::ReleaseShort)
        {
            updateBatterySaveData();
            saveMain();
            for (auto &batteryStatus : _batteryStatuses)
            {
                batteryStatus._nextBatteryStatus = BatteryStatus::None;
                batteryStatus._activeFlag = false;
                batteryStatus.reset();
            }
            nextMode = MainMode::DischargerMode;
        }
    }
    else if (_mainMode == MainMode::ConfigMode)
    {
        PushType pushType{0};
        pushType = _buttonLStatus.getVal();
        if (pushType == PushType::ReleaseShort || pushType == PushType::PushLong)
        {
            shiftParam(-1);
        }
        pushType = _buttonRStatus.getVal();
        if (pushType == PushType::ReleaseShort || pushType == PushType::PushLong)
        {
            shiftParam(1);
        }
        pushType = _buttonUStatus.getVal();
        if (pushType == PushType::ReleaseShort)
        {
            changeSettingMode(-1);
        }
        pushType = _buttonDStatus.getVal();
        if (pushType == PushType::ReleaseShort)
        {
            changeSettingMode(1);
        }
        pushType = _buttonBStatus.getVal();
        if (pushType == PushType::ReleaseShort)
        {
            updateConfigSaveData();
            saveConfig();
            for (auto &batteryStatus : _batteryStatuses)
            {
                batteryStatus._nextBatteryStatus = BatteryStatus::None;
                batteryStatus._activeFlag = false;
                batteryStatus.reset();
            }
            nextMode = _cachedMainMode;
        }
    }
    else if (_mainMode == MainMode::PushDischargerMode)
    {
        PushType pushType{PushType::None};
        int numBattery{0};
        numBattery = 0;
        pushType = _buttonLStatus.getVal();
        if (pushType == PushType::PushLong || pushType == PushType::PushShort || pushType == PushType::Pushed)
        {
            _batteryStatuses[numBattery].pushOn(_dischargeI);
        }
        else if (pushType == PushType::None)
        {
            _batteryStatuses[numBattery].pushOff();
        }

#if defined(V1_PCB)
        numBattery = 2;
#else
        numBattery = 1;
#endif
        pushType = _buttonRStatus.getVal();
        if (pushType == PushType::PushLong || pushType == PushType::PushShort || pushType == PushType::Pushed)
        {
            _batteryStatuses[numBattery].pushOn(_dischargeI);
        }
        else if (pushType == PushType::None)
        {
            _batteryStatuses[numBattery].pushOff();
        }

#if defined(V1_PCB)
        numBattery = 1;
#else
        numBattery = 2;
#endif
        pushType = _buttonAStatus.getVal();
        if (pushType == PushType::PushLong || pushType == PushType::PushShort || pushType == PushType::Pushed)
        {
            _batteryStatuses[numBattery].pushOn(_dischargeI);
        }
        else if (pushType == PushType::None)
        {
            _batteryStatuses[numBattery].pushOff();
        }

        numBattery = 3;
        pushType = _buttonBStatus.getVal();
        if (pushType == PushType::PushLong || pushType == PushType::PushShort || pushType == PushType::Pushed)
        {
            _batteryStatuses[numBattery].pushOn(_dischargeI);
        }
        else if (pushType == PushType::None)
        {
            _batteryStatuses[numBattery].pushOff();
        }

        pushType = _buttonOnStatus.getVal();
        if (pushType == PushType::ReleaseShort)
        {
            nextMode = MainMode::DischargerMode;
        }
    }

    if ((_buttonUStatus.getVal() == PushType::Pushed || _buttonUStatus.getVal() == PushType::PushShort || _buttonUStatus.getVal() == PushType::PushLong)
        && (_buttonDStatus.getVal() == PushType::Pushed || _buttonDStatus.getVal() == PushType::PushShort || _buttonDStatus.getVal() == PushType::PushLong))
    {
        if (_mainMode == MainMode::DischargerMode || _mainMode == MainMode::PushDischargerMode)
        {
            writePinReset();
            _cachedMainMode = _mainMode;
            nextMode = MainMode::ConfigMode;
        }
    }

    if (_mainMode != nextMode)
    {
        drawAdafruit.clearDisplay();
    }

    _mainMode = nextMode;
}

void BatteryController::loopSub()
{
    ++_loopSubCount;
    if ((_loopSubCount % 60) == 0)
    {
        const float xiaoVolt{readAndDrawXiaoBattery()};

        if (_xiaoVoltFlag)
        {
            if (xiaoVolt < (VOLT3_3 + 0.2f))
            {
                _xiaoVoltFlag = false;
            }
        }
        else
        {
            if (xiaoVolt > (VOLT3_3 + 0.3f))
            {
                _xiaoVoltFlag = true;
            }
        }
    }

    if (!_xiaoVoltFlag)
    {
        drawAdafruit.display();
        return;
    }

    if (_ledOnFlag > 0)
    {
        digitalWrite(LED_BUILTIN, ((_loopSubCount / 12) % 2));
    }
    else
    {
        digitalWrite(LED_BUILTIN, 1);
    }

#ifdef SERIAL_DEBUG_ON
    if ((_loopSubCount % 12) == 0)
    {
        Serial.printf("serial print test. %d\n", _loopSubCount);
    }
#endif
    updateButtonStatus();

    if (_clearDisplayFlag)
    {
        setDisplayNone();
        drawAdafruit.display();
    }
    else
    {
        if (_mainMode == MainMode::DischargerMode)
        {
            for (auto &batteryStatus : _batteryStatuses)
            {
                batteryStatus.loopSubNormalDischarge();
            }

            if ((_loopSubCount % 3) == 0)
            {
                setDisplayData();
                drawAdafruit.display();
            }
        }
        else if (_mainMode == MainMode::BatteryConfigMode)
        {
            if ((_loopSubCount % 3) == 0)
            {
                setDisplayBatteryConfig();
                drawAdafruit.display();
            }
        }
        else if (_mainMode == MainMode::ConfigMode)
        {
            if ((_loopSubCount % 3) == 0)
            {
                setDisplayConfig();
                drawAdafruit.display();
            }
        }
        else if (_mainMode == MainMode::PushDischargerMode)
        {
            for (auto &batteryStatus : _batteryStatuses)
            {
                batteryStatus.loopSubPushDischarge();
            }

            if ((_loopSubCount % 3) == 0)
            {
                setDisplayPushDischarge();
                drawAdafruit.display();
            }
        }
    }
};
