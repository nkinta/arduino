#pragma once

#include <vector>

#include <Wire.h>
#define ARDUINO_ARCH_RP2040 // undef HAVE_PORTREG
// #include <Adafruit_GFX.h>
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

class DrawAdafruit
{
  static constexpr uint8_t SCREEN_WIDTH{128};
  static constexpr uint8_t SCREEN_HEIGHT{64};

  // Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
  static const uint8_t OLED_RESET{-1}; // Reset pin # (or -1 if sharing Arduino reset pin)

  static const uint8_t CHARSIZEX{6};
  static const uint8_t CHARSIZEY{9};
  static const uint8_t TEXT_SIZE{1};
  static const uint8_t UNIT_OFFSET{2};

  // static const uint8_t LINE_OFFSET{2};

public:

  Adafruit_SSD1306 _display{SCREEN_WIDTH, SCREEN_HEIGHT, &Wire}; //

  static void setDisplayTuneMenu(DrawAdafruit &adafruit, String &&title, std::vector<String> &menuList, std::vector<String> &valueList, int targetIndex);
  static String formatFloatZeroPad(float value, int integerDigits, int decimalDigits);

  void displaySleep()
  {
    _display.ssd1306_command(SSD1306_DISPLAYOFF);
  }

  void setupDisplay(void) {
    // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
    // adaDisplay.clearDisplay();
    if (!_display.begin(SSD1306_SWITCHCAPVCC, 0x3C, true, true)) { // Address 0x3C for 128x32
      Serial.println(F("SSD1306 allocation failed"));
      for (;;); // Don't proceed, loop forever
    }

    // Wire.setClock(1000);
    // Show initial display buffer contents on the screen --
    // the library initializes this with an Adafruit splash screen.

    // Clear the buffer
    // adaDisplay.setFont(&Picopixel);

    _display.clearDisplay();
    // drawCar();
    _display.display();
    _display.setTextSize(TEXT_SIZE);
    _display.setTextColor(SSD1306_WHITE);
  }

  void setCursor(int16_t x, int16_t y)
  {
    _display.setCursor(x, y);
  }

  void printString(const String& val)
  {
    _display.print(val);
  }

  void setFont(const GFXfont* f)
  {
    _display.setFont(f);
  }

  void removeFont()
  {
    _display.setFont(nullptr);
  }

  void setTextSize(uint8_t size)
  {
    _display.setTextSize(size);
  }

  void drawBat(const int index);

  void drawCar();

  void clearDisplay()
  {
    _display.clearDisplay();
    _display.setTextSize(TEXT_SIZE);
    _display.setTextColor(SSD1306_WHITE);
  }

  void drawFillLine(int line)
  {
    _display.fillRect(0, CHARSIZEY * line, SCREEN_WIDTH, CHARSIZEY, BLACK);
  }

  void drawFillR(int offsetX, int offsetY, int width)
  {
    _display.fillRect(CHARSIZEX * (offsetX - width), CHARSIZEY * offsetY, CHARSIZEX * width, CHARSIZEY, BLACK);
  }

  void drawStringC(const String& string, int offsetY)
  {
    const int offsetX{(SCREEN_WIDTH - (CHARSIZEX * string.length())) / (2 * CHARSIZEX)};
    drawChar(string.c_str(), offsetX, offsetY);
  }

  void drawString(const String& string, int offsetX, int offsetY) {
    drawChar(string.c_str(), offsetX, offsetY);
  }

  void drawFloat(float value, float offsetX, float offsetY, int decimal = 2, int integerDigit = 1) {
    _display.setCursor(CHARSIZEX * offsetX, CHARSIZEY * offsetY);
    _display.print(formatFloatZeroPad(value, integerDigit, decimal));
  }

  void drawInt(int value, float offsetX, float offsetY) {
    _display.setCursor(CHARSIZEX * offsetX, CHARSIZEY * offsetY);
    _display.print(value);
  }

  void drawStringR(const String& string, int offsetX, int offsetY) {
    drawChar(string.c_str(), offsetX - string.length(), offsetY);
  }

  void drawFloatR(float value, float offsetX, float offsetY, int size = 4, int decimal = 2, int integerDigit = 1) {
    String valueStr{formatFloatZeroPad(value, integerDigit, decimal)};
    const int offset{valueStr.length()};
    const int clearOffset{max(offset, size)};

    _display.setCursor(CHARSIZEX * (offsetX - offset), CHARSIZEY * offsetY);
    _display.print(valueStr);
  }

  void drawIntR(int value, float offsetX, float offsetY) {
    String valueInt{String(value)};
    _display.setCursor(CHARSIZEX * (offsetX - valueInt.length()), CHARSIZEY * offsetY);
    _display.print(valueInt);
  }

  void display()
  {
    _display.display();
  }

  void dumpDisplayAsPbm(Stream& out);

  void drawChar(const char* chr, int offsetX, int offsetY) {
    _display.setCursor(CHARSIZEX * offsetX, CHARSIZEY * offsetY);
    _display.print(chr);
  }

};
