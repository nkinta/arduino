#include <SPI.h>
#include <Wire.h>
#include <vector>
#include <stdio.h>

#include <ArduinoLowPower.h>

#include "battery_controller.hpp"
#include "display/draw_adafruit.hpp"

#include "flappy.hpp"

DrawAdafruit drawAdafruit;

BatteryController controller;

ButtonStatus buttonONStatus{};

flappy::Game flappyGame;

unsigned long loopSubMillis{0};;

bool gameModeFlag{false};

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
  int PUSH_BUTTON_GAME{PUSH_BUTTON_D};
  pinMode(PUSH_BUTTON_GAME, INPUT_PULLUP);
  gameModeFlag = !digitalRead(PUSH_BUTTON_GAME);
  // gameModeFlag = true;
  if (gameModeFlag)
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

    LowPower.deepSleep(365 * 24 * 3600 * 1000); // 7 * 24 * 3600 * 1000 // one week // 365 * 24 * 3600 * 1000
}

void loopSub()
{
  buttonONStatus.update();

  if (buttonONStatus.getVal() == PushType::PushLong)
  {
    if (gameModeFlag)
    {
      flappyGame.clearDisplay();
    }
    else
    {
      controller.displaySleep();
    }
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

    if (gameModeFlag)
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