#include <SPI.h>
#include <Wire.h>
#include <vector>
#include <stdio.h>

#include <ArduinoLowPower.h>

#include "battery_controller.hpp"
#include "src/display/draw_adafruit.hpp"

#include "flappy.hpp"
#include "stopwatch.hpp"

DrawAdafruit drawAdafruit;

BatteryController controller;

ButtonStatus buttonONStatus{};

flappy::Game flappyGame;
stopwatch::Stopwatch stopWatch;

bool xiaoVoltValidFlag{true};
bool lowBatteryActive{false};

static constexpr unsigned long LOW_BATTERY_SLEEP_DELAY_MS{30000};
static constexpr unsigned long BATTERY_MONITOR_INTERVAL_FRAMES{60};
static constexpr uint8_t XIAO_READ_BAT{PD4};
static constexpr uint8_t XIAO_READ_BAT_SWITCH{PD3};
static constexpr float XIAO_BATTERY_DIVIDER_RATE{2.f};

unsigned long loopSubMillis{0};;
unsigned long batteryMonitorCount{BATTERY_MONITOR_INTERVAL_FRAMES - 1};
unsigned long lowBatteryDetectedMillis{0};

enum class StartupMode : uint8_t
{
  BatteryController,
  FlappyGame,
  Stopwatch,
};

StartupMode startupMode{StartupMode::BatteryController};

void goDeepSleep();

float readXiaoBatteryVolt()
{
  const int readValue{analogRead(XIAO_READ_BAT)};
  return (static_cast<float>(readValue) / 4096.f) * XIAO_BATTERY_DIVIDER_RATE * VOLT3_3;
}

void displayLowBattery()
{
  drawAdafruit.clearDisplay();
  drawAdafruit.drawStringC("Low Battery", 3);
  drawAdafruit.display();
}

void displayCurrentModeSleep()
{
  if (startupMode == StartupMode::Stopwatch)
  {
    stopWatch.displaySleep();
  }
  else if (startupMode == StartupMode::FlappyGame)
  {
    flappyGame.displaySleep();
  }
  else
  {
    controller.displaySleep();
  }
}

void updateBatteryMonitor()
{
  ++batteryMonitorCount;
  if ((batteryMonitorCount % BATTERY_MONITOR_INTERVAL_FRAMES) == 0)
  {
    const float xiaoVolt{readXiaoBatteryVolt()};

    if (startupMode == StartupMode::BatteryController)
    {
      controller.drawXiaoBattery(xiaoVolt);
    }

    if (xiaoVoltValidFlag)
    {
      if (xiaoVolt < XIAO_MIN_VOLT)
      {
        xiaoVoltValidFlag = false;
        lowBatteryDetectedMillis = millis();
      }
    }
    else if (xiaoVolt > (XIAO_MIN_VOLT + 0.1f))
    {
      xiaoVoltValidFlag = true;
      lowBatteryDetectedMillis = 0;
    }
  }

  lowBatteryActive = !xiaoVoltValidFlag;
  if (!lowBatteryActive)
  {
    return;
  }

  if (lowBatteryDetectedMillis == 0)
  {
    lowBatteryDetectedMillis = millis();
  }

  displayLowBattery();
  if ((millis() - lowBatteryDetectedMillis) >= LOW_BATTERY_SLEEP_DELAY_MS)
  {
    displayCurrentModeSleep();
    goDeepSleep();
  }
}

// the setup function runs once when you press reset or power the board
void setup()
{
  pinMode(LED_BUILTIN, OUTPUT);

#ifdef SERIAL_DEBUG_ON
  Serial.begin(115200);
  // while (!Serial);
  Serial.print("Start!");
#endif

  pinMode(PUSH_BUTTON_ON, INPUT_PULLUP);
  buttonONStatus.init(PUSH_BUTTON_ON);

  BatteryController::writePinReset();
  pinMode(XIAO_READ_BAT_SWITCH, OUTPUT);
  digitalWrite(XIAO_READ_BAT_SWITCH, HIGH);
  pinMode(XIAO_READ_BAT, INPUT);
  pinMode(PUSH_BUTTON_D, INPUT_PULLUP);
  pinMode(PUSH_BUTTON_U, INPUT_PULLUP);

  const bool flappyRequested{!digitalRead(PUSH_BUTTON_D)};
  const bool stopwatchRequested{!digitalRead(PUSH_BUTTON_U)};

  // 開始時にどのボタンを押しているかで、ゲームモード、ストップウォッチモードを起動するか決定する
  if (stopwatchRequested)
  {
    startupMode = StartupMode::Stopwatch;
  }
  else if (flappyRequested)
  {
    startupMode = StartupMode::FlappyGame;
  }
  else
  {
    startupMode = StartupMode::BatteryController;
  }

  if (startupMode == StartupMode::Stopwatch)
  {
    stopWatch.setup();
  }
  else if (startupMode == StartupMode::FlappyGame)
  {
    flappyGame.setup();
  }
  else
  {
    controller.setup();
  }

  loopSubMillis = millis();

}

void callback()
{
    int count{};
    count++;
}

void goDeepSleep()
{
    LowPower.attachInterruptWakeup(WAKE_UP_PIN, callback, RISING);

    BatteryController::writePinReset();

    pinMode(PD3, OUTPUT);
    pinMode(PB5, OUTPUT);
    pinMode(PB1, OUTPUT);
    pinMode(PB0, OUTPUT);

    digitalWrite(PD3, LOW); //VBAT
    digitalWrite(PB5, LOW); //RF_SW
    digitalWrite(PB1, LOW); //IMU
    digitalWrite(PB0, LOW); //MIC
    
    LowPower.deepSleep(365 * 24 * 3600 * 1000); // 7 * 24 * 3600 * 1000 // one week // 365 * 24 * 3600 * 1000
}

void loopSub()
{
  updateBatteryMonitor();
  buttonONStatus.update();

  if (buttonONStatus.getVal() == PushType::PushLong)
  {
    displayCurrentModeSleep();
  }
  else if (buttonONStatus.getVal() == PushType::ReleaseLong)
  {
      goDeepSleep();
  }
}

void loopWhile()
{
    const unsigned long tempMillis{millis()};
    if (tempMillis - loopSubMillis > ONE_FRAME_MS)
    {
        loopSub();
        loopSubMillis = tempMillis;
    }
};

// the loop function runs over and over again forever
void loop()
{
#if 0
  digitalWrite(LED_BUILTIN, HIGH); // turn the LED on (HIGH is the voltage level)
  delay(100);                      // wait for a second
  digitalWrite(LED_BUILTIN, LOW);  // turn the LED off by making the voltage LOW
  delay(100);    

  return;
#endif

  while (true)
  {
    loopWhile();

    if (lowBatteryActive)
    {
      continue;
    }

    if (startupMode == StartupMode::Stopwatch)
    {
      stopWatch.loop();
    }
    else if (startupMode == StartupMode::FlappyGame)
    {
      flappyGame.loop();
    }
    else
    {
      controller.loopWhile();
    }
  }

  // wait for a second
}
