#pragma once

#include <vector>

#include <Wire.h>
#define ARDUINO_ARCH_RP2040 // undef HAVE_PORTREG
#include <Adafruit_SSD1306.h>
#undef ARDUINO_ARCH_RP2040

/*
#include <Fonts/FreeMono9pt7b.h>
#include <Fonts/FreeMono9pt7b.h>
#include <Fonts/FreeMonoBoldOblique9pt7b.h>
#include <Fonts/FreeMonoOblique9pt7b.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSerif9pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSerifBoldItalic9pt7b.h>
#include <Fonts/Org_01.h>
#include <Fonts/PicoPixel.h>
*/

class AdafruitGfxUtility
{
public:
  static constexpr uint8_t SCREEN_WIDTH{128};
  static constexpr uint8_t SCREEN_HEIGHT{64};
  static constexpr int8_t OLED_RESET{-1};

private:
  static constexpr uint8_t CHARSIZEX{6};
  static constexpr uint8_t CHARSIZEY{9};
  static constexpr uint8_t TEXT_SIZE{1};

public:
  static void setDisplayTuneMenu(Adafruit_SSD1306 &display, String &&title, std::vector<String> &menuList, std::vector<String> &valueList, int targetIndex);
  static String formatFloatZeroPad(float value, int integerDigits, int decimalDigits);

  static void displaySleep(Adafruit_SSD1306 &display)
  {
    display.ssd1306_command(SSD1306_DISPLAYOFF);
  }

  static void setupDisplay(Adafruit_SSD1306 &display)
  {
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C, true, true)) {
      Serial.println(F("SSD1306 allocation failed"));
      for (;;);
    }

    display.clearDisplay();
    display.display();
    display.setTextSize(TEXT_SIZE);
    display.setTextColor(SSD1306_WHITE);
  }

  static void drawBat(Adafruit_SSD1306 &display, int index);

  static void drawCar(Adafruit_SSD1306 &display);

  static void drawFillLine(Adafruit_SSD1306 &display, int line)
  {
    display.fillRect(0, CHARSIZEY * line, SCREEN_WIDTH, CHARSIZEY, BLACK);
  }

  static void drawFillR(Adafruit_SSD1306 &display, int offsetX, int offsetY, int width)
  {
    display.fillRect(CHARSIZEX * (offsetX - width), CHARSIZEY * offsetY, CHARSIZEX * width, CHARSIZEY, BLACK);
  }

  static void drawStringC(Adafruit_SSD1306 &display, const String& string, int offsetY)
  {
    const int offsetX{(SCREEN_WIDTH - (CHARSIZEX * string.length())) / (2 * CHARSIZEX)};
    drawChar(display, string.c_str(), offsetX, offsetY);
  }

  static void drawString(Adafruit_SSD1306 &display, const String& string, int offsetX, int offsetY)
  {
    drawChar(display, string.c_str(), offsetX, offsetY);
  }

  static void drawFloat(Adafruit_SSD1306 &display, float value, float offsetX, float offsetY, int decimal = 2, int integerDigit = 1)
  {
    display.setCursor(CHARSIZEX * offsetX, CHARSIZEY * offsetY);
    display.print(formatFloatZeroPad(value, integerDigit, decimal));
  }

  static void drawInt(Adafruit_SSD1306 &display, int value, float offsetX, float offsetY)
  {
    display.setCursor(CHARSIZEX * offsetX, CHARSIZEY * offsetY);
    display.print(value);
  }

  static void drawStringR(Adafruit_SSD1306 &display, const String& string, int offsetX, int offsetY)
  {
    drawChar(display, string.c_str(), offsetX - string.length(), offsetY);
  }

  static void drawFloatR(Adafruit_SSD1306 &display, float value, float offsetX, float offsetY, int size = 4, int decimal = 2, int integerDigit = 1)
  {
    String valueStr{formatFloatZeroPad(value, integerDigit, decimal)};
    const int offset{valueStr.length()};

    display.setCursor(CHARSIZEX * (offsetX - offset), CHARSIZEY * offsetY);
    display.print(valueStr);
  }

  static void drawIntR(Adafruit_SSD1306 &display, int value, float offsetX, float offsetY)
  {
    String valueInt{String(value)};
    display.setCursor(CHARSIZEX * (offsetX - valueInt.length()), CHARSIZEY * offsetY);
    display.print(valueInt);
  }

  static void dumpDisplayAsPbm(Adafruit_SSD1306 &display, Stream& out);

  static void drawChar(Adafruit_SSD1306 &display, const char* chr, int offsetX, int offsetY)
  {
    display.setCursor(CHARSIZEX * offsetX, CHARSIZEY * offsetY);
    display.print(chr);
  }
};
