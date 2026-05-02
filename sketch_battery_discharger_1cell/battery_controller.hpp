#pragma once

#include "discharger_define.hpp"
#include "battery_info.hpp"
#include "save_config_data.hpp"
#include "voltage_mapping.hpp"
#include "button_status.hpp"

static constexpr float FPS{30.f};
static constexpr float SEC{1000.f};
static constexpr float ONE_FRAME_MS{(1.f / FPS) * SEC};

enum class MainMode : uint8_t
{
    DischargerMode, // ノーマル放電モード（指定電圧放電モード）
    PushDischargerMode, // ボタン押す放電モード
    ConfigMode, // 設定モード（電圧値のキャリブレーション等）
    BatteryConfigMode, // ーマル放電モード時の設定モード
    Max,
};

enum class ConfigSettingMode : uint8_t
{
    tuneVolt00Setting, // 0.0V付近の電圧値のキャリブレーション
    tuneVolt05Setting, // 0.5V付近の電圧値のキャリブレーション
    tuneVolt10Setting, // 1.0V付近の電圧値のキャリブレーション
    tuneVolt15Setting, // 1.5V付近の電圧値のキャリブレーション
    tuneVolt20Setting, // 2.0V付近の電圧値のキャリブレーション
    LedOnSetting, // 放電機能が正常時にLEDをチカチカさせる
    discISetting, // 放電用電流
    tuneISetting, // 電流値のキャリブレーション
    decimalSetting, // 小数点何桁まで表示するか
    Max,
};

class BatteryController
{

    static constexpr uint8_t XIAO_READ_BAT{PD4};
    static constexpr uint8_t XIAO_READ_BAT_SWITCH{PD3};

private:
    ButtonStatus _buttonLStatus{};
    ButtonStatus _buttonRStatus{};
    ButtonStatus _buttonUStatus{};
    ButtonStatus _buttonDStatus{};
    ButtonStatus _buttonAStatus{};
    ButtonStatus _buttonBStatus{};
    ButtonStatus _buttonOnStatus{};

    std::vector<ButtonStatus*> _buttonStatuses{
        &_buttonLStatus,
        &_buttonRStatus,
        &_buttonUStatus,
        &_buttonDStatus,
        &_buttonAStatus,
        &_buttonBStatus,
        &_buttonOnStatus
    };

    std::vector<ButtonStatus*> _dischargeButtonStatuses;

    std::vector<BatteryInfo> _batteryStatuses{
        BatteryInfo{READ1_PIN, WRITE1_PIN, 0},
        BatteryInfo{READ2_PIN, WRITE2_PIN, 1},
        BatteryInfo{READ3_PIN, WRITE3_PIN, 2},
        BatteryInfo{READ4_PIN, WRITE4_PIN, 3}};

    ConfigSettingMode _configSettingMode{ConfigSettingMode::tuneVolt00Setting};
    BatteryConfigSettingMode _batteryConfigSettingMode{BatteryConfigSettingMode::DischargeVSetting};

    unsigned long _loopSubMillis{0};

    size_t _currentBatteryIndex{0};

    size_t _currentBatterySettingIndex{0};

    unsigned long _loopSubCount{0};

    size_t _batteryConfigNum{2};

    uint8_t _ledOnFlag{0};

    float _dischargeI{2.f};

    bool _xiaoVoltFlag{true};

    bool _clearDisplayFlag{false};

    SaveBatteryConfigData _saveBatteryConfigData{};

    SaveConfigData _saveConfigData{};

    MainMode _mainMode{MainMode::DischargerMode};

    MainMode _cachedMainMode{MainMode::DischargerMode};

public:
    BatteryController()
    {
        _batteryStatuses[0]._saveBattery = &(_saveBatteryConfigData._battery[0]);
        _batteryStatuses[1]._saveBattery = &(_saveBatteryConfigData._battery[0]);
        _batteryStatuses[2]._saveBattery = &(_saveBatteryConfigData._battery[1]);
        _batteryStatuses[3]._saveBattery = &(_saveBatteryConfigData._battery[1]);

        for (auto &batteryStatus : _batteryStatuses)
        {
            batteryStatus._batteryController = this;
        }

        _batteryStatuses[_currentBatteryIndex]._displayFlag = true;
    }

    float _calibI{1.f};

    int _decimal{3};

    VoltageMapping _voltageMapping;

private:
    float readAndDrawXiaoBattery();

    void saveConfig();

    void saveMain();

    void loadConfig();

    void loadMain();

    void clearEEPROM();

    void updateBatterySaveData();

    void updateConfigSaveData();

    void setDisplayConfig() const;

    void setDisplayBatteryConfig() const
    {
        _saveBatteryConfigData._battery[_currentBatterySettingIndex].setDisplayBatteryConfig(_currentBatterySettingIndex, _batteryConfigSettingMode);
    }

    void setDisplayPushDischarge() const;

    void setDisplayData() const;

    void setDisplayNone() const;

    // void goDeepSleep();

    void updateButtonStatus();

    void changeTargetBatterySetting(int shift)
    {
        _currentBatterySettingIndex = (_currentBatterySettingIndex + shift) % _batteryConfigNum;
    }

    void shiftTargetBattery(int shift);

    void changeTargetBattery(int batteryIndex);

    void changeSettingMode(int shift);

    void changeActive(int shift)
    {
        _batteryStatuses[_currentBatteryIndex].changeActive(shift);
    };

    void shiftParam(int shift);

    void loopSub();

    void loopMain()
    {
        for (auto &batteryStatus : _batteryStatuses)
        {
            batteryStatus.read();
        }
    };

public:
    void displaySleep();

    void clearDisplay()
    {
        _clearDisplayFlag = true;
    }

    void setup();

    static void writePinReset();

    void loopWhile()
    {

        loopMain();

        const unsigned long tempMillis{millis()};
        if (tempMillis - _loopSubMillis > ONE_FRAME_MS)
        {
            loopSub();
            _loopSubMillis = tempMillis;
        }
    };
};
