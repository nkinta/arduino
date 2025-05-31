#pragma once

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

class DrawAdafruit
{

  static constexpr uint8_t SCREEN_HEIGHT{32};
  static constexpr uint8_t SCREEN_WIDTH{128};

  // Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
  static const uint8_t OLED_RESET{4}; // Reset pin # (or -1 if sharing Arduino reset pin)

  static const uint8_t CHARSIZEX{6};
  static const uint8_t CHARSIZEY{8};
  static const uint8_t TEXT_SIZE{1};

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
    adaDisplay.clearDisplay();
    adaDisplay.display();
    adaDisplay.setTextSize(TEXT_SIZE);
  }

  void clearDisplay()
  {
    adaDisplay.clearDisplay();
  }

  void drawString(const String& string, int offsetX, int offsetY) {

    drawChar(string.c_str(), offsetX, offsetY, string.length());
  }

  void drawChar(const char* chr, int offsetX, int offsetY, int sizeChar) {
    adaDisplay.fillRect(CHARSIZEX * offsetX, CHARSIZEY * offsetY, CHARSIZEX * sizeChar, CHARSIZEY * 1, BLACK);
    adaDisplay.setCursor(CHARSIZEX * offsetX, CHARSIZEY * offsetY);
    adaDisplay.setTextColor(SSD1306_WHITE); // Draw 'inverse' text
    adaDisplay.print(chr);
  }

  void drawFloat(float value, float offsetX, float offsetY, int decimal = 2) {
    adaDisplay.fillRect(CHARSIZEX * offsetX, CHARSIZEY * offsetY, CHARSIZEX * 5, CHARSIZEY * 1, BLACK);
    adaDisplay.setCursor(CHARSIZEX * offsetX, CHARSIZEY * offsetY);
    adaDisplay.setTextColor(SSD1306_WHITE); // Draw 'inverse' text
    adaDisplay.print(String(value, decimal));
  }

  void drawFloatR(float value, float offsetX, float offsetY, int size = 4, int decimal = 2) {

    String valueStr{String(value, decimal)};
    int offset{valueStr.length()};

    adaDisplay.fillRect(CHARSIZEX * (offsetX - size), CHARSIZEY * offsetY, CHARSIZEX * offsetX, CHARSIZEY * 1, BLACK);
    adaDisplay.setCursor(CHARSIZEX * (offsetX - offset), CHARSIZEY * offsetY);
    adaDisplay.setTextColor(SSD1306_WHITE); // Draw 'inverse' text
    adaDisplay.print(String(value, decimal));
  }

  void drawIntR(int value, float offsetX, float offsetY, int startOffset) {
    const int offset{startOffset};
    int numOffset{offset - 1};

    if (value != 0)
    {
      numOffset -= (floor(log10(value)));
    }

    adaDisplay.fillRect(CHARSIZEX * offsetX, CHARSIZEY * offsetY, CHARSIZEX * (offset + numOffset), CHARSIZEY * 1, BLACK);
    adaDisplay.setCursor(CHARSIZEX * (offsetX + numOffset), CHARSIZEY * offsetY);
    adaDisplay.setTextColor(SSD1306_WHITE); // Draw 'inverse' text
    // std::cout << std::setw(10) << std::right << num << std::endl;
    adaDisplay.print(value);
  }

  void drawInt(int value, float offsetX, float offsetY) {
    adaDisplay.fillRect(CHARSIZEX * offsetX, CHARSIZEY * offsetY, CHARSIZEX * 5, CHARSIZEY * 1, BLACK);
    adaDisplay.setCursor(CHARSIZEX * offsetX, CHARSIZEY * offsetY);
    adaDisplay.setTextColor(SSD1306_WHITE); // Draw 'inverse' text
    // std::cout << std::setw(10) << std::right << num << std::endl;
    adaDisplay.print(value);
  }

  void display()
  {
    adaDisplay.display();
  }
};