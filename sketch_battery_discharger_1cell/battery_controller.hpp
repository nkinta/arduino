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

class BatteryController
{

    static constexpr uint8_t XIAO_READ_BAT{PD4};
    static constexpr uint8_t XIAO_READ_BAT_SWITCH{PD3};

private:
    ButtonStatus buttonLStatus{};
    ButtonStatus buttonRStatus{};
    ButtonStatus buttonUStatus{};
    ButtonStatus buttonDStatus{};
    ButtonStatus buttonAStatus{};
    ButtonStatus buttonBStatus{};

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

    bool xiaoVoltFlag{true};

    bool clearDisplayFlag{false};

    SaveBatteryConfigData saveBatteryConfigData{};

    SaveConfigData saveConfigData{};

    MainMode mainMode{MainMode::DischargerMode};

    MainMode cachedMainMode{MainMode::DischargerMode};

public:
    BatteryController()
    {
        batteryStatuses[0].saveBattery = &(saveBatteryConfigData.battery[0]);
        batteryStatuses[1].saveBattery = &(saveBatteryConfigData.battery[0]);
        batteryStatuses[2].saveBattery = &(saveBatteryConfigData.battery[1]);
        batteryStatuses[3].saveBattery = &(saveBatteryConfigData.battery[1]);

        for (auto &batteryStatus : batteryStatuses)
        {
            batteryStatus.batteryController = this;
        }

        batteryStatuses[currentBatteryIndex].displayFlag = true;
    }

    float calibI{1.f};

    int decimal{3};

    VoltageMapping voltageMapping;

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
        saveBatteryConfigData.battery[currentBatterySettingIndex].setDisplayBatteryConfig(currentBatterySettingIndex, batteryConfigSettingMode);
    }

    void setDisplayPushDischarge() const;

    void setDisplayData() const;

    void setDisplayNone() const;

    // void goDeepSleep();

    void updateButtonStatus();

    void changeTargetBatterySetting(int shift)
    {
        currentBatterySettingIndex = (currentBatterySettingIndex + shift) % batteryConfigNum;
    }

    void shiftTargetBattery(int shift);

    void changeTargetBattery(int batteryIndex);

    void changeSettingMode(int shift);

    void changeActive(int shift)
    {
        batteryStatuses[currentBatteryIndex].changeActive(shift);
    };

    void shiftParam(int shift);

    void loopSub();

    void loopMain()
    {
        for (auto &batteryStatus : batteryStatuses)
        {
            batteryStatus.read();
        }
    };

public:
    void clearDisplay()
    {
        clearDisplayFlag = true;
    }

    void setup();

    static void writePinReset();

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
