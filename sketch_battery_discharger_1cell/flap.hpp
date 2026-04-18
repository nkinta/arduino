#include <Wire.h>
 #include <Adafruit_GFX.h>
 #include <Adafruit_SSD1306.h>
 
namespace flap {

constexpr int16_t SCREEN_WIDTH{128};
constexpr int16_t SCREEN_HEIGHT{64};
constexpr int16_t OLED_RESET{-1};
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
 
constexpr int16_t BTN_FLAP{D13};
constexpr int16_t BTN_RESTART{D16};
constexpr int16_t ANALOG_READ{D9};
 
 // ゲーム状態
constexpr int16_t STATE_TITLE{0};
constexpr int16_t STATE_PLAYING{1};
constexpr int16_t STATE_DEAD{2};
 
 // 物理 (固定小数点 x16)
constexpr int16_t GRAVITY{3};   // フレームごとの加速 (x16)
constexpr int16_t FLAP_VEL{-44};   // 羽ばたき速度 (x16)
constexpr int16_t BIRD_X{20};
constexpr int16_t BIRD_W{6};
constexpr int16_t BIRD_H{5};
 
 // パイプ
constexpr int16_t PIPE_WIDTH{8};
constexpr int16_t PIPE_GAP{20};  // 通り抜ける隙間
constexpr int16_t PIPE_GAP_MIN{8};   // 上下の壁との最小距離
constexpr int16_t NUM_PIPES{2};
constexpr int16_t PIPE_SPACING{SCREEN_WIDTH / NUM_PIPES};  // 64px間隔
 
constexpr int16_t FRAME_MS{28};   // ~35fps
 
 struct Pipe {
   int x;
   int gapY;    // 隙間の上端Y
   bool scored;
 };
 
 int   gameState;
 int   score;
 int   highScore;
 int   pipeSpeed;  // px/frame
 
 int   birdY16;   // y * 16 (サブピクセル)
 int   velY16;
 
 Pipe  pipes[NUM_PIPES];
 
 bool  prevFlap;
 bool  prevRestart;
 unsigned long lastFrame;
 
 // -----------------------------------------------
 void drawBird(int y) {
   // 胴体
   display.fillRect(BIRD_X, y, BIRD_W, BIRD_H, WHITE);
   // くちばし
   display.drawPixel(BIRD_X + BIRD_W, y + 2, WHITE);
   // 目 (黒で上書き)
   display.drawPixel(BIRD_X + 4, y + 1, BLACK);
   // 羽（下端に1px）
   display.drawPixel(BIRD_X + 2, y + BIRD_H, WHITE);
   display.drawPixel(BIRD_X + 3, y + BIRD_H, WHITE);
 }
 
 void drawPipes() {
   for (int i = 0; i < NUM_PIPES; i++) {
     int px = pipes[i].x;
     int gy = pipes[i].gapY;
     // 上パイプ
     if (gy > 0)
       display.fillRect(px, 0, PIPE_WIDTH, gy, WHITE);
     // 下パイプ
     int botY = gy + PIPE_GAP;
     if (botY < SCREEN_HEIGHT)
       display.fillRect(px, botY, PIPE_WIDTH, SCREEN_HEIGHT - botY, WHITE);
   }
 }
 
 void spawnPipe(int i, int startX) {
   pipes[i].x = startX;
   pipes[i].gapY = PIPE_GAP_MIN + random(SCREEN_HEIGHT - PIPE_GAP - PIPE_GAP_MIN * 2);
   pipes[i].scored = false;
 }
 
 void initPipes() {
   for (int i = 0; i < NUM_PIPES; i++) {
     spawnPipe(i, SCREEN_WIDTH + i * PIPE_SPACING);
   }
 }
 
 bool checkCollision() {
   int birdY = birdY16 / 16;
   // 天井・地面
   if (birdY < 0 || birdY + BIRD_H >= SCREEN_HEIGHT) return true;
   // パイプ
   int birdRight = BIRD_X + BIRD_W;
   for (int i = 0; i < NUM_PIPES; i++) {
     if (birdRight > pipes[i].x && BIRD_X < pipes[i].x + PIPE_WIDTH) {
       if (birdY < pipes[i].gapY || birdY + BIRD_H > pipes[i].gapY + PIPE_GAP)
         return true;
     }
   }
   return false;
 }
 
 void resetGame() {
   birdY16 = (SCREEN_HEIGHT / 2) * 16;
   velY16  = 0;
   score   = 0;
   pipeSpeed = 2;
   initPipes();
   gameState = STATE_PLAYING;
   lastFrame = millis();
 }
 
 // -----------------------------------------------
 void setup() {
   pinMode(BTN_FLAP,    INPUT_PULLUP);
   pinMode(BTN_RESTART, INPUT_PULLUP);
   prevFlap    = false;
   prevRestart = false;
 
   display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
   display.clearDisplay();
   display.display();
 
   randomSeed(3); // analogRead(ANALOG_READ)
   highScore = 0;
   gameState = STATE_TITLE;
 }
 
 void loop() {
   bool flap    = (digitalRead(BTN_FLAP)    == LOW);
   bool restart = (digitalRead(BTN_RESTART) == LOW);
   bool flapPressed    = flap    && !prevFlap;
   bool restartPressed = restart && !prevRestart;
   prevFlap    = flap;
   prevRestart = restart;
 
   // ---- タイトル ----
   if (gameState == STATE_TITLE) {
     display.clearDisplay();
     display.setTextSize(1);
     display.setTextColor(WHITE);
     display.setCursor(22, 15);
     display.print(F("FLAPPY BIRD"));
     display.setCursor(18, 32);
     display.print(F("Press FLAP btn"));
     if (highScore > 0) {
       display.setCursor(28, 50);
       display.print(F("BEST:"));
       display.print(highScore);
     }
     display.display();
     if (flapPressed) resetGame();
     return;
   }
 
   // ---- ゲームオーバー ----
   if (gameState == STATE_DEAD) {
     display.clearDisplay();
     display.setTextSize(1);
     display.setTextColor(WHITE);
     display.setCursor(30, 8);
     display.print(F("GAME OVER"));
     display.setCursor(20, 24);
     display.print(F("Score : "));
     display.print(score);
     display.setCursor(20, 36);
     display.print(F("Best  : "));
     display.print(highScore);
     display.setCursor(12, 52);
     display.print(F("Press RESTART"));
     display.display();
     if (restartPressed) resetGame();
     return;
   }
 
   // ---- プレイ中 ----
   unsigned long now = millis();
   if (now - lastFrame < FRAME_MS) return;
   lastFrame = now;
 
   // 羽ばたき
   if (flapPressed) velY16 = FLAP_VEL;
 
   // 重力
   velY16  += GRAVITY;
   birdY16 += velY16;
 
   // パイプ移動
   for (int i = 0; i < NUM_PIPES; i++) {
     pipes[i].x -= pipeSpeed;
 
     // スコア判定（鳥がパイプを通過した瞬間）
     if (!pipes[i].scored && pipes[i].x + PIPE_WIDTH < BIRD_X) {
       pipes[i].scored = true;
       score++;
       if (score > highScore) highScore = score;
       // 5点ごとに速度アップ（最大4）
       pipeSpeed = min(4, 2 + score / 5);
     }
 
     // 画面外に出たら再生成
     if (pipes[i].x + PIPE_WIDTH < 0) {
       // 最も右にあるパイプの後ろに出す
       int maxX = pipes[i].x;
       for (int j = 0; j < NUM_PIPES; j++) maxX = max(maxX, pipes[j].x);
       spawnPipe(i, maxX + PIPE_SPACING);
     }
   }
 
   // 衝突判定
   if (checkCollision()) {
     gameState = STATE_DEAD;
     return;
   }
 
   // 描画
   display.clearDisplay();
   drawPipes();
   drawBird(birdY16 / 16);
 
   // スコア表示（右上）
   display.setTextSize(1);
   display.setTextColor(WHITE);
   display.setCursor(SCREEN_WIDTH - 18, 1);
   display.print(score);
 
   display.display();
 }

}