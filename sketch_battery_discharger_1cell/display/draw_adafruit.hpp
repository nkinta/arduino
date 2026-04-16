#pragma once

#include <Wire.h>
#define ARDUINO_ARCH_RP2040 // undef HAVE_PORTREG
// #include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#undef ARDUINO_ARCH_RP2040

#include <Fonts/FreeMono9pt7b.h>
#include <Fonts/FreeMonoBoldOblique9pt7b.h>
#include <Fonts/FreeMonoOblique9pt7b.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSerif9pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSerifBoldItalic9pt7b.h>

#include <Fonts/Org_01.h>
#include <Fonts/PicoPixel.h>

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

  void setFont()
  {
    // FreeSerif9pt7b
    // FreeMonoBoldOblique9pt7b
    // FreeMonoOblique9pt7b
    // FreeSerifBoldItalic9pt7b
    // FreeMono9pt7b
    _display.setFont(&FreeMono9pt7b);
  }

  void removeFont()
  {
    _display.setFont(nullptr);
  }

  void setTextSize(uint8_t size)
  {
    _display.setTextSize(size);
  }

  void drawBat(const float voltage)
  {

    // https://javl.github.io/image2cpp/

    const unsigned char bat3[] PROGMEM = {
	    0x7f, 0xfc, 0x80, 0x02, 0xbb, 0xbb, 0xbb, 0xb9, 0xbb, 0xb9, 0xbb, 0xbb, 0x80, 0x02, 0x7f, 0xfc,
    };

    const unsigned char bat2[] PROGMEM = {
	    0x7f, 0xfc, 0x80, 0x02, 0xbb, 0x83, 0xbb, 0x81, 0xbb, 0x81, 0xbb, 0x83, 0x80, 0x02, 0x7f, 0xfc,
    };

    const unsigned char bat1[] PROGMEM = {
	    0x7f, 0xfc, 0x80, 0x02, 0xb8, 0x03, 0xb8, 0x01, 0xb8, 0x01, 0xb8, 0x03, 0x80, 0x02, 0x7f, 0xfc,
    };

    const unsigned char bat0[] PROGMEM = {
	    0x7f, 0xfc, 0x80, 0x02, 0x80, 0x03, 0x80, 0x01, 0x80, 0x01, 0x80, 0x03, 0x80, 0x02, 0x7f, 0xfc,
    };


    const unsigned char* bat_meter[4] = {
      bat0, bat1, bat2, bat3
    };

    uint8_t index{0};
    if (voltage > 4.05f)
    {
      index = 3;
    }
    else if (voltage > 3.85f)
    {
      index = 2;
    }
    else if (voltage > 3.6f)
    {
      index = 1;
    }
    else
    {
      index = 0;
    }

    drawFillLine(6);
    // drawFloat(voltage, 10, 6);
    _display.drawBitmap(110, 55, bat_meter[index], 16, 8, WHITE);
  }

  void drawCar()
  {
    // 'untitled', 16x16px
    unsigned char epd_bitmap_untitled[] PROGMEM = {
      0xff, 0xff, 0x7f, 0xff, 0xbf, 0xff, 0xa0, 0x3f, 0xdf, 0xdf, 0xbf, 0x23, 0xbf, 0xfd, 0xbf, 0xfe, 
      0xa7, 0xe6, 0xdb, 0xda, 0xd8, 0x19, 0xe7, 0xe7, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
    };

    // Array of all bitmaps for convenience. (Total bytes used to store images in PROGMEM = 48)
    const int epd_bitmap_allArray_LEN = 1;
    const unsigned char* epd_bitmap_allArray[1] = {
      epd_bitmap_untitled
    };

    _display.drawBitmap(3, 3, &epd_bitmap_untitled[0], 16, 16, WHITE);
  }

  void clearDisplay()
  {
    _display.clearDisplay();
    _display.setTextSize(TEXT_SIZE);
    _display.setTextColor(SSD1306_WHITE);
  }

  void drawFillLine(int line)
  {
    _display.fillRect(0, CHARSIZEY * line, SCREEN_WIDTH, CHARSIZEY, BLACK);
    // _display.drawFastHLine(0, CHARSIZEY + 5, 128, BLACK);
  }

  void drawStringC(const String& string, int offsetY)
  {
    const int offsetX{(SCREEN_WIDTH - (CHARSIZEX * string.length())) / (2 * CHARSIZEX)};
    drawChar(string.c_str(), offsetX, offsetY);
  }

  void drawString(const String& string, int offsetX, int offsetY) {
    drawChar(string.c_str(), offsetX, offsetY);
  }

  void drawStringR(const String& string, int offsetX, int offsetY) {
    drawChar(string.c_str(), offsetX - string.length(), offsetY);
  }

  void drawFloat(float value, float offsetX, float offsetY, int decimal = 2) {
    _display.setCursor(CHARSIZEX * offsetX, CHARSIZEY * offsetY);

    _display.print(String(value, decimal));
  }

  void drawInt(int value, float offsetX, float offsetY) {
    _display.setCursor(CHARSIZEX * offsetX, CHARSIZEY * offsetY);

    _display.print(value);
  }

  void drawFloatUnit(const char* chr, float value, float offsetX, float offsetY)
  {
    drawFloatR(value, offsetX, offsetY);
    _display.setCursor(CHARSIZEX * offsetX + UNIT_OFFSET, CHARSIZEY * offsetY);
    _display.print(chr);   
  }

  void drawIntUnit(const char* chr, int value, float offsetX, float offsetY)
  {
    drawIntR(value, offsetX, offsetY);
    _display.setCursor(CHARSIZEX * offsetX + UNIT_OFFSET, CHARSIZEY * offsetY);
    _display.print(chr);   
  }

  void drawRPM(int value, float offsetX, float offsetY)
  {
    const char chr[]{"rpm"};
    drawIntR(value, offsetX, offsetY);
    _display.setCursor(CHARSIZEX * offsetX + UNIT_OFFSET, CHARSIZEY * offsetY);
    _display.print(chr);
  }

  void drawV(float value, float offsetX, float offsetY)
  {
    const char chr[]{"v"};
    drawFloatR(value, offsetX, offsetY);
    _display.setCursor(CHARSIZEX * offsetX + UNIT_OFFSET, CHARSIZEY * offsetY);
    _display.print(chr);
  }

  void drawKm(float value, float offsetX, float offsetY)
  {
     const char chr[]{"km/h"};
    drawFloatR(value, offsetX, offsetY);
    _display.setCursor(CHARSIZEX * offsetX + UNIT_OFFSET, CHARSIZEY * offsetY);
    _display.print(chr);   
  }
  
  void drawI(float value, float offsetX, float offsetY)
  {
    const char chr[]{"a"};
    drawFloatR(value, offsetX, offsetY);
    _display.setCursor(CHARSIZEX * offsetX + UNIT_OFFSET, CHARSIZEY * offsetY);
    _display.print(chr);
  }

  void drawFloatR(float value, float offsetX, float offsetY, int size = 4, int decimal = 2) {
    String valueStr{String(value, decimal)};
    const int offset{valueStr.length()};
    const int clearOffset{max(offset, size)};

    _display.setCursor(CHARSIZEX * (offsetX - offset), CHARSIZEY * offsetY);
    _display.print(String(value, decimal));
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

  void drawChar(const char* chr, int offsetX, int offsetY) {
    // display.fillRect(CHARSIZEX * offsetX, CHARSIZEY * offsetY, CHARSIZEX * sizeChar, CHARSIZEY * 1, BLACK);
    _display.setCursor(CHARSIZEX * offsetX, CHARSIZEY * offsetY);
    // display.setTextColor(SSD1306_WHITE); // Draw 'inverse' text
    _display.print(chr);
  }


};