#include "save_battery_config_data.hpp"
#include "src/display/draw_adafruit.hpp"
#include "battery_info.hpp"

void SaveBattery::setDisplayBatteryConfig(Adafruit_SSD1306 &display, int index, BatteryConfigSettingMode settingMode) const
{
    std::vector<String> menuList{"TargetV", "TargetI", "DiscMode", "ReduceI", "KeepMin"};

    const String &disChargeModeString{DISC_MODE_NAMES[static_cast<uint8_t>(_disChargeMode)]};
    const String &reduceModeString{REDUCE_MODE_NAMES[static_cast<uint8_t>(_reduceMode)]};

    std::vector<String> valueList{String(_targetV, 3), String(_targetI), disChargeModeString, reduceModeString, String(_holdMin)};

    String title{"Battery Pair."};
    title += String(index + 1);
    DrawAdafruit::setDisplayTuneMenu(display, std::move(title), menuList, valueList, static_cast<int>(settingMode));
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
    else if (settingMode == BatteryConfigSettingMode::ReduceModeChangeSetting)
    {
        const int nextModeIndex{(static_cast<int>(ReduceMode::Max) + static_cast<int>(_reduceMode) + shift) % static_cast<int>(ReduceMode::Max)};
        _reduceMode = static_cast<ReduceMode>(nextModeIndex);
    }
};
