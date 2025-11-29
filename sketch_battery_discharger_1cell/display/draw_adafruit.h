#pragma once

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

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

  Adafruit_SSD1306 adaDisplay{SCREEN_WIDTH, SCREEN_HEIGHT};



  void setupDisplay(void) {

    // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
    // adaDisplay.clearDisplay();
    if (!adaDisplay.begin(SSD1306_SWITCHCAPVCC, 0x3C, true, true)) { // Address 0x3C for 128x32
      Serial.println(F("SSD1306 allocation failed"));
      for (;;); // Don't proceed, loop forever
    }

    // Show initial display buffer contents on the screen --
    // the library initializes this with an Adafruit splash screen.

    // Clear the buffer

    // adaDisplay.setFont(&Picopixel);

    adaDisplay.clearDisplay();
    // drawCar();
    adaDisplay.display();
    adaDisplay.setTextSize(TEXT_SIZE);
    adaDisplay.setTextColor(SSD1306_WHITE);
  }

  void setFont()
  {
    // FreeSerif9pt7b
    // FreeMonoBoldOblique9pt7b
    // FreeMonoOblique9pt7b
    // FreeSerifBoldItalic9pt7b
    // FreeMono9pt7b
    adaDisplay.setFont(&FreeMono9pt7b);
  }

  void removeFont()
  {
    adaDisplay.setFont(nullptr);
  }

  void setTextSize(uint8_t size)
  {
    adaDisplay.setTextSize(size);
  }

  void drawBat(const float voltage)
  {

    const unsigned char bat3[] PROGMEM = {
	    0x7f, 0xfc, 0x80, 0x02, 0xbb, 0xbb, 0xbb, 0xb9, 0xbb, 0xb9, 0xbb, 0xbb, 0x80, 0x02, 0x7f, 0xfc,
    };

    const unsigned char bat2[] PROGMEM = {
	    0x7f, 0xfc, 0x80, 0x02, 0xbb, 0x83, 0xbb, 0x81, 0xbb, 0x81, 0xbb, 0x83, 0x80, 0x02, 0x7f, 0xfc,
    };

    const unsigned char bat1[] PROGMEM = {
	    0x7f, 0xfc, 0x80, 0x02, 0xb8, 0x03, 0xb8, 0x01, 0xb8, 0x01, 0xb8, 0x03, 0x80, 0x02, 0x7f, 0xfc,
    };

    const unsigned char* bat_meter[3] = {
      bat1, bat2, bat3
    };

    uint8_t index{0};
    if (voltage > 4.1f)
    {
      index = 2;
    }
    else if (voltage > 3.8f)
    {
      index = 1;
    }
    else
    {
      index = 0;
    }

    drawFillLine(6);
    // drawFloat(voltage, 10, 6);
    adaDisplay.drawBitmap(110, 55, bat_meter[index], 16, 8, WHITE);
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

    adaDisplay.drawBitmap(3, 3, &epd_bitmap_untitled[0], 16, 16, WHITE);
  }

  void clearDisplay()
  {
    adaDisplay.clearDisplay();
    adaDisplay.setTextSize(TEXT_SIZE);
    adaDisplay.setTextColor(SSD1306_WHITE);
  }

  void drawFillLine(int line)
  {
    adaDisplay.fillRect(0, CHARSIZEY * line, SCREEN_WIDTH, CHARSIZEY, BLACK);
    // adaDisplay.drawFastHLine(0, CHARSIZEY + 5, 128, BLACK);
  }

  void drawString(const String& string, int offsetX, int offsetY) {
    drawChar(string.c_str(), offsetX, offsetY);
  }

  void drawFloat(float value, float offsetX, float offsetY, int decimal = 2) {
    adaDisplay.setCursor(CHARSIZEX * offsetX, CHARSIZEY * offsetY);

    adaDisplay.print(String(value, decimal));
  }

  void drawInt(int value, float offsetX, float offsetY) {
    adaDisplay.setCursor(CHARSIZEX * offsetX, CHARSIZEY * offsetY);

    adaDisplay.print(value);
  }

  void drawFloatUnit(const char* chr, float value, float offsetX, float offsetY)
  {
    drawFloatR(value, offsetX, offsetY);
    adaDisplay.setCursor(CHARSIZEX * offsetX + UNIT_OFFSET, CHARSIZEY * offsetY);
    adaDisplay.print(chr);   
  }

  void drawIntUnit(const char* chr, int value, float offsetX, float offsetY)
  {
    drawIntR(value, offsetX, offsetY);
    adaDisplay.setCursor(CHARSIZEX * offsetX + UNIT_OFFSET, CHARSIZEY * offsetY);
    adaDisplay.print(chr);   
  }

  void drawRPM(int value, float offsetX, float offsetY)
  {
    const char chr[]{"rpm"};
    drawIntR(value, offsetX, offsetY);
    adaDisplay.setCursor(CHARSIZEX * offsetX + UNIT_OFFSET, CHARSIZEY * offsetY);
    adaDisplay.print(chr);
  }

  void drawV(float value, float offsetX, float offsetY)
  {
    const char chr[]{"v"};
    drawFloatR(value, offsetX, offsetY);
    adaDisplay.setCursor(CHARSIZEX * offsetX + UNIT_OFFSET, CHARSIZEY * offsetY);
    adaDisplay.print(chr);
  }

  void drawKm(float value, float offsetX, float offsetY)
  {
     const char chr[]{"km/h"};
    drawFloatR(value, offsetX, offsetY);
    adaDisplay.setCursor(CHARSIZEX * offsetX + UNIT_OFFSET, CHARSIZEY * offsetY);
    adaDisplay.print(chr);   
  }
  
  void drawI(float value, float offsetX, float offsetY)
  {
    const char chr[]{"a"};
    drawFloatR(value, offsetX, offsetY);
    adaDisplay.setCursor(CHARSIZEX * offsetX + UNIT_OFFSET, CHARSIZEY * offsetY);
    adaDisplay.print(chr);
  }

  void drawFloatR(float value, float offsetX, float offsetY, int size = 4, int decimal = 2) {
    String valueStr{String(value, decimal)};
    const int offset{valueStr.length()};
    const int clearOffset{max(offset, size)};

    // adaDisplay.fillRect(CHARSIZEX * (offsetX - clearOffset), CHARSIZEY * offsetY, CHARSIZEX * clearOffset, CHARSIZEY * 1, BLACK);
    adaDisplay.setCursor(CHARSIZEX * (offsetX - offset), CHARSIZEY * offsetY);
    // adaDisplay.setTextColor(SSD1306_WHITE); // Draw 'inverse' text
    adaDisplay.print(String(value, decimal));
  }

  void drawIntROld(int value, float offsetX, float offsetY, int startOffset) {
    const int offset{startOffset};
    int numOffset{offset - 1};

    if (value != 0)
    {
      numOffset -= (floor(log10(value)));
    }

    adaDisplay.fillRect(CHARSIZEX * offsetX, CHARSIZEY * offsetY, CHARSIZEX * (offset + numOffset), CHARSIZEY * 1, BLACK);
    adaDisplay.setCursor(CHARSIZEX * (offsetX + numOffset), CHARSIZEY * offsetY);
    // std::cout << std::setw(10) << std::right << num << std::endl;
    adaDisplay.print(value);
  }

  void drawIntR(int value, float offsetX, float offsetY) {
    String valueInt{String(value)};

    adaDisplay.fillRect(CHARSIZEX * (offsetX - valueInt.length()), CHARSIZEY * offsetY, CHARSIZEX * valueInt.length(), CHARSIZEY * 1, BLACK);
    adaDisplay.setCursor(CHARSIZEX * (offsetX - valueInt.length()), CHARSIZEY * offsetY);

    adaDisplay.print(valueInt.c_str());
  }

  void display()
  {
    adaDisplay.display();
  }

  void drawChar(const char* chr, int offsetX, int offsetY) {
    // adaDisplay.fillRect(CHARSIZEX * offsetX, CHARSIZEY * offsetY, CHARSIZEX * sizeChar, CHARSIZEY * 1, BLACK);
    adaDisplay.setCursor(CHARSIZEX * offsetX, CHARSIZEY * offsetY);
    // adaDisplay.setTextColor(SSD1306_WHITE); // Draw 'inverse' text
    adaDisplay.print(chr);
  }


};