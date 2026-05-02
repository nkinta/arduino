#pragma once

#include <Arduino.h>
#include <cstdio>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "button_status.hpp"
#include "discharger_define.hpp"
#include "display/fonts/BBHBogle-Regular_16.h"
#include "display/fonts/BBHBogle-Regular_14.h"

namespace stopwatch
{
  class Stopwatch
  {
    static constexpr int16_t SCREEN_WIDTH{128};
    static constexpr int16_t SCREEN_HEIGHT{64};
    static constexpr int16_t OLED_RESET{-1};

    Adafruit_SSD1306 _display{SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET};

    ButtonStatus _buttonLStatus{};
    ButtonStatus _buttonAStatus{};
    ButtonStatus _buttonBStatus{};

    bool _running{false};
    unsigned long _startMillis{0};
    unsigned long _elapsedMillis{0};
    unsigned long _lapMillis{0};
    bool _lapValid{false};
    unsigned long _lastDrawMillis{0};

    bool _stopWatchEnd{false};

    static constexpr unsigned long DRAW_INTERVAL_MS{33};

    unsigned long currentElapsedMillis() const
    {
      if (!_running)
      {
        return _elapsedMillis;
      }

      return _elapsedMillis + (millis() - _startMillis);
    }

    void reset()
    {
      _running = false;
      _startMillis = 0;
      _elapsedMillis = 0;
      _lapMillis = 0;
      _lapValid = false;
    }

    void toggleRunning()
    {
      if (_running)
      {
        _elapsedMillis = currentElapsedMillis();
        _running = false;
      }
      else
      {
        _startMillis = millis();
        _running = true;
      }
    }

    void captureLap()
    {
      if (!_running)
      {
        return;
      }

      _lapMillis = currentElapsedMillis();
      _lapValid = true;
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

      _display.setTextSize(textSize);
      _display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
      _display.setCursor((SCREEN_WIDTH - static_cast<int16_t>(w)) / 2, y);
      _display.print(text);
    }

    int16_t measureGlyphWidth(char c)
    {
      char text[2]{c, '\0'};
      int16_t x1{};
      int16_t y1{};
      uint16_t w{};
      uint16_t h{};
      _display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
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
        _display.getTextBounds(glyph, 0, 0, &x1, &y1, &w, &h);
        _display.setCursor(cursorX + (slotWidth - static_cast<int16_t>(w)) / 2 - x1, baselineY);
        _display.print(glyph);
        cursorX += slotWidth;
        firstGlyph = false;
      }
    }

    void draw()
    {
      char timeText[16]{};
      char lapText[22]{};
      formatElapsed(currentElapsedMillis(), timeText, sizeof(timeText));

      _display.clearDisplay();
      _display.setTextColor(SSD1306_WHITE);

      drawCenteredText("STOPWATCH", 2, 1);

      _display.setFont(&BBHBogle_Regular14pt7b);
      _display.setTextSize(1);
      drawAlignedElapsed(timeText, 36);
      _display.setFont();

      _display.setTextSize(1);
      _display.setCursor(0, 46);
      _display.print(_running ? F("A:STOP") : F("A:START"));

      _display.setCursor(74, 46);
      _display.print(F("B:RESET"));

      _display.setCursor(0, 56);
      if (_lapValid)
      {
        char lapTime[16]{};
        formatElapsed(_lapMillis, lapTime, sizeof(lapTime));
        snprintf(lapText, sizeof(lapText), "LAP %s", lapTime);
        _display.print(lapText);
      }
      else
      {
        _display.print(F("L:LAP"));
      }

      _display.display();
    }

    void clearDisplay()
    {
      _display.clearDisplay();
      _display.display();
    }

  public:

    void displaySleep()
    {
      clearDisplay();
      _display.ssd1306_command(SSD1306_DISPLAYOFF);
    }

    void setup()
    {
      pinMode(PUSH_BUTTON_L, INPUT_PULLUP);
      pinMode(PUSH_BUTTON_A, INPUT_PULLUP);
      pinMode(PUSH_BUTTON_B, INPUT_PULLUP);

      _buttonLStatus.init(PUSH_BUTTON_L);
      _buttonAStatus.init(PUSH_BUTTON_A);
      _buttonBStatus.init(PUSH_BUTTON_B);

      _display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
      _display.clearDisplay();
      _display.display();

      reset();
      draw();
    }

    void loop()
    {
      if (_stopWatchEnd)
      {
        return;
      }

      _buttonLStatus.update();
      _buttonAStatus.update();
      _buttonBStatus.update();

      if (_buttonAStatus.getVal() == PushType::Pushed)
      {
        toggleRunning();
      }

      if (_buttonBStatus.getVal() == PushType::Pushed)
      {
        reset();
      }

      if (_buttonLStatus.getVal() == PushType::Pushed)
      {
        captureLap();
      }

      const unsigned long now{millis()};
      if ((now - _lastDrawMillis) < DRAW_INTERVAL_MS &&
          _buttonAStatus.getVal() == PushType::None &&
          _buttonBStatus.getVal() == PushType::None &&
          _buttonLStatus.getVal() == PushType::None)
      {
        return;
      }

      _lastDrawMillis = now;
      draw();
    }
  };
}
