#pragma once

#include <Arduino.h>

class BatteryMonitor
{
    static constexpr unsigned long LOW_BATTERY_SLEEP_DELAY_MS{30000};
    static constexpr unsigned long BATTERY_MONITOR_INTERVAL_FRAMES{60};
    static constexpr uint8_t XIAO_READ_BAT{PD4};
    static constexpr uint8_t XIAO_READ_BAT_SWITCH{PD3};
    static constexpr float XIAO_BATTERY_DIVIDER_RATE{2.f};

    unsigned long _batteryMonitorCount{BATTERY_MONITOR_INTERVAL_FRAMES - 1};
    unsigned long _lowBatteryDetectedMillis{0};

    float _xiaoVolt{0.f};

    bool _xiaoVoltValidFlag{true};

public:
    void setup();

    bool update();

    float xiaoVolt() const
    {
        return _xiaoVolt;
    }

    bool isLowBatteryActive() const
    {
        return !_xiaoVoltValidFlag;
    }

    bool shouldGoDeepSleep() const;

private:
    float readXiaoBatteryVolt() const;
};
