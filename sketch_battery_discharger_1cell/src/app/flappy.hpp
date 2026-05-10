#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "../../button_status.hpp"
#include "../../discharger_define.hpp"
#include "../display/adafruit_gfx_utility.hpp"

extern Adafruit_SSD1306 oledDisplay;

namespace flappy
{
  constexpr int16_t SCREEN_WIDTH{AdafruitGfxUtility::SCREEN_WIDTH};
  constexpr int16_t SCREEN_HEIGHT{AdafruitGfxUtility::SCREEN_HEIGHT};

  constexpr int16_t ANALOG_READ{D9};

  constexpr int16_t STATE_TITLE{0};
  constexpr int16_t STATE_PLAYING{1};
  constexpr int16_t STATE_DEAD{2};

  constexpr int16_t GRAVITY{3};
  constexpr int16_t FLAP_VEL{-40};
  constexpr int16_t BIRD_X{20};
  constexpr int16_t BIRD_W{6};
  constexpr int16_t BIRD_H{5};

  constexpr int16_t INIT_PIPE_SPEED{1};

  constexpr int16_t PIPE_WIDTH{8};
  constexpr int16_t PIPE_GAP{25};
  constexpr int16_t PIPE_GAP_MIN{8};
  constexpr int16_t NUM_PIPES{2};
  constexpr int16_t PIPE_SPACING{SCREEN_WIDTH / NUM_PIPES};

  constexpr int16_t FRAME_MS{28};

  class Game
  {
    enum class CharacterType
    {
      Bird,
      Car,
    };

    struct Pipe
    {
      int _x;
      int _gapY;
      bool _scored;
    };

    bool _gameEnd{false};

    int _gameState;
    int _score;
    int _highScore;
    int _pipeSpeed;

    int _birdY16;
    int _velY16;

    Pipe _pipes[NUM_PIPES];

    ButtonStatus _buttonFlapStatus{};
    ButtonStatus _buttonRestartStatus{};
    unsigned long _lastFrame;

    void drawBird(int y, CharacterType characterType)
    {
      if (characterType == CharacterType::Bird)
      {
        oledDisplay.fillRect(BIRD_X, y, BIRD_W, BIRD_H, WHITE);
        oledDisplay.drawPixel(BIRD_X + BIRD_W, y + 2, WHITE);
        oledDisplay.drawPixel(BIRD_X + 4, y + 1, BLACK);
        oledDisplay.drawPixel(BIRD_X + 2, y + BIRD_H, WHITE);
        oledDisplay.drawPixel(BIRD_X + 3, y + BIRD_H, WHITE);
        return;
      }

      oledDisplay.fillRect(BIRD_X, y, BIRD_W, BIRD_H, WHITE);
      oledDisplay.drawPixel(BIRD_X + 1, y + BIRD_H, WHITE);
      oledDisplay.drawPixel(BIRD_X + 5, y + BIRD_H, WHITE);
      oledDisplay.drawPixel(BIRD_X + BIRD_W, y + 2, WHITE);
      oledDisplay.drawPixel(BIRD_X + BIRD_W, y + 3, WHITE);
      oledDisplay.drawPixel(BIRD_X + BIRD_W, y + 4, WHITE);
      oledDisplay.drawPixel(BIRD_X, y + 1, BLACK);

      oledDisplay.drawPixel(BIRD_X + 4, y + 2, BLACK);
      oledDisplay.drawPixel(BIRD_X + 5, y + 2, BLACK);
      oledDisplay.drawPixel(BIRD_X + 5, y, BLACK);
    }

    void drawPipes()
    {
      for (int i = 0; i < NUM_PIPES; i++)
      {
        int px = _pipes[i]._x;
        int gy = _pipes[i]._gapY;
        if (gy > 0)
        {
          oledDisplay.fillRect(px, 0, PIPE_WIDTH, gy, WHITE);
        }
        const int botY = gy + PIPE_GAP;
        if (botY < SCREEN_HEIGHT)
        {
          oledDisplay.fillRect(px, botY, PIPE_WIDTH, SCREEN_HEIGHT - botY, WHITE);
        }
      }
    }

    void spawnPipe(int i, int startX)
    {
      _pipes[i]._x = startX;
      _pipes[i]._gapY = PIPE_GAP_MIN + random(0, SCREEN_HEIGHT - PIPE_GAP - PIPE_GAP_MIN * 2);
      _pipes[i]._scored = false;
    }

    void initPipes()
    {
      for (int i = 0; i < NUM_PIPES; i++)
      {
        spawnPipe(i, SCREEN_WIDTH + i * PIPE_SPACING);
      }
    }

    bool checkCollision()
    {
      const int birdY = _birdY16 / 16;
      if (birdY < 0 || birdY + BIRD_H >= SCREEN_HEIGHT)
      {
        return true;
      }

      const int birdRight = BIRD_X + BIRD_W;
      for (int i = 0; i < NUM_PIPES; i++)
      {
        if (birdRight > _pipes[i]._x && BIRD_X < _pipes[i]._x + PIPE_WIDTH)
        {
          if (birdY < _pipes[i]._gapY || birdY + BIRD_H > _pipes[i]._gapY + PIPE_GAP)
          {
            return true;
          }
        }
      }
      return false;
    }

    void resetGame()
    {
      _birdY16 = (SCREEN_HEIGHT / 2) * 16;
      _velY16 = 0;
      _score = 0;
      _pipeSpeed = INIT_PIPE_SPEED;
      initPipes();
      _gameState = STATE_PLAYING;
      _lastFrame = millis();
    }

    void clearDisplay()
    {
      oledDisplay.clearDisplay();
      oledDisplay.display();
    }

  public:
    void displaySleep()
    {
      _gameEnd = true;
      clearDisplay();
      oledDisplay.ssd1306_command(SSD1306_DISPLAYOFF);
    }

    void setup()
    {
      pinMode(PUSH_BUTTON_A, INPUT_PULLUP);
      _buttonFlapStatus.init(PUSH_BUTTON_A);

      AdafruitGfxUtility::setupDisplay(oledDisplay);

      randomSeed(analogRead(ANALOG_READ));
      _highScore = 0;
      _gameState = STATE_TITLE;
    }

    void loop()
    {
      if (_gameEnd)
      {
        return;
      }

      _buttonFlapStatus.update();

      const bool flapPressed{_buttonFlapStatus.getVal() == PushType::Pushed};
      const bool restartPressed{flapPressed};

      if (_gameState == STATE_TITLE)
      {
        oledDisplay.clearDisplay();
        oledDisplay.setTextSize(1);
        oledDisplay.setTextColor(WHITE);
        oledDisplay.setCursor(22, 15);
        oledDisplay.print(F("FLAPPY GAME"));
        oledDisplay.setCursor(18, 32);
        oledDisplay.print(F("Press FLAP btn"));
        if (_highScore > 0)
        {
          oledDisplay.setCursor(28, 50);
          oledDisplay.print(F("BEST:"));
          oledDisplay.print(_highScore);
        }
        oledDisplay.display();
        if (flapPressed)
        {
          resetGame();
        }
        return;
      }

      if (_gameState == STATE_DEAD)
      {
        oledDisplay.clearDisplay();
        oledDisplay.setTextSize(1);
        oledDisplay.setTextColor(WHITE);
        oledDisplay.setCursor(30, 8);
        oledDisplay.print(F("GAME OVER"));
        oledDisplay.setCursor(30, 24);
        oledDisplay.print(F("Score : "));
        oledDisplay.print(_score);
        oledDisplay.setCursor(30, 36);
        oledDisplay.print(F("Best  : "));
        oledDisplay.print(_highScore);
        oledDisplay.setCursor(24, 52);
        oledDisplay.print(F("Press RESTART"));
        oledDisplay.display();
        if (restartPressed)
        {
          resetGame();
        }
        return;
      }

      const unsigned long now = millis();
      if (now - _lastFrame < FRAME_MS)
      {
        return;
      }
      _lastFrame = now;

      if (flapPressed)
      {
        _velY16 = FLAP_VEL;
      }

      _velY16 += GRAVITY;
      _birdY16 += _velY16;

      for (int i = 0; i < NUM_PIPES; i++)
      {
        _pipes[i]._x -= _pipeSpeed;

        if (!_pipes[i]._scored && _pipes[i]._x + PIPE_WIDTH < BIRD_X)
        {
          _pipes[i]._scored = true;
          _score++;
          if (_score > _highScore)
          {
            _highScore = _score;
          }
          _pipeSpeed = min(1, INIT_PIPE_SPEED + _score / 10);
        }

        if (_pipes[i]._x + PIPE_WIDTH < 0)
        {
          int maxX = _pipes[i]._x;
          for (int j = 0; j < NUM_PIPES; j++)
          {
            maxX = max(maxX, _pipes[j]._x);
          }
          spawnPipe(i, maxX + PIPE_SPACING);
        }
      }

      if (checkCollision())
      {
        _gameState = STATE_DEAD;
        return;
      }

      oledDisplay.clearDisplay();
      drawPipes();
      drawBird(_birdY16 / 16, CharacterType::Car);

      oledDisplay.setTextSize(1);
      oledDisplay.setTextColor(WHITE);
      oledDisplay.setCursor(SCREEN_WIDTH - 18, 1);
      oledDisplay.print(_score);

      oledDisplay.display();
    }
  };
}
