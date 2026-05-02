#include "save_config_data.hpp"

#include <algorithm>
#include "src/display/draw_adafruit.hpp"


int SaveConfigData::voltClamp(int value)
{
return std::clamp(value, -1 * VOLT_RANGE, VOLT_RANGE);
};

void SaveConfigData::shiftParam(const ConfigSettingMode &configMode, int shift)
{
    if (configMode == ConfigSettingMode::tuneVolt00Setting)
    {
        _voltDatas[0] = SaveConfigData::voltClamp(_voltDatas[0] + shift);
    }
    else if (configMode == ConfigSettingMode::tuneVolt05Setting)
    {
        _voltDatas[1] = SaveConfigData::voltClamp(_voltDatas[1] + shift);
    }
    else if (configMode == ConfigSettingMode::tuneVolt10Setting)
    {
        _voltDatas[2] = SaveConfigData::voltClamp(_voltDatas[2] + shift);
    }
    else if (configMode == ConfigSettingMode::tuneVolt15Setting)
    {
        _voltDatas[3] = SaveConfigData::voltClamp(_voltDatas[3] + shift);
    }
    else if (configMode == ConfigSettingMode::tuneVolt20Setting)
    {
        _voltDatas[4] = SaveConfigData::voltClamp(_voltDatas[4] + shift);
    }
    else if (configMode == ConfigSettingMode::LedOnSetting)
    {
        _ledOnFlag = ((_ledOnFlag == 0) ? 1 : 0);
    }
    else if (configMode == ConfigSettingMode::discISetting)
    {
        _dischargeI = std::clamp(_dischargeI + (shift * 0.1f), 0.4f, 3.f);
    }
    else if (configMode == ConfigSettingMode::tuneISetting)
    {
        _calibI = std::clamp(_calibI + (shift * 0.1f), 0.8f, 1.2f);
    }
    else if (configMode == ConfigSettingMode::decimalSetting)
    {
        _decimal = std::clamp(_decimal + shift, 2, 3);
    }
};

void SaveConfigData::setDisplayConfig(DrawAdafruit &drawAdafruit, ConfigSettingMode settingMode) const
{
    std::vector<String> menuList{"0.0V", "0.5V", "1.0V", "1.5V", "2.0V", "LedOn", "DiscI", "AmpTune", "Decimal"};

    std::vector<String> valueList{
        String(_voltDatas[0]),
        String(_voltDatas[1]),
        String(_voltDatas[2]),
        String(_voltDatas[3]),
        String(_voltDatas[4]),
        String(_ledOnFlag == 0 ? false : true),
        String(_dischargeI),
        String(_calibI),
        String(_decimal),
    };

    DrawAdafruit::setDisplayTuneMenu(drawAdafruit, "Config", menuList, valueList, static_cast<int>(settingMode));
}
