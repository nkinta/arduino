#pragma once

#include <Arduino.h>
#include <cstdio>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "button_status.hpp"
#include "discharger_define.hpp"
#include "display/fonts/BBHBogle-Regular_16.h"

namespace stopwatch
{
  class Stopwatch
  {
    static constexpr int16_t SCREEN_WIDTH{128};
    static constexpr int16_t SCREEN_HEIGHT{64};
    static constexpr int16_t OLED_RESET{-1};

    Adafruit_SSD1306 display{SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET};

    ButtonStatus buttonLStatus{};
    ButtonStatus buttonAStatus{};
    ButtonStatus buttonBStatus{};

    bool running{false};
    unsigned long startMillis{0};
    unsigned long elapsedMillis{0};
    unsigned long lapMillis{0};
    bool lapValid{false};
    unsigned long lastDrawMillis{0};

    bool stopWatchEnd{false};

    static constexpr unsigned long DRAW_INTERVAL_MS{33};

    unsigned long currentElapsedMillis() const
    {
      if (!running)
      {
        return elapsedMillis;
      }

      return elapsedMillis + (millis() - startMillis);
    }

    void reset()
    {
      running = false;
      startMillis = 0;
      elapsedMillis = 0;
      lapMillis = 0;
      lapValid = false;
    }

    void toggleRunning()
    {
      if (running)
      {
        elapsedMillis = currentElapsedMillis();
        running = false;
      }
      else
      {
        startMillis = millis();
        running = true;
      }
    }

    void captureLap()
    {
      if (!running)
      {
        return;
      }

      lapMillis = currentElapsedMillis();
      lapValid = true;
    }

    static void formatElapsed(unsigned long totalMillis, char *buffer, size_t bufferSize)
    {
      const unsigned long totalCentiseconds{totalMillis / 10UL};
      const unsigned long centiseconds{totalCentiseconds % 100UL};
      const unsigned long totalSeconds{totalCentiseconds / 100UL};
      const unsigned long seconds{totalSeconds % 60UL};
      const unsigned long minutes{(totalSeconds / 60UL) % 100UL};

      snprintf(buffer, bufferSize, "%02lu:%02lu.%02lu", minutes, seconds, centiseconds);
    }

    void drawCenteredText(const char *text, int16_t y, uint8_t textSize)
    {
      int16_t x1{};
      int16_t y1{};
      uint16_t w{};
      uint16_t h{};

      display.setTextSize(textSize);
      display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
      display.setCursor((SCREEN_WIDTH - static_cast<int16_t>(w)) / 2, y);
      display.print(text);
    }

    int16_t measureGlyphWidth(char c)
    {
      char text[2]{c, '\0'};
      int16_t x1{};
      int16_t y1{};
      uint16_t w{};
      uint16_t h{};
      display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
      return static_cast<int16_t>(w);
    }

    void drawAlignedElapsed(const char *text, int16_t baselineY)
    {
      static constexpr char SAMPLE_DIGITS[] = "0123456789";
      int16_t digitWidth{};
      for (const char *p = SAMPLE_DIGITS; *p != '\0'; ++p)
      {
        const int16_t currentWidth{measureGlyphWidth(*p)};
        if (currentWidth > digitWidth)
        {
          digitWidth = currentWidth;
        }
      }

      const int16_t colonWidth{measureGlyphWidth(':')};
      const int16_t dotWidth{measureGlyphWidth('.')};
      const int16_t digitSpacing{1};
      const int16_t punctuationSpacing{2};

      int16_t totalWidth{};
      bool firstGlyph{true};
      for (const char *p = text; *p != '\0'; ++p)
      {
        if (!firstGlyph)
        {
          totalWidth += ((*p >= '0') && (*p <= '9')) ? digitSpacing : punctuationSpacing;
        }

        if ((*p >= '0') && (*p <= '9'))
        {
          totalWidth += digitWidth;
        }
        else if (*p == ':')
        {
          totalWidth += colonWidth;
        }
        else if (*p == '.')
        {
          totalWidth += dotWidth;
        }
        else
        {
          continue;
        }

        firstGlyph = false;
      }

      int16_t cursorX{static_cast<int16_t>((SCREEN_WIDTH - totalWidth) / 2)};
      firstGlyph = true;
      for (const char *p = text; *p != '\0'; ++p)
      {
        const char currentChar{*p};
        int16_t slotWidth{};
        int16_t spacing{};
        if ((currentChar >= '0') && (currentChar <= '9'))
        {
          slotWidth = digitWidth;
          spacing = digitSpacing;
        }
        else if (currentChar == ':')
        {
          slotWidth = colonWidth;
          spacing = punctuationSpacing;
        }
        else if (currentChar == '.')
        {
          slotWidth = dotWidth;
          spacing = punctuationSpacing;
        }
        else
        {
          continue;
        }

        if (!firstGlyph)
        {
          cursorX += spacing;
        }

        char glyph[2]{currentChar, '\0'};
        int16_t x1{};
        int16_t y1{};
        uint16_t w{};
        uint16_t h{};
        display.getTextBounds(glyph, 0, 0, &x1, &y1, &w, &h);
        display.setCursor(cursorX + (slotWidth - static_cast<int16_t>(w)) / 2 - x1, baselineY);
        display.print(glyph);
        cursorX += slotWidth;
        firstGlyph = false;
      }
    }

    void draw()
    {
      char timeText[16]{};
      char lapText[22]{};
      formatElapsed(currentElapsedMillis(), timeText, sizeof(timeText));

      display.clearDisplay();
      display.setTextColor(SSD1306_WHITE);

      drawCenteredText("STOPWATCH", 2, 1);

      display.setFont(&BBHBogle_Regular16pt7b);
      display.setTextSize(1);
      drawAlignedElapsed(timeText, 36);
      display.setFont();

      display.setTextSize(1);
      display.setCursor(0, 46);
      display.print(running ? F("A:STOP") : F("A:START"));

      display.setCursor(74, 46);
      display.print(F("B:RESET"));

      display.setCursor(0, 56);
      if (lapValid)
      {
        char lapTime[16]{};
        formatElapsed(lapMillis, lapTime, sizeof(lapTime));
        snprintf(lapText, sizeof(lapText), "LAP %s", lapTime);
        display.print(lapText);
      }
      else
      {
        display.print(F("L:LAP"));
      }

      display.display();
    }

    void clearDisplay()
    {
      display.clearDisplay();
      display.display();
    }

  public:

    void displaySleep()
    {
      clearDisplay();
      display.ssd1306_command(SSD1306_DISPLAYOFF);
    }

    void setup()
    {
      pinMode(PUSH_BUTTON_L, INPUT_PULLUP);
      pinMode(PUSH_BUTTON_A, INPUT_PULLUP);
      pinMode(PUSH_BUTTON_B, INPUT_PULLUP);

      buttonLStatus.init(PUSH_BUTTON_L);
      buttonAStatus.init(PUSH_BUTTON_A);
      buttonBStatus.init(PUSH_BUTTON_B);

      display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
      display.clearDisplay();
      display.display();

      reset();
      draw();
    }

    void loop()
    {
      if (stopWatchEnd)
      {
        return;
      }

      buttonLStatus.update();
      buttonAStatus.update();
      buttonBStatus.update();

      if (buttonAStatus.getVal() == PushType::Pushed)
      {
        toggleRunning();
      }

      if (buttonBStatus.getVal() == PushType::Pushed)
      {
        reset();
      }

      if (buttonLStatus.getVal() == PushType::Pushed)
      {
        captureLap();
      }

      const unsigned long now{millis()};
      if ((now - lastDrawMillis) < DRAW_INTERVAL_MS &&
          buttonAStatus.getVal() == PushType::None &&
          buttonBStatus.getVal() == PushType::None &&
          buttonLStatus.getVal() == PushType::None)
      {
        return;
      }

      lastDrawMillis = now;
      draw();
    }
  };
}
