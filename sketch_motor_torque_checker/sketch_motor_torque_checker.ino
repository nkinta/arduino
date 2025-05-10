#include "esp_bt_main.h"
#include "esp_bt.h"
#include "esp_wifi.h"

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

RTC_DATA_ATTR int counter = 0;  //RTC coprocessor領域に変数を宣言することでスリープ復帰後も値が保持できる

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

  void drawChar(char* chr, int offsetX, int offsetY, int sizeChar) {

    adaDisplay.fillRect(CHARSIZEX * offsetX, CHARSIZEY * offsetY, CHARSIZEX * sizeChar, CHARSIZEY * 1, BLACK);
    adaDisplay.setCursor(CHARSIZEX * offsetX, CHARSIZEY * offsetY);
    adaDisplay.setTextColor(SSD1306_WHITE); // Draw 'inverse' text
    adaDisplay.println(chr);
  }

  void drawFloat(float volt, float offsetX, float offsetY) {

    adaDisplay.fillRect(CHARSIZEX * offsetX, CHARSIZEY * offsetY, CHARSIZEX * 5, CHARSIZEY * 1, BLACK);
    adaDisplay.setCursor(CHARSIZEX * offsetX, CHARSIZEY * offsetY);
    adaDisplay.setTextColor(SSD1306_WHITE); // Draw 'inverse' text
    adaDisplay.println(String(volt, 2));
  }

  void drawInt(int isButton, float offsetX, float offsetY) {
    adaDisplay.fillRect(CHARSIZEX * offsetX, CHARSIZEY * offsetY, CHARSIZEX * 5, CHARSIZEY * 1, BLACK);
    adaDisplay.setCursor(CHARSIZEX * offsetX, CHARSIZEY * offsetY);
    adaDisplay.setTextColor(SSD1306_WHITE); // Draw 'inverse' text
    adaDisplay.println(isButton);
  }

  void display()
  {
    adaDisplay.display();
  }
};

DrawAdafruit drawAdafruit;

class Counter
{
  private:
  static constexpr int THREASHOULD{2048};
  static constexpr uint8_t READ_PIN{3};

  bool currentVoltFlag{false};

  bool oldVoltFlag{false};

  int count{0};

  int maxCount{0};

  public:

  void setup()
  {
    pinMode(READ_PIN, INPUT);
  }

  void readVolt()
  {
    const int val{analogRead(READ_PIN)};

    oldVoltFlag = currentVoltFlag;
    if (val > THREASHOULD + 256)
    {
      currentVoltFlag = true;
    }
    else if (val < THREASHOULD - 256)
    {
      currentVoltFlag = false;
    }

    if (oldVoltFlag != currentVoltFlag && currentVoltFlag == true)
    {
      count++;
    }
  }

  void getCounter(int& outCount, int& outMaxCount)
  {
    outCount = count; 

    if (count > maxCount)
    {
      maxCount = count;
    }

    outMaxCount = maxCount;

    count = 0;
  }
};

class Controller
{

private:

  static constexpr float FPS{30.f};

  static constexpr float ONE_FRAME_MS{(1.f / FPS) * 1000.f};

  static constexpr int WRITE_POWER_PIN{2};
  static constexpr int PUSH_BUTTON1{10};
  static constexpr int PUSH_BUTTON2{9};

  static constexpr int TABLE[] = {30, 60, 80, 100, 120, 140, 160, 180, 200, 220, 240, 250};

  static constexpr int TABLE_COUNT{sizeof(TABLE) / sizeof(int)};

  static constexpr int POLE{14};

  static constexpr int ROT_RATE{POLE / 2};

  static constexpr int GEAR_RATE{24 / 8};

  static constexpr float PULSE_RATE{static_cast<float>(GEAR_RATE) / static_cast<float>(ROT_RATE)};

  int buttonCount{0};

  Counter counter;

  unsigned long voltMillisBuf{0};

  unsigned long rpmMillisBuf{0};

  bool button1Flag{false};
  bool button2Flag{false};
  bool button2OldFlag{false};

  bool button2ReleaseFlag{false};

  int powerNum{0};
  int power{TABLE[0]};

  void readVolt()
  {
    int val1 = digitalRead(PUSH_BUTTON1);
    if (val1 == LOW)
    {
      button1Flag = true;
    }
    else
    {
      button1Flag = false;
    }

    int val2 = digitalRead(PUSH_BUTTON2);
    if (val2 == LOW)
    {
      button2Flag = true;
    }
    else
    {
      button2Flag = false;
    }

    counter.readVolt();
  }

public:

  void setup()
  {
    counter.setup();
    pinMode(WRITE_POWER_PIN, OUTPUT);
    pinMode(PUSH_BUTTON1, INPUT);
    pinMode(PUSH_BUTTON2, INPUT);

    char rpmMessage[] = "rpm";
    int sizeChar = sizeof(rpmMessage);
    drawAdafruit.drawChar(rpmMessage, 5, 0, sizeChar);
    drawAdafruit.drawChar(rpmMessage, 14, 0, sizeChar);

  }

  void controlMain()
  {
    if (!button2Flag && (button2OldFlag != button2Flag))
    {
      powerNum = (powerNum + 1) % TABLE_COUNT;
      power = constrain(TABLE[powerNum], 0, 255);
    }

    button2OldFlag = button2Flag;
  }

  void display()
  {
    if (button1Flag)
    {
      char message[] = "On";
      int sizeChar = sizeof(message);
      drawAdafruit.drawChar(message, 0, 1, sizeChar);
    }
    else
    {
      char message[] = "Off";
      int sizeChar = sizeof(message);
      drawAdafruit.drawChar(message, 0, 1, sizeChar);
    }

    drawAdafruit.drawInt(power, 0, 2);

  }

  void loopWhile()
  {
    readVolt();

    if ((millis() - rpmMillisBuf) > 1000.f)
    {

      int count{0};
      int maxCount{0};
      counter.getCounter(count, maxCount);

      const int rpm{count * PULSE_RATE * 60.f};
      const int maxRpm{maxCount * PULSE_RATE * 60.f};

      drawAdafruit.drawInt(rpm, 0, 0);
      drawAdafruit.drawInt(maxRpm, 9, 0);

      rpmMillisBuf = millis();
    }

    if ((millis() - voltMillisBuf) > ONE_FRAME_MS)
    {
      controlMain();

      display();

      drawAdafruit.display();

      analogWrite(WRITE_POWER_PIN, power);

      voltMillisBuf = millis();
    }

  }
};

Controller controller;

void drawStartMessage()
{
  char message[] = "torque checker!";
  int sizeChar = sizeof(message);

  drawAdafruit.drawChar(message, 0, 0, sizeChar);
  drawAdafruit.display();
}

void setup() {

  delay(500);

  Serial.begin(9600);
  Serial.printf("start\r\n");

  drawAdafruit.setupDisplay();

  drawStartMessage();
  // delay(1000);
  drawAdafruit.clearDisplay();

  controller.setup();

}



void go_to_sleep() {
    // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.printf("Cause:%d\r\n", esp_sleep_get_wakeup_cause());
  wakeup_cause_print();
  Serial.printf("Counter:%d\r\n", counter++);
  Serial.println("Start!!");
  for (int i = 0; i < 15; i++) {
    Serial.printf("%d analog :%d\n", i, analogRead(0));
    delay(1000);
  }
  Serial.println("Go to DeepSleep!!");
  esp32c3_deepsleep(10, 2);  //10秒間スリープ スリープ中にGPIO2がHIGHになったら目覚める
}

void loop() {

  while(true)
  {
    controller.loopWhile();
  }

  return;
  // put your main code here, to run repeatedly:
}

void esp32c3_deepsleep(uint8_t sleep_time, uint8_t wakeup_gpio) {
  // スリープ前にwifiとBTを明示的に止めないとエラーになる
  esp_bluedroid_disable();
  esp_bt_controller_disable();
  esp_wifi_stop();
  esp_deep_sleep_enable_gpio_wakeup(BIT(wakeup_gpio), ESP_GPIO_WAKEUP_GPIO_HIGH);  // 設定したIOピンがHIGHになったら目覚める
  esp_deep_sleep(1000 * 1000 * sleep_time);
}

void wakeup_cause_print() {
  esp_sleep_wakeup_cause_t wakeup_reason;
  wakeup_reason = esp_sleep_get_wakeup_cause();
  switch (wakeup_reason) {
    case 0: Serial.println("Wakeup caused by ESP_SLEEP_WAKEUP_UNDEFINED"); break;
    case 1: Serial.println("Wakeup caused by ESP_SLEEP_WAKEUP_ALL"); break;
    case 2: Serial.println("Wakeup caused by ESP_SLEEP_WAKEUP_EXT0"); break;
    case 3: Serial.println("Wakeup caused by ESP_SLEEP_WAKEUP_EXT1"); break;
    case 4: Serial.println("Wakeup caused by ESP_SLEEP_WAKEUP_TIMER"); break;
    case 5: Serial.println("Wakeup caused by ESP_SLEEP_WAKEUP_TOUCHPAD"); break;
    case 6: Serial.println("Wakeup caused by ESP_SLEEP_WAKEUP_ULP"); break;
    case 7: Serial.println("Wakeup caused by ESP_SLEEP_WAKEUP_GPIO"); break;
    case 8: Serial.println("Wakeup caused by ESP_SLEEP_WAKEUP_UART"); break;
    case 9: Serial.println("Wakeup caused by ESP_SLEEP_WAKEUP_WIFI"); break;
    case 10: Serial.println("Wakeup caused by ESP_SLEEP_WAKEUP_COCPU"); break;
    case 11: Serial.println("Wakeup caused by ESP_SLEEP_WAKEUP_COCPU_TRAP_TRIG"); break;
    case 12: Serial.println("Wakeup caused by ESP_SLEEP_WAKEUP_BT"); break;
    default: Serial.println("Wakeup was not caused by deep sleep"); break;
  }
}