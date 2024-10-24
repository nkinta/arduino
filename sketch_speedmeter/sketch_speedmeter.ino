
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <MsTimer2.h>
#include <EEPROM.h>

#define DEBUG_ON 0

enum MotorStatus{
    STAT_DEFALUT,
    STAT_SLEEP
};

MotorStatus currentStatus = STAT_DEFALUT;

const int SENSOR_READ_VOLT = 7;

const float FPS = 60.f;
const float ONE_FRAME_MS = (1.f / FPS) * 1000.f;

const int SENSOR_IN_VOLT = 4;



int CHARSIZEX = 6;
int CHARSIZEY = 8;
int TEXT_SIZE = 1;

long sumAnalogRead0 = 0;
int readCount = 0;

int settingMode = 0;

const float VOLT5 = 4.90;

const float ROLLER_SIZE = 18.9;

const int THRESHOULD_INPUT = 512;

bool activeFlag = true;
bool suspendFlag = false;
bool reflectFlag = false;

int sameCount = 0;
int cachedSameCount = 0;

long changeCount = 0;

const int TO_SLEEP_COUNT = 16;
int toSleepCount = 0;

float maxKmSpeed = 0.f;

float REG = 0.36;

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     4 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void setup() {

  activeFlag = true;

  pinMode(SENSOR_READ_VOLT, INPUT);

  pinMode(SENSOR_IN_VOLT, OUTPUT);

  Serial.begin(9600);

  setupDisplay();
  
  digitalWrite(SENSOR_IN_VOLT, HIGH);

  readVolt();
  changeCount = 0;

  Serial.write("set up end");
}

void setupDisplay(void) {

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3C for 128x32
    Serial.println(F("SSD1306 allocation failed"));
    for (;;); // Don't proceed, loop forever
  }

  // Show initial display buffer contents on the screen --
  // the library initializes this with an Adafruit splash screen.


  // Clear the buffer
  display.clearDisplay();
  drawHello();
  display.display();
  
  display.setTextSize(TEXT_SIZE);
}

void updateMain() {

  drawVoltAll();
}

void drawHello() {
  drawChar("Hello", 0, 0);
}

void readVolt() {

  float analogReadValue0 = analogRead(SENSOR_READ_VOLT);

  readCount++;
  sumAnalogRead0 += analogReadValue0;

  if (THRESHOULD_INPUT < analogReadValue0)
  {
    bool newFlag = true;
    if (reflectFlag != newFlag)
    {
      changeCount++;
#if DEBUG_ON
      cachedSameCount = sameCount;
      sameCount = 0;
    }
    else
    {
      sameCount++;
#endif
    }
    reflectFlag = newFlag;
  }
  else
  {
    bool newFlag = false;
    if (reflectFlag != newFlag)
    {
      changeCount++;
#if DEBUG_ON
      cachedSameCount = sameCount;
      sameCount = 0;
    }
    else
    {
      sameCount++;
#endif
    }
    reflectFlag = newFlag;
  }
}

void drawSpeed() {

  static const int DRAW_ROW = 0;
#if DEBUG_ON
  static const int DRAW_ROW = 2;
#endif

  const static int changeCountPerRot = 2.f;
  const static int fpsRate = (60 / changeCountPerRot);
#if DEBUG_ON
  drawInt(changeCount * fpsRate, 0, 1);
  drawInt(cachedSameCount, 6, 1);
#endif
  const static float TO_KM_SPEED = ROLLER_SIZE * PI * 60.f * 60.f * 0.001 * 0.001 * (1.f / changeCountPerRot);
  float kmSpeed = float(changeCount) * TO_KM_SPEED;
  drawFloat(kmSpeed , 0, DRAW_ROW);
  drawChar("km/h", 5, DRAW_ROW);

  if (kmSpeed > maxKmSpeed)
  {
    maxKmSpeed = kmSpeed;
    drawChar("max", 0, DRAW_ROW + 1);
    drawFloat(maxKmSpeed , 4, DRAW_ROW + 1);
    drawChar("km/h", 9, DRAW_ROW + 1);
  }

  changeCount = 0;

#if DEBUG_ON
  drawVoltAll();
#endif

}

void drawVoltAll() {
  static const float toVolt = VOLT5 / 1024.f;
  static const float toA = 1.f / REG;

  if (readCount > 0)
  {
    const float volt0 = constrain(((float)sumAnalogRead0 / (float)readCount) * toVolt, 0.f, 1000.f);

    drawFloat(volt0, 0, 0);
    drawChar("V", 4, 0);

    sumAnalogRead0 = 0.f;
    readCount = 0;
  }
}

void drawInt(int isButton, float offsetX, float offsetY) {
  display.fillRect(CHARSIZEX * offsetX, CHARSIZEY * offsetY, CHARSIZEX * 5, CHARSIZEY * 1, BLACK);
  display.setCursor(CHARSIZEX * offsetX, CHARSIZEY * offsetY);
  display.setTextColor(SSD1306_WHITE); // Draw 'inverse' text
  display.println(isButton);
}

void drawFloat(float volt, float offsetX, float offsetY) {

  display.fillRect(CHARSIZEX * offsetX, CHARSIZEY * offsetY, CHARSIZEX * 5, CHARSIZEY * 1, BLACK);
  display.setCursor(CHARSIZEX * offsetX, CHARSIZEY * offsetY);
  display.setTextColor(SSD1306_WHITE); // Draw 'inverse' text
  display.println(String(volt, 2));
}

void drawChar(char* chr, float offsetX, float offsetY) {

  display.fillRect(CHARSIZEX * offsetX, CHARSIZEY * offsetY, CHARSIZEX * 1, CHARSIZEY * 1, BLACK);
  display.setCursor(CHARSIZEX * offsetX, CHARSIZEY * offsetY);
  display.setTextColor(SSD1306_WHITE); // Draw 'inverse' text
  display.println(chr);

}

void checkStatus()
{
  if (changeCount == 0)
  {
    ++toSleepCount;
  }
  else
  {
    toSleepCount = 0;
  }

  if (toSleepCount > TO_SLEEP_COUNT)
  {
    digitalWrite(SENSOR_IN_VOLT, LOW);
    drawChar("Sleep", 0, 2);
    currentStatus = STAT_SLEEP;
  }
}

void loop() {

  static unsigned char count = 0;
  static unsigned long millis_buf = 0;

  static unsigned long volt_millis_buf = 0;
  static unsigned long frame_millis_buf = 0;
  static unsigned long suspend_millis_buf = 0;


  while (true)
  {

    readVolt();

    //　現在の経過時間-この待機処理を通過した時間が1000(ms)になるまで待機
    if ((millis() - volt_millis_buf) > ONE_FRAME_MS * FPS)
    {
      checkStatus();

      if (currentStatus != STAT_SLEEP)
      {
        drawSpeed();
      }

      display.display();

      volt_millis_buf = millis();
    }

    /*
      if ((millis() - frame_millis_buf) > ONE_FRAME_MS)
      {
      frame_millis_buf = millis();
      }
    */
  }


}
