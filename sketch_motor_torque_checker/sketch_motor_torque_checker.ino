#include "esp_bt_main.h"
#include "esp_bt.h"
#include "esp_wifi.h"
#include "Preferences.h"

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <EEPROM.h>

Preferences preferences;

RTC_DATA_ATTR int counter = 0;  //RTC coprocessor領域に変数を宣言することでスリープ復帰後も値が保持できる

static constexpr float FPS{15.f};
static constexpr float SEC{1000.f};
static constexpr float ONE_FRAME_MS{(1.f /FPS) * SEC};

int MODE_ADDRESS = 0x0000;

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

  void drawChar(const char* chr, int offsetX, int offsetY, int sizeChar) {

    adaDisplay.fillRect(CHARSIZEX * offsetX, CHARSIZEY * offsetY, CHARSIZEX * sizeChar, CHARSIZEY * 1, BLACK);
    adaDisplay.setCursor(CHARSIZEX * offsetX, CHARSIZEY * offsetY);
    adaDisplay.setTextColor(SSD1306_WHITE); // Draw 'inverse' text
    adaDisplay.print(chr);
  }

  void drawFloat(float value, float offsetX, float offsetY) {

    adaDisplay.fillRect(CHARSIZEX * offsetX, CHARSIZEY * offsetY, CHARSIZEX * 5, CHARSIZEY * 1, BLACK);
    adaDisplay.setCursor(CHARSIZEX * offsetX, CHARSIZEY * offsetY);
    adaDisplay.setTextColor(SSD1306_WHITE); // Draw 'inverse' text
    adaDisplay.print(String(value, 2));
  }

  void drawIntR(int value, float offsetX, float offsetY) {
    int offset{6};
    int numOffset{offset - 1};

    if (value != 0)
    {
      numOffset -= (log10(value) - 1);
    }

    adaDisplay.fillRect(CHARSIZEX * offsetX, CHARSIZEY * offsetY, CHARSIZEX * offset, CHARSIZEY * 1, BLACK);
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

DrawAdafruit drawAdafruit;

class ValueCounter
{
  unsigned long totalValue{0};

  int count{0};

  public:
  void readVolt(int inVolt)
  {
    totalValue += inVolt;
    count += 1;
  }

  unsigned long calcValue()
  {
    if (count == 0)
    {
      return 0;
    }
    unsigned long result{totalValue / count};
    totalValue = 0;
    count = 0;
    return result;
  }

};

class RotateCounter
{
  private:
  static constexpr int THREASHOULD{2048};
  static constexpr float TO_RPM{60.f};

  static constexpr int POLE{14};
  static constexpr int ROT_RATE{POLE / 2};
  static constexpr int GEAR_RATE{24 / 8};
  static constexpr float PULSE_RATE{static_cast<float>(GEAR_RATE) / static_cast<float>(ROT_RATE)};

  unsigned long oldMillis{0};
  unsigned long totalDiffMillis{0};

  bool currentVoltFlag{false};
  bool oldVoltFlag{false};

  long count{0};
  long maxCount{0};

  float rpm{0.f};
  float maxRpm{0.f};

  public:
  void readVolt(int inVolt)
  {
    const int val{inVolt};

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

  void sleep(unsigned long millis)
  {
    const unsigned long diffMillis{millis - oldMillis};
    totalDiffMillis += diffMillis;
  }

  void start(unsigned long millis)
  {
    oldMillis = millis;
  }

  void calcRPM(float& rpm, float& maxRpm)
  {
    if (count > maxCount)
    {
      maxCount = count;
    }

    rpm = count * PULSE_RATE * TO_RPM * (SEC / static_cast<float>(totalDiffMillis));
    maxRpm = maxCount * PULSE_RATE * TO_RPM * (SEC / static_cast<float>(totalDiffMillis));

    count = 0;
    totalDiffMillis = 0;
  }


};

class Controller
{
  static constexpr uint8_t READ_PIN{3};
  static constexpr uint8_t V_READ_PIN{2};
  static constexpr uint8_t I_READ_PIN{4};

private:

  enum class CheckMode {
    FreeCheckMode = 0,
    TorqueCheckMode,
    MaxCheckMode
  };

  static constexpr float ANALOG_READ_SLOPE{3453.f / 2.5f};

  static float calcVValue(unsigned long voltInt)
  {
    return static_cast<float>(voltInt) * (2.f / ANALOG_READ_SLOPE);
  }

  static float calcIValue(unsigned long voltInt, unsigned long offset)
  {
    return (static_cast<float>(voltInt) - offset) * (-20.f / ANALOG_READ_SLOPE);
  }

  struct FreeCheckParam
  {

    static constexpr int TABLE[] = {30, 100, 250};
    static constexpr int TABLE_COUNT{sizeof(TABLE) / sizeof(int)};

    int power{TABLE[0]};
    int powerNum{0};

    RotateCounter rotateCounter;
    ValueCounter iValueCounter;
    ValueCounter vValueCounter;
    
    unsigned int iOffset{0};

    bool nextFlag{false};

    unsigned long misslisDiffA{0};
    unsigned long misslisDiffB{0};

    bool isChangeB(unsigned long millis)
    {
      const unsigned long misslisDiff{millis - misslisDiffB};
      if (misslisDiff > SEC)
      {
        misslisDiffB = millis;
        return true;
      }
      else
      {
        return false;
      }
    }

    bool isChangeA(unsigned long millis)
    {
      const unsigned long misslisDiff{millis - misslisDiffA};
      if (misslisDiff > ONE_FRAME_MS)
      {
        misslisDiffA = millis;
        return true;
      }
      else
      {
        return false;
      }
    }

    void displayB()
    {
      float rpm, maxRpm;
      rotateCounter.calcRPM(rpm, maxRpm);
      drawAdafruit.drawIntR(rpm, 2, 0);
    };

    void displayA()
    {
      float vVoltFloat{Controller::calcVValue(vValueCounter.calcValue())};
      float iVoltFloat{Controller::calcIValue(iValueCounter.calcValue(), iOffset)}; // * (10.f / ANALOG_READ_SLOPE) 

      drawAdafruit.drawFloat(vVoltFloat, 6, 1);
      drawAdafruit.drawFloat(iVoltFloat, 12, 1);

      drawAdafruit.drawInt(power, 0, 1);
    };

    void setNext()
    {
      nextFlag = true;
    }

    void next()
    {
      if (nextFlag)
      {
        powerNum = (powerNum + 1) % TABLE_COUNT;
        power = constrain(TABLE[powerNum], 0, 255);
        nextFlag = false;
      }
    };

  };

  struct TorqueCheckParam
  {
    enum class StateMode {
      SleepMode = 0,
      WaitMode,
      CalcMode,
      MaxStateMode,
    };

    struct RPMCache
    {
      int rpm{0};
      int maxRpm{0};
      float vValue{0};
      float iValue{0};
    };

    float waitTime{2000.f};
    float calcTime{2000.f};

    int power{TABLE[0]};
    int tableIndex{0};
    bool onFlag{false};

    bool blinkFlag{false};

    unsigned long drawMillisBuf{0};
    unsigned long modeMillisBuf{0};

    static constexpr int TABLE[] = {30, 100, 250};
    static constexpr int TABLE_COUNT{sizeof(TABLE) / sizeof(int)};

    RPMCache rpmCahes[TABLE_COUNT];

    StateMode currentMode{StateMode::SleepMode};

    RotateCounter rotateCounter{};
    ValueCounter iValueCounter{};
    ValueCounter vValueCounter{};

    unsigned int iOffset{0};

    void setOn()
    {
      onFlag = true;
    }

    void reset()
    {
      if (onFlag)
      {
        currentMode = StateMode::WaitMode;
        modeMillisBuf = millis();

        tableIndex = 0;

        for (int i = 0; i < TABLE_COUNT; ++i)
        {
          rpmCahes[i].rpm = 0;
          rpmCahes[i].maxRpm = 0;
        }
        onFlag = false;
      }
    };

    void next()
    {
      if (currentMode == StateMode::WaitMode)
      {
        currentMode = StateMode::CalcMode;
      }
      else if (currentMode == StateMode::CalcMode)
      {

        float rpm, maxRpm;
        rotateCounter.calcRPM(rpm, maxRpm);

        rpmCahes[tableIndex].rpm = rpm;
        rpmCahes[tableIndex].maxRpm = maxRpm;
        rpmCahes[tableIndex].vValue = Controller::calcVValue(vValueCounter.calcValue());
        rpmCahes[tableIndex].iValue = Controller::calcIValue(iValueCounter.calcValue(), iOffset);

        if (tableIndex == TABLE_COUNT - 1)
        {
          currentMode = StateMode::SleepMode;
          tableIndex = 0;
          power = constrain(TABLE[0], 0, 255);
        }
        else
        {
          currentMode = StateMode::WaitMode;
          tableIndex = (tableIndex + 1) % TABLE_COUNT;
          power = constrain(TABLE[tableIndex], 0, 255);
        }


      }

    };

    void display()
    {
      blinkFlag = !blinkFlag;

      static char sleepMessage[] = "sleep";
      static char waitMessage[] = "wait";
      static char calcMessage[] = "calc";
      if (currentMode == StateMode::WaitMode)
      {
        int sizeChar = sizeof(waitMessage);
        drawAdafruit.drawChar(waitMessage, 0, 0, sizeChar);
      }
      else if (currentMode == StateMode::CalcMode)
      {
        int sizeChar = sizeof(calcMessage);
        drawAdafruit.drawChar(calcMessage, 0, 0, sizeChar);
      }
      else if (currentMode == StateMode::SleepMode)
      {
        int sizeChar = sizeof(sleepMessage);
        drawAdafruit.drawChar(sleepMessage, 0, 0, sizeChar);
      }

      // drawAdafruit.drawInt(tableIndex, 0, 0);

      static const int OFFSET{1};
      for (int i = 0; i < TABLE_COUNT; ++i)
      {
        drawAdafruit.drawIntR(rpmCahes[i].rpm, 3, i + 1);
        drawAdafruit.drawFloat(rpmCahes[i].vValue, 10, i + 1);
        drawAdafruit.drawFloat(rpmCahes[i].iValue, 16, i + 1);

        const int colIndex{i + OFFSET};
        const int rowIndex{2};
        if (currentMode == StateMode::CalcMode && i == tableIndex)
        {
          if (blinkFlag)
          {
            drawAdafruit.drawChar(">>", 0, colIndex, rowIndex);
          }
          else
          {
            drawAdafruit.drawChar("  ", 0, colIndex, rowIndex);
          }
        }
        else if (currentMode == StateMode::WaitMode && i == tableIndex)
        {
          if (blinkFlag)
          {
            drawAdafruit.drawChar("> ", 0, colIndex, rowIndex);
          }
          else
          {
            drawAdafruit.drawChar("  ", 0, colIndex, rowIndex);
          }
        }
        else
        {
          drawAdafruit.drawChar("  ", 0, colIndex, rowIndex);
        }
        // drawAdafruit.drawInt(rpmCahes[i].maxRpm, 9, i + 1);
      }

    };

    float currentMills()
    {
      if (currentMode == StateMode::WaitMode)
      {
        return waitTime;
      }
      else if (currentMode == StateMode::CalcMode)
      {
        return calcTime;
      }

    };

    bool isChangeMode(float millis)
    {
      const float modeMisslisDiff{millis - modeMillisBuf};
      if (modeMisslisDiff > currentMills())
      {
        modeMillisBuf = millis;
        return true;
      }
      else
      {
        return false;
      }
    }

    bool isDraw(float millis)
    {
      const float drawMillisDiff{millis - drawMillisBuf};
      if (drawMillisDiff > ONE_FRAME_MS)
      {
        drawMillisBuf = millis;
        return true;
      }
      else
      {
        return false;
      }
    }

  };

  static constexpr int WRITE_POWER_PIN{20};
  static constexpr int PUSH_BUTTON1{10};
  static constexpr int PUSH_BUTTON2{9};
  
  unsigned long loopSubMillis{0};

  FreeCheckParam freeCheckParam{};
  TorqueCheckParam torqueCheckParam{};

  int buttonCount{0};

  CheckMode checkMode{CheckMode::FreeCheckMode};

  bool button1Flag{false};
  bool button2Flag{false};
  bool button1OldFlag{false};
  bool button2OldFlag{false};

  unsigned long iOffset{3400};

  void drawMode()
  {
    String modeName;
    if (checkMode == CheckMode::FreeCheckMode)
    {
      modeName = String("mode1");
    }
    else if (checkMode == CheckMode::TorqueCheckMode)
    {
      modeName = String("mode2");
    }

    drawAdafruit.drawChar(modeName.c_str(), 0, 0, modeName.length());
  }

  void changeModeDisplay()
  {
      drawAdafruit.clearDisplay();
      drawMode();
      drawAdafruit.display();

      unsigned long iOffset = calibI();
      freeCheckParam.iOffset = iOffset;
      torqueCheckParam.iOffset = iOffset;

      drawAdafruit.clearDisplay();
  }

  unsigned long calibI()
  {
    ValueCounter iValue{};

    for (int i = 0; i < 100; ++i)
    {
      int iVolt{analogRead(I_READ_PIN)};
      iValue.readVolt(iVolt);
      delay(10);
    }

    return iValue.calcValue();
  }

  void loopSub()
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

    if (!button1Flag && (button1OldFlag != button1Flag))
    {
      checkMode = static_cast<CheckMode>((static_cast<int>(checkMode) + 1) % static_cast<int>(CheckMode::MaxCheckMode));

      // EEPROM.write(MODE_ADDRESS, static_cast<uint8_t>(checkMode));
      int checkModeInt = static_cast<uint8_t>(checkMode);
      preferences.putInt("check_mode", checkModeInt);
      changeModeDisplay();
    }
    button1OldFlag = button1Flag;

    if (!button2Flag && (button2OldFlag != button2Flag))
    {
      if (checkMode == CheckMode::FreeCheckMode)
      {
        freeCheckParam.setNext();
      }
      else if (checkMode == CheckMode::TorqueCheckMode)
      {
        torqueCheckParam.setOn();
      }
    }
    button2OldFlag = button2Flag;

  }

  void loopMain()
  {
    int volt{analogRead(READ_PIN)};
    int vVolt{analogRead(V_READ_PIN)};
    int iVolt{analogRead(I_READ_PIN)};
    if (checkMode == CheckMode::FreeCheckMode)
    {
      freeCheckParam.rotateCounter.readVolt(volt);
      freeCheckParam.vValueCounter.readVolt(vVolt);
      freeCheckParam.iValueCounter.readVolt(iVolt);
    }
    else if (checkMode == CheckMode::TorqueCheckMode)
    {
      torqueCheckParam.rotateCounter.readVolt(volt);
      torqueCheckParam.vValueCounter.readVolt(vVolt);
      torqueCheckParam.iValueCounter.readVolt(iVolt);
    }
  };

public:

  void setup()
  {
    pinMode(READ_PIN, ANALOG);
    pinMode(V_READ_PIN, ANALOG);
    pinMode(I_READ_PIN, ANALOG);

    pinMode(WRITE_POWER_PIN, OUTPUT);
    pinMode(PUSH_BUTTON1, INPUT);
    pinMode(PUSH_BUTTON2, INPUT);

    preferences.begin("my-app", false); 
    int checkModeInt = preferences.getInt("check_mode", 0);

    checkMode = static_cast<CheckMode>(checkModeInt); // EEPROM.read(MODE_ADDRESS));
    drawAdafruit.clearDisplay();
    drawMode();
    drawAdafruit.display();
    delay(500);
    drawAdafruit.clearDisplay();

    changeModeDisplay();
  };

  void loopWhile()
  {
    loopMain();

    const unsigned long tempMills{millis()};
    if (tempMills - loopSubMillis > ONE_FRAME_MS)
    {
      loopSub();
      loopSubMillis = tempMills;
    }

    if (checkMode == CheckMode::FreeCheckMode)
    {
      // const float rpmMisslisDiff{millis() - freeCheckParam.rpmMillisBuf};
      if (freeCheckParam.isChangeB(millis()))
      {
        freeCheckParam.rotateCounter.sleep(millis());
        freeCheckParam.displayB();

        // freeCheckParam.rpmMillisBuf = millis();
        freeCheckParam.rotateCounter.start(millis());
      }

      // const float voltMillisDiff{millis() - freeCheckParam.voltMillisBuf};
      if (freeCheckParam.isChangeA(millis())) // voltMillisDiff > ONE_FRAME_MS)
      {
        freeCheckParam.rotateCounter.sleep(millis());

        freeCheckParam.next();
        freeCheckParam.displayA();

        drawAdafruit.display();

        analogWrite(WRITE_POWER_PIN, freeCheckParam.power);
        // freeCheckParam.voltMillisBuf = millis();

        freeCheckParam.rotateCounter.start(millis());
      }
    }
    else if (checkMode == CheckMode::TorqueCheckMode)
    {

      if (torqueCheckParam.isChangeMode(millis()))
      {
        torqueCheckParam.rotateCounter.sleep(millis());

        torqueCheckParam.next();

        torqueCheckParam.rotateCounter.start(millis());
      }

      if (torqueCheckParam.isDraw(millis()))
      {
        torqueCheckParam.rotateCounter.sleep(millis());

        torqueCheckParam.reset();
        torqueCheckParam.display();
        drawAdafruit.display();
        analogWrite(WRITE_POWER_PIN, torqueCheckParam.power);

        torqueCheckParam.rotateCounter.start(millis());
      }
    }

  };

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