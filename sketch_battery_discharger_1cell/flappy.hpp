#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

namespace flappy
{

  constexpr int16_t SCREEN_WIDTH{128};
  constexpr int16_t SCREEN_HEIGHT{64};
  constexpr int16_t OLED_RESET{-1};
  Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

  constexpr int16_t BTN_FLAP1{D13};
  constexpr int16_t BTN_FLAP2{D16};
  constexpr int16_t ANALOG_READ{D9};

  // ゲーム状態
  constexpr int16_t STATE_TITLE{0};
  constexpr int16_t STATE_PLAYING{1};
  constexpr int16_t STATE_DEAD{2};

  // 物理設定（固定小数点 x16）
  constexpr int16_t GRAVITY{3};    // フレームごとの重力加速度（x16）
  constexpr int16_t FLAP_VEL{-40}; // 羽ばたき時の速度（x16）
  constexpr int16_t BIRD_X{20};
  constexpr int16_t BIRD_W{6};
  constexpr int16_t BIRD_H{5};

  constexpr int16_t INIT_PIPE_SPEED{1};

  // パイプ
  constexpr int16_t PIPE_WIDTH{8};
  constexpr int16_t PIPE_GAP{25};    // 上下のパイプ間の隙間
  constexpr int16_t PIPE_GAP_MIN{8}; // 画面端と隙間の最小マージン
  constexpr int16_t NUM_PIPES{2};
  constexpr int16_t PIPE_SPACING{SCREEN_WIDTH / NUM_PIPES}; // 64px 間隔

  constexpr int16_t FRAME_MS{28}; // ~35fps

  class Game
  {

    struct Pipe
    {
      int _x;
      int _gapY; // 隙間の上端Y
      bool _scored;
    };

    bool _gameEnd{false};

    int _gameState;
    int _score;
    int _highScore;
    int _pipeSpeed; // px/frame

    int _birdY16; // y * 16（サブピクセル）
    int _velY16;

    Pipe _pipes[NUM_PIPES];

    bool _prevFlap;
    bool _prevRestart;
    unsigned long _lastFrame;

    void drawBird(int y)
    {
      // 体
      display.fillRect(BIRD_X, y, BIRD_W, BIRD_H, WHITE);
      // くちばし
      display.drawPixel(BIRD_X + BIRD_W, y + 2, WHITE);
      // 目（黒で塗る）
      display.drawPixel(BIRD_X + 4, y + 1, BLACK);
      // 羽の下に 1px 足す
      display.drawPixel(BIRD_X + 2, y + BIRD_H, WHITE);
      display.drawPixel(BIRD_X + 3, y + BIRD_H, WHITE);
    }

    void drawPipes()
    {
      for (int i = 0; i < NUM_PIPES; i++)
      {
        int px = _pipes[i]._x;
        int gy = _pipes[i]._gapY;
        // 上パイプ
        if (gy > 0)
          display.fillRect(px, 0, PIPE_WIDTH, gy, WHITE);
        // 下パイプ
        int botY = gy + PIPE_GAP;
        if (botY < SCREEN_HEIGHT)
          display.fillRect(px, botY, PIPE_WIDTH, SCREEN_HEIGHT - botY, WHITE);
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
      int birdY = _birdY16 / 16;
      // 天井と床の判定
      if (birdY < 0 || birdY + BIRD_H >= SCREEN_HEIGHT)
        return true;
      // パイプとの衝突判定
      int birdRight = BIRD_X + BIRD_W;
      for (int i = 0; i < NUM_PIPES; i++)
      {
        if (birdRight > _pipes[i]._x && BIRD_X < _pipes[i]._x + PIPE_WIDTH)
        {
          if (birdY < _pipes[i]._gapY || birdY + BIRD_H > _pipes[i]._gapY + PIPE_GAP)
            return true;
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
      display.clearDisplay();
      display.display();
    }

  public:
    void displaySleep()
    {
      _gameEnd = true;
      clearDisplay();
      display.ssd1306_command(SSD1306_DISPLAYOFF);
    }

    void setup()
    {
      pinMode(BTN_FLAP1, INPUT_PULLUP);
      pinMode(BTN_FLAP2, INPUT_PULLUP);
      _prevFlap = false;
      _prevRestart = false;

      display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
      display.clearDisplay();
      display.display();

      randomSeed(analogRead(ANALOG_READ)); // analogRead(ANALOG_READ)
      _highScore = 0;
      _gameState = STATE_TITLE;
    }

    void loop()
    {
      if (_gameEnd)
      {
        return;
      }

      bool flap = ((digitalRead(BTN_FLAP1) == LOW) || (digitalRead(BTN_FLAP2) == LOW));
      bool restart = ((digitalRead(BTN_FLAP1) == LOW) || (digitalRead(BTN_FLAP2) == LOW));
      bool flapPressed = flap && !_prevFlap;
      bool restartPressed = restart && !_prevRestart;
      _prevFlap = flap;
      _prevRestart = restart;

      // ---- タイトル ----
      if (_gameState == STATE_TITLE)
      {
        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(WHITE);
        display.setCursor(22, 15);
        display.print(F("FLAPPY BIRD"));
        display.setCursor(18, 32);
        display.print(F("Press FLAP btn"));
        if (_highScore > 0)
        {
          display.setCursor(28, 50);
          display.print(F("BEST:"));
          display.print(_highScore);
        }
        display.display();
        if (flapPressed)
          resetGame();
        return;
      }

      // ---- ゲームオーバー ----
      if (_gameState == STATE_DEAD)
      {
        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(WHITE);
        display.setCursor(30, 8);
        display.print(F("GAME OVER"));
        display.setCursor(30, 24);
        display.print(F("Score : "));
        display.print(_score);
        display.setCursor(30, 36);
        display.print(F("Best  : "));
        display.print(_highScore);
        display.setCursor(24, 52);
        display.print(F("Press RESTART"));
        display.display();
        if (restartPressed)
          resetGame();
        return;
      }

      // ---- プレイ中 ----
      unsigned long now = millis();
      if (now - _lastFrame < FRAME_MS)
        return;
      _lastFrame = now;

      // 羽ばたき
      if (flapPressed)
        _velY16 = FLAP_VEL;

      // 重力
      _velY16 += GRAVITY;
      _birdY16 += _velY16;

      // パイプ移動
      for (int i = 0; i < NUM_PIPES; i++)
      {
        _pipes[i]._x -= _pipeSpeed;

        // スコア加算: 鳥がパイプを通過した瞬間
        if (!_pipes[i]._scored && _pipes[i]._x + PIPE_WIDTH < BIRD_X)
        {
          _pipes[i]._scored = true;
          _score++;
          if (_score > _highScore)
            _highScore = _score;
          // 5 点ごとに速度アップしたいが、最大 4 程度
          _pipeSpeed = min(1, INIT_PIPE_SPEED + _score / 10);
        }

        // 画面外に出たら右端へ再配置
        if (_pipes[i]._x + PIPE_WIDTH < 0)
        {
          // 最も右にあるパイプの後ろへ出す
          int maxX = _pipes[i]._x;
          for (int j = 0; j < NUM_PIPES; j++)
            maxX = max(maxX, _pipes[j]._x);
          spawnPipe(i, maxX + PIPE_SPACING);
        }
      }

      // 衝突判定
      if (checkCollision())
      {
        _gameState = STATE_DEAD;
        return;
      }

      // 描画
      display.clearDisplay();
      drawPipes();
      drawBird(_birdY16 / 16);

      // スコア表示（右上）
      display.setTextSize(1);
      display.setTextColor(WHITE);
      display.setCursor(SCREEN_WIDTH - 18, 1);
      display.print(_score);

      display.display();
    }
  };
}
