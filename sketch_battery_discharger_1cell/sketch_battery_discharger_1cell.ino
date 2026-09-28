#include <SPI.h>
#include <Wire.h>
#include <vector>
#include <stdio.h>

#include <ArduinoLowPower.h>

#include "battery_monitor.hpp"
#include "battery_controller.hpp"
#include "src/display/adafruit_gfx_utility.hpp"

#include "src/app/flappy.hpp"
#include "src/app/stopwatch.hpp"

Adafruit_SSD1306 oledDisplay{AdafruitGfxUtility::SCREEN_WIDTH, AdafruitGfxUtility::SCREEN_HEIGHT, &Wire, AdafruitGfxUtility::OLED_RESET};

BatteryController controller;

ButtonStatus buttonONStatus{};
ButtonStatus buttonLStatus{};
ButtonStatus buttonRStatus{};
ButtonStatus buttonUStatus{};
ButtonStatus buttonDStatus{};
ButtonStatus buttonAStatus{};

flappy::Game flappyGame;
stopwatch::Stopwatch stopWatch;
BatteryMonitor batteryMonitor;

unsigned long loopSubMillis{0};;
bool dumpDisplayButtonLock{false};
bool skipModeLoopThisFrame{false};

enum class StartupMode : uint8_t
{
  Menu,
  BatteryController,
  FlappyGame,
  Stopwatch,
};

enum class StartupMenuItem : uint8_t
{
  Controller,
  ControllerMeasurement,
  Stopwatch,
  FlappyGame,
  None,
  MemoryReset,
  Max,
};

StartupMode startupMode{StartupMode::Menu};
StartupMenuItem startupMenuItem{StartupMenuItem::Controller};

void goDeepSleep();
bool updateDisplayDumpRequest();
void drawStartupMenu();
void updateStartupMenu();
void startSelectedMode();

void displayLowBattery()
{
  oledDisplay.clearDisplay();
  AdafruitGfxUtility::drawStringC(oledDisplay, "Low Battery", 3);
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
  else if (startupMode == StartupMode::BatteryController)
  {
    controller.displaySleep();
  }
  else
  {
    AdafruitGfxUtility::displaySleep(oledDisplay);
  }
}

void drawStartupMenu()
{
  static const char * const menuItems[]{
    "Discharge",
    "Measure",
    "Stopwatch",
    "Game",
    "",
    "MemoryReset",
  };

  oledDisplay.clearDisplay();
  AdafruitGfxUtility::drawStringC(oledDisplay, "< Menu >", 0);
  for (uint8_t index{0}; index < static_cast<uint8_t>(StartupMenuItem::Max); ++index)
  {
    const bool selected{index == static_cast<uint8_t>(startupMenuItem)};
    AdafruitGfxUtility::drawString(oledDisplay, selected ? ">" : " ", 0, index + 1);
    AdafruitGfxUtility::drawStringC(oledDisplay, String(menuItems[index]), index + 1);
  }
  oledDisplay.display();
}

void startSelectedMode()
{
  if (startupMenuItem == StartupMenuItem::Stopwatch)
  {
    stopWatch.setup();
    startupMode = StartupMode::Stopwatch;
  }
  else if (startupMenuItem == StartupMenuItem::FlappyGame)
  {
    flappyGame.setup();
    startupMode = StartupMode::FlappyGame;
  }
  else
  {
    controller.setup();
    if (startupMenuItem == StartupMenuItem::MemoryReset)
    {
      controller.resetSavedData();
    }
    if (startupMenuItem == StartupMenuItem::ControllerMeasurement)
    {
      controller.startMeasurementMode();
    }
    startupMode = StartupMode::BatteryController;
  }
}

void updateStartupMenu()
{
  buttonUStatus.update();
  buttonDStatus.update();
  buttonAStatus.update();

  const int itemCount{static_cast<int>(StartupMenuItem::Max)};
  if (buttonUStatus.getVal() == PushType::ReleaseShort)
  {
    startupMenuItem = static_cast<StartupMenuItem>(
      (static_cast<int>(startupMenuItem) + itemCount - 1) % itemCount);
    drawStartupMenu();
  }
  else if (buttonDStatus.getVal() == PushType::ReleaseShort)
  {
    startupMenuItem = static_cast<StartupMenuItem>(
      (static_cast<int>(startupMenuItem) + 1) % itemCount);
    drawStartupMenu();
  }
  else if (buttonAStatus.getVal() == PushType::ReleaseShort)
  {
    startSelectedMode();
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
  pinMode(PUSH_BUTTON_U, INPUT_PULLUP);
  pinMode(PUSH_BUTTON_D, INPUT_PULLUP);
  pinMode(PUSH_BUTTON_A, INPUT_PULLUP);
  buttonLStatus.init(PUSH_BUTTON_L);
  buttonRStatus.init(PUSH_BUTTON_R);
  buttonONStatus.init(PUSH_BUTTON_ON);
  buttonUStatus.init(PUSH_BUTTON_U);
  buttonDStatus.init(PUSH_BUTTON_D);
  buttonAStatus.init(PUSH_BUTTON_A);

  BatteryController::writePinReset();
  batteryMonitor.setup();

  AdafruitGfxUtility::setupDisplay(oledDisplay);
  drawStartupMenu();

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
      AdafruitGfxUtility::dumpDisplayAsPbm(oledDisplay, Serial);
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
#ifdef XIAO_BATTERY_VOLT_DISPLAY
    controller.drawXiaoBatteryVolt(batteryMonitor.xiaoVolt());
#else
    controller.drawXiaoBattery(batteryMonitor.xiaoVolt());
#endif
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

    if (startupMode == StartupMode::Menu)
    {
      updateStartupMenu();
    }
    else if (startupMode == StartupMode::Stopwatch)
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
      if (controller.shouldIdleSleep())
      {
        displayCurrentModeSleep();
        goDeepSleep();
      }
    }
  }

  // wait for a second
}
