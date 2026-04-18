#include <SPI.h>
#include <Wire.h>
#include <vector>
#include <stdio.h>

#include "battery_controller.hpp"
#include "display/draw_adafruit.hpp"

#include "flappy.hpp"

DrawAdafruit drawAdafruit;

BatteryController controller;

flappy::Game flappyGame;

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

  controller.preSetup();
  int PUSH_BUTTON_GAME{BatteryController::PUSH_BUTTON_D};
  pinMode(PUSH_BUTTON_GAME, INPUT_PULLUP);
  if (!digitalRead(PUSH_BUTTON_GAME))
  {
    gameModeFlag = true;
    flappyGame.setup();
  }
  else
  {
    controller.setup();
  }

}

// the loop function runs over and over again forever
void loop()
{
  if (gameModeFlag)
  {
    flappyGame.loop();
    return;
  }

#if 0
  digitalWrite(LED_BUILTIN, HIGH); // turn the LED on (HIGH is the voltage level)
  delay(100);                      // wait for a second
  digitalWrite(LED_BUILTIN, LOW);  // turn the LED off by making the voltage LOW
  delay(100);    

  return;
#endif

  while (true)
  {
    controller.loopWhile();
  }

  // wait for a second
}