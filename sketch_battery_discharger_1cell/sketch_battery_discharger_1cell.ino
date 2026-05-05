#include <SPI.h>
#include <Wire.h>
#include <vector>
#include <stdio.h>

#include <ArduinoLowPower.h>

#include "battery_monitor.hpp"
#include "battery_controller.hpp"
#include "src/display/draw_adafruit.hpp"

#include "src/app/flappy.hpp"
#include "src/app/stopwatch.hpp"

Adafruit_SSD1306 oledDisplay{DrawAdafruit::SCREEN_WIDTH, DrawAdafruit::SCREEN_HEIGHT, &Wire, DrawAdafruit::OLED_RESET};

BatteryController controller;

ButtonStatus buttonONStatus{};
ButtonStatus buttonLStatus{};
ButtonStatus buttonRStatus{};

flappy::Game flappyGame;
stopwatch::Stopwatch stopWatch;
BatteryMonitor batteryMonitor;

unsigned long loopSubMillis{0};;
bool dumpDisplayButtonLock{false};
bool skipModeLoopThisFrame{false};

enum class StartupMode : uint8_t
{
  BatteryController,
  FlappyGame,
  Stopwatch,
};

StartupMode startupMode{StartupMode::BatteryController};

void goDeepSleep();
bool updateDisplayDumpRequest();

void displayLowBattery()
{
  oledDisplay.clearDisplay();
  DrawAdafruit::drawStringC(oledDisplay, "Low Battery", 3);
  oledDisplay.display();
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

// the setup function runs once when you press reset or power the board
void setup()
{
  pinMode(LED_BUILTIN, OUTPUT);

#ifdef SERIAL_DEBUG_ON
  Serial.begin(115200);
  while (!Serial);
  Serial.print("Start!");
#endif

  pinMode(PUSH_BUTTON_L, INPUT_PULLUP);
  pinMode(PUSH_BUTTON_R, INPUT_PULLUP);
  pinMode(PUSH_BUTTON_ON, INPUT_PULLUP);
  buttonLStatus.init(PUSH_BUTTON_L);
  buttonRStatus.init(PUSH_BUTTON_R);
  buttonONStatus.init(PUSH_BUTTON_ON);

  BatteryController::writePinReset();
  batteryMonitor.setup();
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

bool updateDisplayDumpRequest()
{
  const auto isActivePush = [](PushType pushType) {
    return pushType == PushType::Pushed || pushType == PushType::PushShort || pushType == PushType::PushLong;
  };

  buttonLStatus.update();
  buttonRStatus.update();

  const bool dumpDisplayRequested{isActivePush(buttonLStatus.getVal()) && isActivePush(buttonRStatus.getVal())};
  if (dumpDisplayRequested)
  {
    if (!dumpDisplayButtonLock)
    {
      DrawAdafruit::dumpDisplayAsPbm(oledDisplay, Serial);
      dumpDisplayButtonLock = true;
    }
    return true;
  }

  dumpDisplayButtonLock = false;
  return false;
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
#ifdef SERIAL_DEBUG_ON

  if (updateDisplayDumpRequest())
  {
    skipModeLoopThisFrame = true;
    return;
  }

  skipModeLoopThisFrame = false;
#endif

  if (batteryMonitor.update() && startupMode == StartupMode::BatteryController)
  {
    controller.drawXiaoBattery(batteryMonitor.xiaoVolt());
  }

  if (batteryMonitor.isLowBatteryActive())
  {
    displayLowBattery();
    if (batteryMonitor.shouldGoDeepSleep())
    {
      displayCurrentModeSleep();
      goDeepSleep();
    }
  }

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

  while (true)
  {
    loopWhile();

    if (skipModeLoopThisFrame)
    {
      continue;
    }

    if (batteryMonitor.isLowBatteryActive())
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
