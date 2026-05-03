 #include <Wire.h>
 #include <Adafruit_GFX.h>
 #include <Adafruit_SSD1306.h>
 
 #define SCREEN_WIDTH 128
 #define SCREEN_HEIGHT 64
 #define OLED_RESET -1
 Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
 
 // ボタンピン
 #define BTN_LEFT   2
 #define BTN_RIGHT  3
 #define BTN_DOWN   4
 #define BTN_ROT_CW 5   // 時計回り回転
 #define BTN_ROT_CCW 6  // 反時計回り回転
 #define BTN_DROP   7   // ハードドロップ
 
 // フィールドサイズ（ブロック単位）
 #define FIELD_W 10
 #define FIELD_H 16
 #define BLOCK_SIZE 4
 
 // フィールド描画オフセット（左寄せ）
 #define FIELD_X 0
 #define FIELD_Y 0
 
 // スコア表示エリア（右側）
 #define SCORE_X 44
 
 // テトロミノ定義（4x4、各4回転）
 // 0=I, 1=O, 2=T, 3=S, 4=Z, 5=J, 6=L
 const uint8_t PIECES[7][4][4] PROGMEM = {
   // I
   { {0b0000, 0b1111, 0b0000, 0b0000},
     {0b0100, 0b0100, 0b0100, 0b0100},
     {0b0000, 0b1111, 0b0000, 0b0000},
     {0b0100, 0b0100, 0b0100, 0b0100} },
   // O
   { {0b0110, 0b0110, 0b0000, 0b0000},
     {0b0110, 0b0110, 0b0000, 0b0000},
     {0b0110, 0b0110, 0b0000, 0b0000},
     {0b0110, 0b0110, 0b0000, 0b0000} },
   // T
   { {0b0100, 0b1110, 0b0000, 0b0000},
     {0b0100, 0b0110, 0b0100, 0b0000},
     {0b0000, 0b1110, 0b0100, 0b0000},
     {0b0100, 0b1100, 0b0100, 0b0000} },
   // S
   { {0b0110, 0b1100, 0b0000, 0b0000},
     {0b0100, 0b0110, 0b0010, 0b0000},
     {0b0110, 0b1100, 0b0000, 0b0000},
     {0b0100, 0b0110, 0b0010, 0b0000} },
   // Z
   { {0b1100, 0b0110, 0b0000, 0b0000},
     {0b0010, 0b0110, 0b0100, 0b0000},
     {0b1100, 0b0110, 0b0000, 0b0000},
     {0b0010, 0b0110, 0b0100, 0b0000} },
   // J
   { {0b0100, 0b0100, 0b1100, 0b0000},
     {0b1000, 0b1110, 0b0000, 0b0000},
     {0b0110, 0b0100, 0b0100, 0b0000},
     {0b0000, 0b1110, 0b0010, 0b0000} },
   // L
   { {0b0100, 0b0100, 0b0110, 0b0000},
     {0b0000, 0b1110, 0b1000, 0b0000},
     {0b1100, 0b0100, 0b0100, 0b0000},
     {0b0010, 0b1110, 0b0000, 0b0000} },
 };
 
 uint8_t field[FIELD_H][FIELD_W];
 
 int curX, curY, curType, curRot;
 uint32_t score;
 int level;
 bool gameOver;
 unsigned long lastFall;
 unsigned long fallInterval;
 
 // ボタン状態（チャタリング対策）
 bool prevBtn[6];
 unsigned long btnTime[6];
 #define DEBOUNCE_MS 50
 
 bool btnPressed(int idx, int pin) {
   bool cur = (digitalRead(pin) == LOW);
   if (cur != prevBtn[idx]) {
     if (millis() - btnTime[idx] > DEBOUNCE_MS) {
       btnTime[idx] = millis();
       prevBtn[idx] = cur;
       return cur; // 押した瞬間だけ true
     }
   }
   return false;
 }
 
 void initGame() {
   memset(field, 0, sizeof(field));
   score = 0;
   level = 1;
   gameOver = false;
   fallInterval = 800;
   spawnPiece();
 }
 
 void spawnPiece() {
   curType = random(7);
   curRot = 0;
   curX = 3;
   curY = 0;
   if (!canPlace(curX, curY, curType, curRot)) {
     gameOver = true;
   }
 }
 
 bool canPlace(int x, int y, int t, int r) {
   for (int row = 0; row < 4; row++) {
     uint8_t line = pgm_read_byte(&PIECES[t][r][row]);
     for (int col = 0; col < 4; col++) {
       if (line & (0x08 >> col)) {
         int fx = x + col;
         int fy = y + row;
         if (fx < 0 || fx >= FIELD_W || fy >= FIELD_H) return false;
         if (fy >= 0 && field[fy][fx]) return false;
       }
     }
   }
   return true;
 }
 
 void placePiece() {
   for (int row = 0; row < 4; row++) {
     uint8_t line = pgm_read_byte(&PIECES[curType][curRot][row]);
     for (int col = 0; col < 4; col++) {
       if (line & (0x08 >> col)) {
         int fx = curX + col;
         int fy = curY + row;
         if (fy >= 0) field[fy][fx] = curType + 1;
       }
     }
   }
   clearLines();
   spawnPiece();
 }
 
 void clearLines() {
   int cleared = 0;
   for (int row = FIELD_H - 1; row >= 0; row--) {
     bool full = true;
     for (int col = 0; col < FIELD_W; col++) {
       if (!field[row][col]) { full = false; break; }
     }
     if (full) {
       for (int r = row; r > 0; r--)
         memcpy(field[r], field[r-1], FIELD_W);
       memset(field[0], 0, FIELD_W);
       cleared++;
       row++; // 同じ行を再チェック
     }
   }
   if (cleared > 0) {
     const int pts[] = {0, 100, 300, 500, 800};
     score += (uint32_t)pts[min(cleared, 4)] * level;
     level = 1 + (int)(score / 1000);
     fallInterval = max(100UL, 800UL - (unsigned long)(level - 1) * 70);
   }
 }
 
 void drawField() {
   // 枠
   display.drawRect(FIELD_X, FIELD_Y, FIELD_W * BLOCK_SIZE + 2, FIELD_H * BLOCK_SIZE + 2, WHITE);
 
   // フィールド
   for (int row = 0; row < FIELD_H; row++) {
     for (int col = 0; col < FIELD_W; col++) {
       if (field[row][col]) {
         display.fillRect(FIELD_X + 1 + col * BLOCK_SIZE,
                          FIELD_Y + 1 + row * BLOCK_SIZE,
                          BLOCK_SIZE - 1, BLOCK_SIZE - 1, WHITE);
       }
     }
   }
 
   // 現在のピース
   for (int row = 0; row < 4; row++) {
     uint8_t line = pgm_read_byte(&PIECES[curType][curRot][row]);
     for (int col = 0; col < 4; col++) {
       if (line & (0x08 >> col)) {
         int fx = curX + col;
         int fy = curY + row;
         if (fy >= 0) {
           display.fillRect(FIELD_X + 1 + fx * BLOCK_SIZE,
                            FIELD_Y + 1 + fy * BLOCK_SIZE,
                            BLOCK_SIZE - 1, BLOCK_SIZE - 1, WHITE);
         }
       }
     }
   }
 }
 
 void drawScore() {
   display.setTextSize(1);
   display.setTextColor(WHITE);
   display.setCursor(SCORE_X, 0);
   display.print(F("SCR"));
   display.setCursor(SCORE_X, 10);
   display.print(score);
   display.setCursor(SCORE_X, 25);
   display.print(F("LVL"));
   display.setCursor(SCORE_X, 35);
   display.print(level);
 }
 
 void drawGameOver() {
   display.clearDisplay();
   display.setTextSize(1);
   display.setTextColor(WHITE);
   display.setCursor(20, 20);
   display.print(F("GAME OVER"));
   display.setCursor(10, 35);
   display.print(F("SCR:"));
   display.print(score);
   display.display();
 }
 
 void setup() {
   pinMode(BTN_LEFT,    INPUT_PULLUP);
   pinMode(BTN_RIGHT,   INPUT_PULLUP);
   pinMode(BTN_DOWN,    INPUT_PULLUP);
   pinMode(BTN_ROT_CW,  INPUT_PULLUP);
   pinMode(BTN_ROT_CCW, INPUT_PULLUP);
   pinMode(BTN_DROP,    INPUT_PULLUP);
 
   memset(prevBtn, 0, sizeof(prevBtn));
   memset(btnTime, 0, sizeof(btnTime));
 
   display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
   display.clearDisplay();
   display.display();
 
   randomSeed(analogRead(A0));
   initGame();
   lastFall = millis();
 }
 
 void loop() {
   if (gameOver) {
     drawGameOver();
     // いずれかのボタンでリスタート
     for (int i = 0; i < 6; i++) {
       int pins[] = {BTN_LEFT, BTN_RIGHT, BTN_DOWN, BTN_ROT_CW, BTN_ROT_CCW, BTN_DROP};
       if (btnPressed(i, pins[i])) { initGame(); lastFall = millis(); }
     }
     return;
   }
 
   // 入力
   if (btnPressed(0, BTN_LEFT)) {
     if (canPlace(curX - 1, curY, curType, curRot)) curX--;
   }
   if (btnPressed(1, BTN_RIGHT)) {
     if (canPlace(curX + 1, curY, curType, curRot)) curX++;
   }
   if (btnPressed(2, BTN_DOWN)) {
     if (canPlace(curX, curY + 1, curType, curRot)) curY++;
     else placePiece();
     lastFall = millis();
   }
   if (btnPressed(3, BTN_ROT_CW)) {
     int nr = (curRot + 1) % 4;
     if (canPlace(curX, curY, curType, nr)) curRot = nr;
   }
   if (btnPressed(4, BTN_ROT_CCW)) {
     int nr = (curRot + 3) % 4;
     if (canPlace(curX, curY, curType, nr)) curRot = nr;
   }
   if (btnPressed(5, BTN_DROP)) {
     while (canPlace(curX, curY + 1, curType, curRot)) curY++;
     placePiece();
     lastFall = millis();
   }
 
   // 自動落下
   if (millis() - lastFall >= fallInterval) {
     lastFall = millis();
     if (canPlace(curX, curY + 1, curType, curRot)) {
       curY++;
     } else {
       placePiece();
     }
   }
 
   // 描画
   display.clearDisplay();
   drawField();
   drawScore();
   display.display();
 }
