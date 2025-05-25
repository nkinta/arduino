#include "esp_bt_main.h"
#include "esp_bt.h"
#include "esp_wifi.h"
#include "Preferences.h"

#include <EEPROM.h>

#include "display/draw_adafruit.h"

Preferences preferences;

RTC_DATA_ATTR int counter = 0;  //RTC coprocessor領域に変数を宣言することでスリープ復帰後も値が保持できる

static constexpr float FPS{15.f};
static constexpr float SEC{1000.f};
static constexpr float ONE_FRAME_MS{(1.f /FPS) * SEC};

int MODE_ADDRESS = 0x0000;

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

  void reset()
  {
    totalValue = 0;
    count = 0;
  }

  unsigned long calcValue()
  {
    if (count == 0)
    {
      return 0;
    }
    unsigned long result{totalValue / count};
    reset();
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

  float calcRPM()
  {
    float result{count * PULSE_RATE * TO_RPM * (SEC / static_cast<float>(totalDiffMillis))};
    reset();

    return result;
  }

  void reset()
  {
    count = 0;
    totalDiffMillis = 0;
  }

};

struct VoltageMapping
{
  struct VoltPair
  {
    int input{0.f};
    float volt{0};
  };

  const std::vector<VoltPair> mappingData{{29, 0.f}, {327, 0.5f}, {658, 1.f}, {989, 1.5f}, {1326, 2.f}, {1671, 2.5f}, {2011, 3.f}, {2359, 3.5f}, {2698, 4.f}, {3042, 4.5f}, {3405, 5.f}};

  float getVoltage(int input)
  {
    const VoltPair* before{0};
    for (const VoltPair& current: mappingData)
    {
      if (before)
      {
        if (before->input < input && input <= current.input)
        {
          return before->volt + ((current.volt - before->volt) / static_cast<float>(current.input - before->input)) * (input - before->input);
        }
      }

      before = &current;
    }
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

  static float calcVValue(int voltInt)
  {
    static VoltageMapping voltageMapping;
    // return static_cast<float>(voltInt) * (2.f / ANALOG_READ_SLOPE);
    return voltageMapping.getVoltage(voltInt);
  }

  static float calcIValue(int voltInt, int offset)
  {
    static VoltageMapping voltageMapping;    
    // return (static_cast<float>(voltInt) - offset) * (-20.f / ANALOG_READ_SLOPE);
    return 10.f * voltageMapping.getVoltage(-voltInt + offset);
  }

  struct RPMCache
  {
    int rpm{0};
    float vValue{0};
    float iValue{0};
    void reset()
    {
      rpm = 0;
      vValue = 0.f;
      iValue = 0.f;
    }
  };

  static const char* getDrawArrow(const bool blinkFlag, int blinkType)
  {
    static char arrow1Message[] = ">";
    static char arrow2Message[] = ">>";
    static char emptyMessage[] = "  ";
    const char* arrowMessage = emptyMessage;
    if (blinkType == 2)
    {
      if (blinkFlag)
      {
        arrowMessage = arrow2Message;
      }
      else
      {
        arrowMessage = emptyMessage;
      }
    }
    else if (blinkType == 1)
    {
      if (blinkFlag)
      {
        arrowMessage = arrow1Message;
      }
      else
      {
        arrowMessage = emptyMessage;
      }
    }

    return arrowMessage;
  }

  struct FreeCheckParam
  {

    static constexpr int TABLE[] = {30, 100, 250};
    static constexpr int TABLE_COUNT{sizeof(TABLE) / sizeof(int)};

    int powerNum{0};

    RotateCounter rotateCounter;
    ValueCounter iValueCounter;
    ValueCounter vValueCounter;
    
    unsigned int iOffset{0};

    bool nextFlag{false};

    unsigned long misslisDiffA{0};
    unsigned long misslisDiffB{0};

    static constexpr int RPM_CACHE_COUNT{2};

    RPMCache rpmCahes[RPM_CACHE_COUNT]{};

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

    int getPower()
    {
      int power = constrain(TABLE[powerNum], 0, 255);
      return power;
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
      RPMCache& currentCache{rpmCahes[0]};
      currentCache.rpm = rotateCounter.calcRPM();
      currentCache.vValue = Controller::calcVValue(vValueCounter.calcValue());
      currentCache.iValue = Controller::calcIValue(iValueCounter.calcValue(), iOffset);

      RPMCache& maxCache{rpmCahes[1]};
      if (currentCache.rpm > maxCache.rpm)
      {
        maxCache = currentCache;
      }

      static const int OFFSET{1};
      static const int ROW_INDEX{2};
      for (int i = 0; i < RPM_CACHE_COUNT; ++i)
      {
        drawAdafruit.drawIntR(rpmCahes[i].rpm, 3, i + 1);
        drawAdafruit.drawFloat(rpmCahes[i].vValue, 10, i + 1);
        drawAdafruit.drawFloat(rpmCahes[i].iValue, 15, i + 1);
      }
    };

    void displayA()
    {
      drawAdafruit.drawChar("T", 0, 0, 1);
      drawAdafruit.drawInt(powerNum, 1, 0);
    };

    void setNext()
    {
      nextFlag = true;
    }

    void reset()
    {
      for (int i = 0; i < RPM_CACHE_COUNT; ++i)
      {
        rpmCahes[i].reset();
      }
      powerNum = 0;
    }

    void next()
    {
      if (nextFlag)
      {
        powerNum = (powerNum + 1) % TABLE_COUNT;
        for (int i = 0; i < RPM_CACHE_COUNT; ++i)
        {
          rpmCahes[i].reset();
        }
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

    float waitTime{2000.f};
    float calcTime{2000.f};

    bool onFlag{false};

    bool blinkFlag{false};

    unsigned long drawMillisBuf{0};
    unsigned long modeMillisBuf{0};

    int tableIndex{0};
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

    void start()
    {
      currentMode = StateMode::WaitMode;
      tableIndex = 0;
      modeMillisBuf = millis();

      for (int i = 0; i < TABLE_COUNT; ++i)
      {
        rpmCahes[i].reset();
      }
    }

    void loop()
    {
      if (onFlag)
      {
        start();
        onFlag = false;
      }
    };

    int getPower()
    {
      int power = constrain(TABLE[tableIndex], 0, 255);
      return power;
    }

    void reset()
    {
      for (int i = 0; i < TABLE_COUNT; ++i)
      {
        rpmCahes[i].reset();
      }
      currentMode = StateMode::SleepMode;
      tableIndex = 0;
    }

    void next()
    {
      if (currentMode == StateMode::WaitMode)
      {
        rotateCounter.reset();
        vValueCounter.reset();
        iValueCounter.reset();
        currentMode = StateMode::CalcMode;
      }
      else if (currentMode == StateMode::CalcMode)
      {
        RPMCache& currentCache = rpmCahes[tableIndex];
        currentCache.rpm = rotateCounter.calcRPM();
        currentCache.vValue = Controller::calcVValue(vValueCounter.calcValue());
        currentCache.iValue = Controller::calcIValue(iValueCounter.calcValue(), iOffset);

        if (tableIndex == TABLE_COUNT - 1)
        {
          currentMode = StateMode::SleepMode;
          tableIndex = 0;
        }
        else
        {
          currentMode = StateMode::WaitMode;
          tableIndex = (tableIndex + 1) % TABLE_COUNT;
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
      static const int ROW_INDEX{2};
      for (int i = 0; i < TABLE_COUNT; ++i)
      {
        drawAdafruit.drawIntR(rpmCahes[i].rpm, 3, i + 1);
        drawAdafruit.drawFloat(rpmCahes[i].vValue, 10, i + 1);
        drawAdafruit.drawFloat(rpmCahes[i].iValue, 15, i + 1);

        const int colIndex{i + OFFSET};


        int blinkType{0};
        if (currentMode == StateMode::CalcMode && i == tableIndex)
        {
          blinkType = 1;
        }
        else if (currentMode == StateMode::CalcMode && i == tableIndex)
        {
          blinkType = 2;
        }

        const char* arrowMessage{getDrawArrow(blinkFlag, blinkType)};

        drawAdafruit.drawChar(arrowMessage, 0, colIndex, ROW_INDEX);
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
      if (checkMode == CheckMode::FreeCheckMode)
      {
        freeCheckParam.reset();
        freeCheckParam.iOffset = iOffset;
      }
      else if (checkMode == CheckMode::TorqueCheckMode)
      {
        torqueCheckParam.reset();
        torqueCheckParam.iOffset = iOffset;
      }

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

    preferences.begin("torque_checker_app", false); 
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

        analogWrite(WRITE_POWER_PIN, freeCheckParam.getPower());
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

        torqueCheckParam.loop();
        torqueCheckParam.display();
        drawAdafruit.display();
        analogWrite(WRITE_POWER_PIN, torqueCheckParam.getPower());

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