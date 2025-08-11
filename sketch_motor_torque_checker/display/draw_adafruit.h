#pragma once

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

class DrawAdafruit
{

  static constexpr uint8_t SCREEN_HEIGHT{64};
  static constexpr uint8_t SCREEN_WIDTH{128};

  // Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
  static const uint8_t OLED_RESET{4}; // Reset pin # (or -1 if sharing Arduino reset pin)

  static const uint8_t CHARSIZEX{6};
  static const uint8_t CHARSIZEY{9};
  static const uint8_t TEXT_SIZE{1};
  static const uint8_t UNIT_OFFSET{2};

  // static const uint8_t LINE_OFFSET{2};

  Adafruit_SSD1306 adaDisplay{SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET};

public:

  void setupDisplay(void) {

    // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
    if (!adaDisplay.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3C for 128x32
      Serial.println(F("SSD1306 allocation failed"));
      for (;;); // Don't proceed, loop forever
    }

    // Show initial display buffer contents on the screen --
    // the library initializes this with an Adafruit splash screen.

    // Clear the buffer
    // adaDisplay.clearDisplay();
    adaDisplay.display();
    adaDisplay.setTextSize(TEXT_SIZE);
    adaDisplay.setTextColor(SSD1306_WHITE);
  }

  void clearDisplay()
  {
    adaDisplay.clearDisplay();
    adaDisplay.setTextSize(TEXT_SIZE);
    adaDisplay.setTextColor(SSD1306_WHITE);
  }

  void drawClear(int offsetY) {
    adaDisplay.fillRect(0, CHARSIZEY * offsetY, SCREEN_WIDTH, CHARSIZEY * 1, BLACK);
  }

  void drawString(const String& string, int offsetX, int offsetY) {
    drawChar(string.c_str(), offsetX, offsetY, string.length());
  }

  void drawFloat(float value, float offsetX, float offsetY, int decimal = 2) {
    adaDisplay.fillRect(CHARSIZEX * offsetX, CHARSIZEY * offsetY, CHARSIZEX * 5, CHARSIZEY * 1, BLACK);
    adaDisplay.setCursor(CHARSIZEX * offsetX, CHARSIZEY * offsetY);
    // adaDisplay.setTextColor(SSD1306_WHITE); // Draw 'inverse' text
    adaDisplay.print(String(value, decimal));
  }

  void drawInt(int value, float offsetX, float offsetY) {
    adaDisplay.fillRect(CHARSIZEX * offsetX, CHARSIZEY * offsetY, CHARSIZEX * 5, CHARSIZEY * 1, BLACK);
    adaDisplay.setCursor(CHARSIZEX * offsetX, CHARSIZEY * offsetY);

    // std::cout << std::setw(10) << std::right << num << std::endl;
    adaDisplay.print(value);
  }

  void drawRPM(int value, float offsetX, float offsetY)
  {
    const char chr[]{"rpm"};
    drawIntR(value, offsetX, offsetY);
    adaDisplay.setCursor(CHARSIZEX * (offsetX) + UNIT_OFFSET, CHARSIZEY * offsetY);
    adaDisplay.print(chr);
  }

  void drawV(float value, float offsetX, float offsetY)
  {
    const char chr[]{"v"};
    const int floatSize{4};
    drawFloat(value, offsetX, offsetY);
    adaDisplay.setCursor(CHARSIZEX * (offsetX + floatSize) + UNIT_OFFSET, CHARSIZEY * offsetY);
    adaDisplay.print(chr);
  }

  void drawKm(float value, float offsetX, float offsetY)
  {
     const char chr[]{"km/h"};
    const int floatSize{4};
    drawFloat(value, offsetX, offsetY);
    adaDisplay.setCursor(CHARSIZEX * (offsetX + floatSize) + UNIT_OFFSET, CHARSIZEY * offsetY);
    adaDisplay.print(chr);   
  }

  void drawI(float value, float offsetX, float offsetY)
  {
    const char chr[]{"a"};
    const int floatSize{4};
    drawFloat(value, offsetX, offsetY);
    adaDisplay.setCursor(CHARSIZEX * (offsetX + floatSize) + UNIT_OFFSET, CHARSIZEY * offsetY);
    adaDisplay.print(chr);
  }

  void drawFloatR(float value, float offsetX, float offsetY, int size = 4, int decimal = 2) {
    String valueStr{String(value, decimal)};
    const int offset{valueStr.length()};
    const int clearOffset{max(offset, size)};

    adaDisplay.fillRect(CHARSIZEX * (offsetX - clearOffset), CHARSIZEY * offsetY, CHARSIZEX * clearOffset, CHARSIZEY * 1, BLACK);
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

  void drawFillLine(int line)
  {
    adaDisplay.fillRect(0, CHARSIZEY * line, 128, CHARSIZEY, BLACK);
    // adaDisplay.drawFastHLine(0, CHARSIZEY + 5, 128, BLACK);
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

  void drawChar(const char* chr, int offsetX, int offsetY, int sizeChar) {
    adaDisplay.fillRect(CHARSIZEX * offsetX, CHARSIZEY * offsetY, CHARSIZEX * sizeChar, CHARSIZEY * 1, BLACK);
    adaDisplay.setCursor(CHARSIZEX * offsetX, CHARSIZEY * offsetY);
    // adaDisplay.setTextColor(SSD1306_WHITE); // Draw 'inverse' text
    adaDisplay.print(chr);
  }


};