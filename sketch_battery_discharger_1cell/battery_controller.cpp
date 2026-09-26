#include "battery_controller.hpp"

// #define DEEP_SLEEP_ESCAPE_PIN   D14
#include <EEPROM.h>
#include "src/display/adafruit_gfx_utility.hpp"

extern Adafruit_SSD1306 oledDisplay;

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
    _saveConfigData._id = SaveConfigData::SAVEDATA_ID;
    saveCustomData<SaveConfigData>((byte *)&_saveConfigData);
}

void BatteryController::saveMain()
{
    _saveBatteryConfigData._id = SaveBatteryConfigData::SAVEDATA_ID;
    saveCustomData<SaveBatteryConfigData>((byte *)&_saveBatteryConfigData);
}

void BatteryController::saveMeasurement()
{
    _saveMeasurementData._id = SaveMeasurementData::SAVEDATA_ID;
    saveCustomData<SaveMeasurementData>((byte *)&_saveMeasurementData);
}

void BatteryController::loadConfig()
{
    SaveConfigData tempData;
    loadCustomData<SaveConfigData>((byte *)&tempData);
    if (tempData._id != SaveConfigData::SAVEDATA_ID)
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
    if (tempData._id != SaveBatteryConfigData::SAVEDATA_ID)
    {
        return;
    }
    if (tempData._ver != _saveBatteryConfigData._ver)
    {
        return;
    }
    _saveBatteryConfigData = tempData;
};

void BatteryController::loadMeasurement()
{
    SaveMeasurementData tempData;
    loadCustomData<SaveMeasurementData>((byte *)&tempData);
    if (tempData._id != SaveMeasurementData::SAVEDATA_ID)
    {
        return;
    }
    if (tempData._ver != _saveMeasurementData._ver)
    {
        return;
    }
    _saveMeasurementData = tempData;
}

void BatteryController::clearEEPROM()
{
    const uint8_t clearSize{256};
    for (int i = 0; i < clearSize; ++i)
    {
        EEPROM.write(i, 0xFF);
    }
}

void BatteryController::setup()
{
    AdafruitGfxUtility::setupDisplay(oledDisplay);

    digitalWrite(PA6, HIGH); // FLASH

    // MemReset
    pinMode(MEM_RESET_PIN, INPUT_PULLUP);
    int val{HIGH};
    val = digitalRead(MEM_RESET_PIN);
    if (val != LOW)
    {
        loadMain();
        loadConfig();
        loadMeasurement();
    }
    else
    {
        saveMain();
        saveConfig();
        saveMeasurement();
    }

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

    // PushDischargeのボタン割り当て初期化
    static const std::vector<int> dischargeButtonIndices{PUSH_DISCHARGE_NO1, PUSH_DISCHARGE_NO2, PUSH_DISCHARGE_NO3, PUSH_DISCHARGE_NO4};

    _dischargeButtonStatuses.resize(dischargeButtonIndices.size(), nullptr);
    for (int index = 0; index < dischargeButtonIndices.size(); ++index)
    {
        for (ButtonStatus* buttonStatus: _buttonStatuses)
        {
            if (buttonStatus->_pinId == dischargeButtonIndices[index])
            {
                _dischargeButtonStatuses[index] = buttonStatus;
                break;
            }
        }
    }

    updateBatterySaveData();
    updateConfigSaveData();
    updateMeasurementSaveData();
};

void BatteryController::updateConfigSaveData()
{

    const static SaveConfigData defaultSaveConfigData{};
    if (_saveConfigData._id != SaveConfigData::SAVEDATA_ID)
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
    _idleSleepMin = _saveConfigData._idleSleepMin;
}

void BatteryController::updateMeasurementSaveData()
{
    const static SaveMeasurementData defaultSaveMeasurementData{};
    if (_saveMeasurementData._id != SaveMeasurementData::SAVEDATA_ID)
    {
        _saveMeasurementData = defaultSaveMeasurementData;
    }

    _measurement.discSeconds = _saveMeasurementData._seconds;
    _measurement.restSeconds = _saveMeasurementData._restSeconds;
    _measurement.current = _saveMeasurementData._current;
}

void BatteryController::updateBatterySaveData()
{
    for (int i{0}; i < _batteryStatuses.size(); ++i)
    {
        auto &batteryStatus{_batteryStatuses[i]};

        const static SaveBattery defaultSaveBattery{};
        auto *saveBattery{batteryStatus._saveBattery};
        if (_saveBatteryConfigData._id != SaveBatteryConfigData::SAVEDATA_ID)
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

void BatteryController::drawXiaoBattery(float xiaoVolt) const
{
    uint8_t index{0};
    if (xiaoVolt > XIAO_FULL_VOLT)
    {
      index = 3;
    }
    else if (xiaoVolt > XIAO_LEVEL2_VOLT)
    {
      index = 2;
    }
    else if (xiaoVolt > XIAO_MIN_VOLT)
    {
      index = 1;
    }
    else
    {
      index = 0;
    }

    AdafruitGfxUtility::drawBat(oledDisplay, index);
}

void BatteryController::drawXiaoBatteryVolt(float xiaoVolt) const
{
    oledDisplay.fillRect(96, 54, 32, 10, BLACK);
    AdafruitGfxUtility::drawFloatR(oledDisplay, xiaoVolt, 20, 6, 4, 2);
    AdafruitGfxUtility::drawString(oledDisplay, "V", 20, 6);
}

void BatteryController::setDisplayConfig() const
{
    _saveConfigData.setDisplayConfig(oledDisplay, _configSettingMode);
}

void BatteryController::setDisplayPushDischarge() const
{
    for (int i = 0; i < 6; ++i)
    {
        AdafruitGfxUtility::drawFillLine(oledDisplay, i);
    }
    AdafruitGfxUtility::drawStringC(oledDisplay, String("Discharge ") + String(_dischargeI) + String("A"), 0);
    for (auto &batteryStatus : _batteryStatuses)
    {
        batteryStatus.setDisplayPushData(oledDisplay);
    }

    int line{4};
    int virOffset{0};
    if (0)
    {
        float lV{_batteryStatuses[0]._v + _batteryStatuses[1]._v};
        float rV{_batteryStatuses[2]._v + _batteryStatuses[3]._v};

        virOffset += 10;
        AdafruitGfxUtility::drawFloatR(oledDisplay, lV, virOffset, line, 4, _saveConfigData._decimal);
        AdafruitGfxUtility::drawString(oledDisplay, "V", virOffset, line);
        virOffset += 10;
        AdafruitGfxUtility::drawFloatR(oledDisplay, rV, virOffset, line, 4, _saveConfigData._decimal);
        AdafruitGfxUtility::drawString(oledDisplay, "V", virOffset, line);
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
        AdafruitGfxUtility::drawFillR(oledDisplay, virOffset, line, 6);
        AdafruitGfxUtility::drawFloatR(oledDisplay, targetBatteryStatus->_ohm, virOffset, line, 4, 1);
        static constexpr char CHAR_DATA_OHM[] = {0x6D, 0xe9, 0x00};
        AdafruitGfxUtility::drawChar(oledDisplay, &CHAR_DATA_OHM[0], virOffset, line);
    }
}

void BatteryController::setDisplayNone() const
{
    oledDisplay.clearDisplay();
}

void BatteryController::setDisplayMeasurement() const
{
    oledDisplay.clearDisplay();
    if (_measurement.state == MeasurementState::Setting)
    {
        AdafruitGfxUtility::drawStringC(oledDisplay, "Measure", 0);

        if (_measurement.pair == 0)
        {
            AdafruitGfxUtility::drawStringC(oledDisplay,
                String(">") + String(_batteryStatuses[0]._v, 2) + String(">") + String(_batteryStatuses[1]._v, 2)
                + String(" ") + String(_batteryStatuses[2]._v, 2) + String(" ") + String(_batteryStatuses[3]._v, 2)
                , 1);
        }
        else
        {
            AdafruitGfxUtility::drawStringC(oledDisplay,
                String(" ") + String(_batteryStatuses[0]._v, 2) + String(" ") + String(_batteryStatuses[1]._v, 2)
                + String(">") + String(_batteryStatuses[2]._v, 2) + String(">") + String(_batteryStatuses[3]._v, 2)
                , 1);
        }

        AdafruitGfxUtility::drawString(oledDisplay, String("Current ") + String(_measurement.current, 1) + String("A"), 1, 3);
        AdafruitGfxUtility::drawString(oledDisplay, String("Disc ") + String(_measurement.discSeconds) + String("s"), 1, 4);
        AdafruitGfxUtility::drawString(oledDisplay, String("Rest ") + String(_measurement.restSeconds) + String("s"), 11, 4);

        return;
    }

    if (_measurement.state == MeasurementState::Editing)
    {
        std::vector<String> menuList{"Current", "DiscSec", "RestSec"};
        std::vector<String> valueList{
            String(_measurement.current, 1) + String("A"),
            String(_measurement.discSeconds) + String("s"),
            String(_measurement.restSeconds) + String("s"),
        };
        AdafruitGfxUtility::setDisplayTuneMenu(oledDisplay, "Measure Setup", menuList, valueList, static_cast<int>(_measurement.setting));

        return;
    }

    if (_measurement.state == MeasurementState::Running || _measurement.state == MeasurementState::Resting)
    {
        const bool running{_measurement.state == MeasurementState::Running};
        const unsigned long startMillis{running ? _measurement.startMillis : _measurement.restStartMillis};
        const unsigned long elapsed{(millis() - startMillis) / 1000UL};
        const unsigned long duration{running ? _measurement.discSeconds : _measurement.restSeconds};
        const String stateName{running ? String("Run") : String("Rest")};

        AdafruitGfxUtility::drawStringC(oledDisplay, stateName + String(" ") + String(elapsed) + String("/") + String(duration) + String("s"), 0);

        // AdafruitGfxUtility::drawStringC(oledDisplay, String(_measurement.result[0].preDischargeVolt, 3) + String("V ") + String(_measurement.result[1].preDischargeVolt, 3) + String("V"), 1);
        for (int index = 0; index < 2; ++index)
        {
            int line{3 * index + 1};
            const int batteryIndex{static_cast<int>(_measurement.pair * 2 + index)};
            AdafruitGfxUtility::drawString(oledDisplay, String("B") + String(batteryIndex + 1), 0, line);

            AdafruitGfxUtility::drawFloatR(oledDisplay, _measurement.result[batteryIndex].preDischargeVolt, 9, line, 4, 3);
            AdafruitGfxUtility::drawString(oledDisplay, "V", 9, line);

            AdafruitGfxUtility::drawFloatR(oledDisplay, _batteryStatuses[batteryIndex]._v, 16, line, 4, 3);
            AdafruitGfxUtility::drawString(oledDisplay, "V", 16, line);

            AdafruitGfxUtility::drawFloatR(oledDisplay, _measurement.result[index].milliWattHour, 9, line + 1, 5, 1);
            AdafruitGfxUtility::drawString(oledDisplay, "mWh", 9, line + 1);
        }
        return;
    }

    /*
    if (_measurement.state == MeasurementState::Resting)
    {
        if (!_measurement.postVoltageCaptured)
        {
            AdafruitGfxUtility::drawStringC(oledDisplay, "Wait 1s", 0);
        }
        else
        {
            const unsigned long elapsed{(millis() - _measurement.restStartMillis) / 1000UL};
            AdafruitGfxUtility::drawStringC(oledDisplay, String("Rest ") + String(elapsed) + String("/") + String(_measurement.restSeconds) + String("s"), 0);
        }
        for (int index = 0; index < 2; ++index)
        {
            int line{2 * index + 1};
            const int batteryIndex{static_cast<int>(_measurement.pair * 2 + index)};
            AdafruitGfxUtility::drawString(oledDisplay, String("B") + String(batteryIndex + 1), 0, line);
            AdafruitGfxUtility::drawFloatR(oledDisplay, _batteryStatuses[batteryIndex]._v, 9, line, 4, 3);
            AdafruitGfxUtility::drawString(oledDisplay, "V", 9, line);
        }
        return;
    }
    */

    if (_measurement.resultPage == 0)
    {
        AdafruitGfxUtility::drawStringC(oledDisplay, "Result", 0);

        for (int index = 0; index < 2; ++index)
        {
            int line{3 * index + 1};
            AdafruitGfxUtility::drawString(oledDisplay, String("B") + String(_measurement.pair * 2 + index + 1), 0, line);

            AdafruitGfxUtility::drawFloatR(oledDisplay, _measurement.result[index].preDischargeVolt, 8, line, 4, 3);
            AdafruitGfxUtility::drawString(oledDisplay, "V", 8, line);

            AdafruitGfxUtility::drawChar(oledDisplay, DisplayConst::CHAR_DATA_ARROW_NEW, 10, line);

            AdafruitGfxUtility::drawFloatR(oledDisplay, _measurement.result[index].postDischargeVolt, 17, line, 4, 3);
            AdafruitGfxUtility::drawString(oledDisplay, "V", 17, line);

            AdafruitGfxUtility::drawFloatR(oledDisplay, _measurement.result[index].postRestVoltage, 8, line + 1, 4, 3);
            AdafruitGfxUtility::drawString(oledDisplay, "V", 8, line + 1);

            AdafruitGfxUtility::drawFloatR(oledDisplay, _measurement.result[index].milliWattHour, 21, line + 1, 5, 1);

        }
    }
    else
    {
        const bool restGraph{_measurement.resultPage >= 3};
        const int graphBattery{restGraph ? _measurement.resultPage - 3 : _measurement.resultPage - 1};
        const int batteryIndex{static_cast<int>(_measurement.pair * 2 + graphBattery)};
        const float *voltageData{restGraph ? _measurement.result[graphBattery].restVoltage : _measurement.result[graphBattery].dischargeVoltage};
        const int sampleCount{restGraph ? _measurement.restSampleCount : _measurement.sampleCount};
        AdafruitGfxUtility::drawStringC(oledDisplay, String(restGraph ? "Rest B" : "Discharge B") + String(batteryIndex + 1), 0);
        constexpr int GRAPH_LEFT{14};
        constexpr int GRAPH_RIGHT{126};
        constexpr int GRAPH_TOP{10};
        constexpr int GRAPH_BOTTOM{55};

        float voltageAverage{0.f};
        if (sampleCount > 0)
        {
            for (int sample = 0; sample < sampleCount; ++sample)
            {
                voltageAverage += voltageData[sample];
            }
            voltageAverage /= sampleCount;
        }
        const float graphMinVolt{voltageAverage - 0.05f};
        const float graphMaxVolt{voltageAverage + 0.05f};

        oledDisplay.drawLine(GRAPH_LEFT, GRAPH_TOP, GRAPH_LEFT, GRAPH_BOTTOM, WHITE);
        oledDisplay.drawLine(GRAPH_LEFT, GRAPH_BOTTOM, GRAPH_RIGHT, GRAPH_BOTTOM, WHITE);
        AdafruitGfxUtility::drawFloatR(oledDisplay, graphMaxVolt, 13, 1, 4, 2);
        AdafruitGfxUtility::drawFloatR(oledDisplay, graphMinVolt, 13, 6, 4, 2);

        if (sampleCount > 0)
        {
            int previousX{GRAPH_LEFT};
            const float firstVoltage{constrain(voltageData[0], graphMinVolt, graphMaxVolt)};
            int previousY{GRAPH_BOTTOM - static_cast<int>((firstVoltage - graphMinVolt) * (GRAPH_BOTTOM - GRAPH_TOP) / (graphMaxVolt - graphMinVolt))};
            for (int sample = 1; sample < sampleCount; ++sample)
            {
                const int x{GRAPH_LEFT + (sample * (GRAPH_RIGHT - GRAPH_LEFT)) / (sampleCount - 1)};
                const float voltage{constrain(voltageData[sample], graphMinVolt, graphMaxVolt)};
                const int y{GRAPH_BOTTOM - static_cast<int>((voltage - graphMinVolt) * (GRAPH_BOTTOM - GRAPH_TOP) / (graphMaxVolt - graphMinVolt))};
                oledDisplay.drawLine(previousX, previousY, x, y, WHITE);
                previousX = x;
                previousY = y;
            }
        }
    }
}

void BatteryController::startMeasurement()
{
    _measurement.state = MeasurementState::Running;
    _measurement.startMillis = millis();
    _measurement.lastSampleMillis = _measurement.startMillis;
    _measurement.sampleCount = 0;
    _measurement.restSampleCount = 0;
    for (int index = 0; index < 2; ++index)
    {
        _measurement.restVoltageSum[index] = 0.f;
        _measurement.restVoltageSampleCount[index] = 0;
    }
    for (int index = 0; index < 2; ++index)
    {
        _measurement.dischargeVoltageSum[index] = 0.f;
        _measurement.dischargeVoltageSampleCount[index] = 0;
    }
    for (int index = 0; index < 2; ++index)
    {
        BatteryInfo &battery{_batteryStatuses[_measurement.pair * 2 + index]};
        _measurement.result[index] = MeasurementResultData{};
        _measurement.result[index].preDischargeVolt = battery._v;
        battery.reset();
        battery._activeFlag = true;
        battery.pushOn(_measurement.current);
    }
}

void BatteryController::updateMeasurement()
{
    const unsigned long now{millis()};
    if (_measurement.state == MeasurementState::Resting)
    {
        if (!_measurement.postVoltageCaptured)
        {
            if (now - _measurement.restStartMillis < 1000UL)
            {
                return;
            }
            for (int index = 0; index < 2; ++index)
            {
                _measurement.result[index].postDischargeVolt = _batteryStatuses[_measurement.pair * 2 + index]._v;
            }
            _measurement.postVoltageCaptured = true;
            _measurement.restStartMillis = now;
            _measurement.restLastSampleMillis = now;
            for (int index = 0; index < 2; ++index)
            {
                _measurement.restVoltageSum[index] = 0.f;
                _measurement.restVoltageSampleCount[index] = 0;
            }
            return;
        }
        for (int index = 0; index < 2; ++index)
        {
            const int batteryIndex{static_cast<int>(_measurement.pair * 2 + index)};
            _measurement.restVoltageSum[index] += _batteryStatuses[batteryIndex]._v;
            ++_measurement.restVoltageSampleCount[index];
        }
        while (now - _measurement.restLastSampleMillis >= 1000UL && _measurement.restSampleCount < 120)
        {
            _measurement.restLastSampleMillis += 1000UL;
            const int sample{_measurement.restSampleCount++};
            for (int index = 0; index < 2; ++index)
            {
                _measurement.result[index].restVoltage[sample] = _measurement.restVoltageSampleCount[index] > 0
                    ? _measurement.restVoltageSum[index] / _measurement.restVoltageSampleCount[index]
                    : _batteryStatuses[_measurement.pair * 2 + index]._v;
                _measurement.restVoltageSum[index] = 0.f;
                _measurement.restVoltageSampleCount[index] = 0;
            }
        }
        if (now - _measurement.restStartMillis >= static_cast<unsigned long>(_measurement.restSeconds) * 1000UL)
        {
            for (int index = 0; index < 2; ++index)
            {
                _measurement.result[index].postRestVoltage = _batteryStatuses[_measurement.pair * 2 + index]._v;
            }
            _measurement.state = MeasurementState::Result;
            _measurement.resultPage = 0;
        }
        return;
    }
    if (_measurement.state != MeasurementState::Running)
    {
        return;
    }
    for (int index = 0; index < 2; ++index)
    {
        const int batteryIndex{static_cast<int>(_measurement.pair * 2 + index)};
        _measurement.dischargeVoltageSum[index] += _batteryStatuses[batteryIndex]._v;
        ++_measurement.dischargeVoltageSampleCount[index];
    }
    while (now - _measurement.lastSampleMillis >= 1000UL && _measurement.sampleCount < 120)
    {
        _measurement.lastSampleMillis += 1000UL;
        const int sample{_measurement.sampleCount++};
        for (int index = 0; index < 2; ++index)
        {
            const float voltageAverage{_measurement.dischargeVoltageSampleCount[index] > 0
                ? _measurement.dischargeVoltageSum[index] / _measurement.dischargeVoltageSampleCount[index]
                : _batteryStatuses[_measurement.pair * 2 + index]._v};
            _measurement.result[index].dischargeVoltage[sample] = voltageAverage;
            _measurement.result[index].milliWattHour += voltageAverage * _measurement.current * (1000.f / 3600.f);
            _measurement.dischargeVoltageSum[index] = 0.f;
            _measurement.dischargeVoltageSampleCount[index] = 0;
        }
    }
    if (now - _measurement.startMillis >= static_cast<unsigned long>(_measurement.discSeconds) * 1000UL)
    {
        for (int index = 0; index < 2; ++index)
        {
            _batteryStatuses[_measurement.pair * 2 + index].pushOff();
            _batteryStatuses[_measurement.pair * 2 + index]._activeFlag = false;
        }
        _measurement.restStartMillis = now;
        _measurement.restLastSampleMillis = now;
        _measurement.postVoltageCaptured = false;
        _measurement.state = MeasurementState::Resting;
    }
}

void BatteryController::shiftMeasurementSetting(int shift)
{
    const int count{static_cast<int>(MeasurementSetting::Max)};
    const int value{(static_cast<int>(_measurement.setting) + shift + count) % count};
    _measurement.setting = static_cast<MeasurementSetting>(value);
}

void BatteryController::shiftMeasurementValue(int shift)
{
    if (_measurement.setting == MeasurementSetting::Time)
    {
        _measurement.discSeconds = constrain(static_cast<int>(_measurement.discSeconds) + shift * 10, 10, 120);
    }
    else if (_measurement.setting == MeasurementSetting::Current)
    {
        _measurement.current = constrain(_measurement.current + shift * 0.1f, 1.f, 3.f);
    }
    else if (_measurement.setting == MeasurementSetting::RestTime)
    {
        _measurement.restSeconds = constrain(static_cast<int>(_measurement.restSeconds) + shift * 10, 10, 120);
    }
}

void BatteryController::shiftMeasurementPair(int shift)
{
    _measurement.pair = (_measurement.pair + 2 + shift) % 2;
}

void BatteryController::setDisplayData() const
{
    AdafruitGfxUtility::drawFillLine(oledDisplay, 0);
    for (auto &batteryStatus : _batteryStatuses)
    {
        batteryStatus.setDisplayData(oledDisplay);
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
    oledDisplay.clearDisplay();
    oledDisplay.display();
    AdafruitGfxUtility::displaySleep(oledDisplay);
}

void BatteryController::shiftTargetBattery(int shift)
{
    const int count{static_cast<int>(_batteryStatuses.size())};
    const int currentIndex{static_cast<int>(_currentBatteryIndex)};
    _currentBatteryIndex = static_cast<size_t>((count + currentIndex + shift) % count);

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
        _saveConfigData.shiftParam(_configSettingMode, shift);
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

bool BatteryController::isAnyButtonActive() const
{
    for (const ButtonStatus* buttonStatus : _buttonStatuses)
    {
        if (buttonStatus->getVal() != PushType::None)
        {
            return true;
        }
    }

    return false;
}

bool BatteryController::isDischarging() const
{
    for (const auto &batteryStatus : _batteryStatuses)
    {
        if (batteryStatus.isDischarging())
        {
            return true;
        }
    }

    return false;
}

void BatteryController::updateIdleSleepRequest()
{
    if (_idleSleepMin == 0)
    {
        _idleStartMillis = millis();
        _idleSleepRequested = false;
        return;
    }

    if (isDischarging() || isAnyButtonActive())
    {
        _idleStartMillis = millis();
        _idleSleepRequested = false;
        return;
    }

    const unsigned long tempMillis{millis()};
    if (_idleStartMillis == 0)
    {
        _idleStartMillis = tempMillis;
        return;
    }

    const unsigned long idleSleepDelayMs{static_cast<unsigned long>(_idleSleepMin) * 60UL * 1000UL};
    if ((tempMillis - _idleStartMillis) >= idleSleepDelayMs)
    {
        _idleSleepRequested = true;
    }
}

void BatteryController::updateButtonStatus()
{
    for (ButtonStatus* buttonStatus :_buttonStatuses)
    {
        buttonStatus->update();
    }

    MainMode nextMode{_mainMode};
    if (_mainMode == MainMode::DischargerMode)
    {
        PushType pushType{0};
        const bool leftActive{_buttonLStatus.getVal() == PushType::Pushed || _buttonLStatus.getVal() == PushType::PushShort || _buttonLStatus.getVal() == PushType::PushLong};
        const bool rightActive{_buttonRStatus.getVal() == PushType::Pushed || _buttonRStatus.getVal() == PushType::PushShort || _buttonRStatus.getVal() == PushType::PushLong};
        if (leftActive && rightActive)
        {
            _measurement.state = MeasurementState::Setting;
            nextMode = MainMode::MeasurementMode;
        }
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
    else if (_mainMode == MainMode::MeasurementMode)
    {
        PushType pushType{0};
        if (_measurement.state == MeasurementState::Setting)
        {
            pushType = _buttonLStatus.getVal();
            if (pushType == PushType::ReleaseShort || pushType == PushType::PushLong)
            {
                shiftMeasurementPair(-1);
            }
            pushType = _buttonRStatus.getVal();
            if (pushType == PushType::ReleaseShort || pushType == PushType::PushLong)
            {
                shiftMeasurementPair(1);
            }
            pushType = _buttonAStatus.getVal();
            if (pushType == PushType::ReleaseShort)
            {
                startMeasurement();
            }
            pushType = _buttonBStatus.getVal();
            if (pushType == PushType::ReleaseShort)
            {
                nextMode = MainMode::DischargerMode;
            }
        }
        else if (_measurement.state == MeasurementState::Editing)
        {
            pushType = _buttonUStatus.getVal();
            if (pushType == PushType::ReleaseShort)
            {
                shiftMeasurementSetting(-1);
            }
            pushType = _buttonDStatus.getVal();
            if (pushType == PushType::ReleaseShort)
            {
                shiftMeasurementSetting(1);
            }
            pushType = _buttonLStatus.getVal();
            if (pushType == PushType::ReleaseShort || pushType == PushType::PushLong)
            {
                shiftMeasurementValue(-1);
            }
            pushType = _buttonRStatus.getVal();
            if (pushType == PushType::ReleaseShort || pushType == PushType::PushLong)
            {
                shiftMeasurementValue(1);
            }
            pushType = _buttonBStatus.getVal();
            if (pushType == PushType::ReleaseShort)
            {
                _saveMeasurementData._seconds = _measurement.discSeconds;
                _saveMeasurementData._restSeconds = _measurement.restSeconds;
                _saveMeasurementData._current = _measurement.current;
                saveMeasurement();
                _measurement.state = MeasurementState::Setting;
            }
        }
        else if (_measurement.state == MeasurementState::Running)
        {
            pushType = _buttonAStatus.getVal();
            if (pushType == PushType::ReleaseShort)
            {
                for (int index = 0; index < 2; ++index)
                {
                    _batteryStatuses[_measurement.pair * 2 + index].pushOff();
                    _batteryStatuses[_measurement.pair * 2 + index]._activeFlag = false;
                }
                _measurement.restStartMillis = millis();
                _measurement.restLastSampleMillis = _measurement.restStartMillis;
                _measurement.postVoltageCaptured = false;
                _measurement.state = MeasurementState::Resting;
            }
        }
        else if (_measurement.state == MeasurementState::Resting)
        {
            pushType = _buttonAStatus.getVal();
            if (pushType == PushType::ReleaseShort)
            {
                for (int index = 0; index < 2; ++index)
                {
                    _measurement.result[index].postRestVoltage = _batteryStatuses[_measurement.pair * 2 + index]._v;
                }
                _measurement.state = MeasurementState::Result;
                _measurement.resultPage = 0;
            }
        }
        else if (_measurement.state == MeasurementState::Result)
        {
            pushType = _buttonUStatus.getVal();
            if (pushType == PushType::ReleaseShort)
            {
                _measurement.resultPage = static_cast<uint8_t>((_measurement.resultPage + 4) % 5);
            }
            pushType = _buttonDStatus.getVal();
            if (pushType == PushType::ReleaseShort)
            {
                _measurement.resultPage = static_cast<uint8_t>((_measurement.resultPage + 1) % 5);
            }
            pushType = _buttonAStatus.getVal();
            if (pushType == PushType::ReleaseShort)
            {
                nextMode = MainMode::DischargerMode;
            }
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

        for (int buttonIndex = 0; buttonIndex < _dischargeButtonStatuses.size(); ++buttonIndex)
        {
            ButtonStatus* butonStatus = _dischargeButtonStatuses[buttonIndex];
            if (butonStatus == nullptr)
            {
                continue;
            }
            PushType pushType{butonStatus->getVal()};
            if (pushType == PushType::PushLong || pushType == PushType::PushShort || pushType == PushType::Pushed)
            {
                _batteryStatuses[buttonIndex].pushOn(_dischargeI);
            }
            else if (pushType == PushType::None)
            {
                _batteryStatuses[buttonIndex].pushOff();
            }
        }

        PushType pushType{0};
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
        else if (_mainMode == MainMode::MeasurementMode && _measurement.state == MeasurementState::Setting)
        {
            _measurement.state = MeasurementState::Editing;
            _measurement.setting = MeasurementSetting::Time;
        }
    }

    if (_mainMode != nextMode)
    {
        oledDisplay.clearDisplay();
    }

    _mainMode = nextMode;
}

void BatteryController::setDisplayBatteryConfig(Adafruit_SSD1306& display) const
{
    _saveBatteryConfigData._battery[_currentBatterySettingIndex].setDisplayBatteryConfig(display, _currentBatterySettingIndex, _batteryConfigSettingMode);
}

void BatteryController::loopSub()
{
    ++_loopSubCount;

    if (_ledOnFlag > 0)
    {
        digitalWrite(LED_BUILTIN, ((_loopSubCount / 12) % 2));
    }
    else
    {
        digitalWrite(LED_BUILTIN, 1);
    }

#ifdef SERIAL_DEBUG_ON  
    /*
    if ((_loopSubCount % 12) == 0)
    {
        Serial.printf("serial print test. %d\n", _loopSubCount);
    }
    */
#endif
    updateButtonStatus();

    if (_clearDisplayFlag)
    {
        setDisplayNone();
        oledDisplay.display();
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
                oledDisplay.display();
            }
        }
        else if (_mainMode == MainMode::BatteryConfigMode)
        {
            if ((_loopSubCount % 3) == 0)
            {
                setDisplayBatteryConfig(oledDisplay);
                oledDisplay.display();
            }
        }
        else if (_mainMode == MainMode::ConfigMode)
        {
            if ((_loopSubCount % 3) == 0)
            {
                setDisplayConfig();
                oledDisplay.display();
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
                oledDisplay.display();
            }
        }
        else if (_mainMode == MainMode::MeasurementMode)
        {
            for (size_t index = 0; index < _batteryStatuses.size(); ++index)
            {
            if (index == _measurement.pair * 2 || index == _measurement.pair * 2 + 1)
                {
                    _batteryStatuses[index].loopSubPushDischarge();
                }
                else
                {
                    _batteryStatuses[index].read();
                }
            }
            updateMeasurement();
            if ((_loopSubCount % 3) == 0)
            {
                setDisplayMeasurement();
                oledDisplay.display();
            }
        }
    }

    updateIdleSleepRequest();
};
