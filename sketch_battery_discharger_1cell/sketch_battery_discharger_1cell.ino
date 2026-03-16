#include <SPI.h>
#include <Wire.h>
#include <vector>
#include <stdio.h>

#include "battery_controller.hpp"
#include "display/draw_adafruit.hpp"
#include "voltage_mapping.hpp"

DrawAdafruit drawAdafruit;

VoltageMapping voltageMapping;

BatteryController controller;

// the setup function runs once when you press reset or power the board
void setup()
{
  pinMode(LED_BUILTIN, OUTPUT);

#ifdef SERIAL_DEBUG_ON
  Serial.begin(115200);
  // while (!Serial);
  Serial.print("Start!");
#endif

  drawAdafruit.setupDisplay();
  controller.setup();
}

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

  drawAdafruit.display();

  while (true)
  {
    controller.loopWhile();
  }

  // wait for a second
}