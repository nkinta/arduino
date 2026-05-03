#include "battery_monitor.hpp"

#include "discharger_define.hpp"

void BatteryMonitor::setup()
{
    pinMode(XIAO_READ_BAT_SWITCH, OUTPUT);
    digitalWrite(XIAO_READ_BAT_SWITCH, HIGH);
    pinMode(XIAO_READ_BAT, INPUT);
}

bool BatteryMonitor::update()
{
    ++_batteryMonitorCount;
    if ((_batteryMonitorCount % BATTERY_MONITOR_INTERVAL_FRAMES) != 0)
    {
        return false;
    }

    _xiaoVolt = readXiaoBatteryVolt();
    if (_xiaoVoltValidFlag)
    {
        if (_xiaoVolt < XIAO_MIN_VOLT)
        {
            _xiaoVoltValidFlag = false;
            _lowBatteryDetectedMillis = millis();
        }
    }
    else if (_xiaoVolt > (XIAO_MIN_VOLT + 0.1f))
    {
        _xiaoVoltValidFlag = true;
        _lowBatteryDetectedMillis = 0;
    }

    return true;
}

bool BatteryMonitor::shouldGoDeepSleep() const
{
    if (!isLowBatteryActive())
    {
        return false;
    }

    if (_lowBatteryDetectedMillis == 0)
    {
        return false;
    }

    return (millis() - _lowBatteryDetectedMillis) >= LOW_BATTERY_SLEEP_DELAY_MS;
}

float BatteryMonitor::readXiaoBatteryVolt() const
{
    const int readValue{analogRead(XIAO_READ_BAT)};
    return (static_cast<float>(readValue) / 4096.f) * XIAO_BATTERY_DIVIDER_RATE * VOLT3_3;
}
