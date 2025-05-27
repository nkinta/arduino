#include "esp_bt_main.h"
#include "esp_bt.h"
#include "esp_wifi.h"
#include "Preferences.h"

#include <EEPROM.h>

#include "display/draw_adafruit.h"
#include "deep_sleep.h"

Preferences preferences;

// 

static constexpr float FPS{ 15.f };
static constexpr float SEC{ 1000.f };
static constexpr float ONE_FRAME_MS{ (1.f / FPS) * SEC };

int MODE_ADDRESS = 0x0000;

DrawAdafruit drawAdafruit;

class ValueCounter {
  unsigned long totalValue{ 0 };

  int count{ 0 };

public:
  void readVolt(int inVolt) {
    totalValue += inVolt;
    count += 1;
  }

  void reset() {
    totalValue = 0;
    count = 0;
  }

  unsigned long calcValue() {
    if (count == 0) {
      return 0;
    }
    unsigned long result{ totalValue / count };
    reset();
    return result;
  }
};

class RotateCounter {
private:
  static constexpr int THREASHOULD{ 2048 };
  static constexpr float TO_RPM{ 60.f };

  static constexpr int POLE{ 14 };
  static constexpr int ROT_RATE{ POLE / 2 };
  static constexpr int GEAR_RATE{ 24 / 8 };
  static constexpr float PULSE_RATE{ static_cast<float>(GEAR_RATE) / static_cast<float>(ROT_RATE) };

  unsigned long oldMillis{ 0 };
  unsigned long totalDiffMillis{ 0 };

  bool currentVoltFlag{ false };
  bool oldVoltFlag{ false };

  long count{ 0 };
  long maxCount{ 0 };

  float rpm{ 0.f };
  float maxRpm{ 0.f };

public:
  void readVolt(int inVolt) {
    const int val{ inVolt };

    oldVoltFlag = currentVoltFlag;
    if (val > THREASHOULD + 256) {
      currentVoltFlag = true;
    } else if (val < THREASHOULD - 256) {
      currentVoltFlag = false;
    }

    if (oldVoltFlag != currentVoltFlag && currentVoltFlag == true) {
      count++;
    }
  }

  void sleep(unsigned long millis) {
    const unsigned long diffMillis{ millis - oldMillis };
    totalDiffMillis += diffMillis;
  }

  void start(unsigned long millis) {
    oldMillis = millis;
  }

  float calcRPM() {
    float result{0};
    if (totalDiffMillis > 0)
    {
      result = count * PULSE_RATE * TO_RPM * (SEC / static_cast<float>(totalDiffMillis));
    }
    reset();

    return result;
  }

  void reset() {
    count = 0;
    totalDiffMillis = 0;
  }
};

struct VoltageMapping {
  struct VoltPair {
    int input{ 0.f };
    float volt{ 0 };
  };

  const std::vector<VoltPair> mappingData{ { 29, 0.f }, { 327, 0.5f }, { 658, 1.f }, { 989, 1.5f }, { 1326, 2.f }, { 1671, 2.5f }, { 2011, 3.f }, { 2359, 3.5f }, { 2698, 4.f }, { 3042, 4.5f }, { 3405, 5.f }, {3820, 5.5f}, {4094, 5.8f} };

  float getVoltage(int input) {
    const VoltPair* before{ nullptr };
    for (const VoltPair& current : mappingData) {
      if (before) {
        if (before->input < input && input <= current.input) {
          return before->volt + ((current.volt - before->volt) / static_cast<float>(current.input - before->input)) * (input - before->input);
        }
      }

      before = &current;
    }
    return 0.f;
  }
};

static VoltageMapping voltageMapping;

class Controller {
  static constexpr uint8_t READ_PIN{ 3 };
  static constexpr uint8_t V_READ_PIN{ 2 };
  static constexpr uint8_t I_READ_PIN{ 4 };


private:

  enum class CheckMode {
    FreeCheckMode = 0,
    TorqueCheckMode,
    MaxCheckMode
  };

  static float calcVValue(int voltInt) {
    // return static_cast<float>(voltInt) * (2.f / ANALOG_READ_SLOPE);
    return voltageMapping.getVoltage(voltInt);
  }

  static float calcIValue(int voltInt, float offsetVoltage) {
    // return (static_cast<float>(voltInt) - offset) * (-20.f / ANALOG_READ_SLOPE);

    static float ampereRate{(80.f / 5.f) * 1.22f};
    return ampereRate * (offsetVoltage - voltageMapping.getVoltage(voltInt) * 0.5f);
  }

  struct RPMCache {
    int rpm{ 0 };
    float vValue{ 0 };
    float iValue{ 0 };
    void reset() {
      rpm = 0;
      vValue = 0.f;
      iValue = 0.f;
    }
  };

  static const char* getDrawArrow(const bool blinkFlag, int blinkType) {
    static char arrow1Message[] = ">";
    static char arrow2Message[] = ">>";
    static char emptyMessage[] = "  ";
    const char* arrowMessage = emptyMessage;
    if (blinkType == 2) {
      if (blinkFlag) {
        arrowMessage = arrow2Message;
      } else {
        arrowMessage = emptyMessage;
      }
    } else if (blinkType == 1) {
      if (blinkFlag) {
        arrowMessage = arrow1Message;
      } else {
        arrowMessage = emptyMessage;
      }
    }

    return arrowMessage;
  }

  struct BaseCheckParam {
    RotateCounter rotateCounter{};
    ValueCounter iValueCounter{};
    ValueCounter vValueCounter{};
  };

  struct FreeCheckParam : public BaseCheckParam {

    static constexpr int TABLE[] = { 30, 100, 120, 250 };
    static constexpr int TABLE_COUNT{ sizeof(TABLE) / sizeof(int) };

    int powerNum{ 0 };

    float iOffsetVoltage{ 0.f };

    bool nextFlag{ false };

    unsigned long misslisDiffA{ 0 };
    unsigned long misslisDiffB{ 0 };

    static constexpr int RPM_CACHE_COUNT{ 2 };

    RPMCache rpmCaches[RPM_CACHE_COUNT]{};

    bool isExecuteB(unsigned long millis) {
      const unsigned long misslisDiff{ millis - misslisDiffB };
      if (misslisDiff > SEC) {
        misslisDiffB = millis;
        return true;
      } else {
        return false;
      }
    }

    int getPower() {
      int power = constrain(TABLE[powerNum], 0, 255);
      return power;
    }

    bool isExecuteA(unsigned long millis) {
      const unsigned long misslisDiff{ millis - misslisDiffA };
      if (misslisDiff > ONE_FRAME_MS) {
        misslisDiffA = millis;
        return true;
      } else {
        return false;
      }
    }

    void displayB() {
      RPMCache& currentCache{ rpmCaches[0] };
      currentCache.rpm = rotateCounter.calcRPM();
      currentCache.vValue = Controller::calcVValue(vValueCounter.calcValue());
      currentCache.iValue = Controller::calcIValue(iValueCounter.calcValue(), iOffsetVoltage);

      RPMCache& maxCache{ rpmCaches[1] };
      if (currentCache.rpm > maxCache.rpm) {
        maxCache = currentCache;
      }

      static const int OFFSET{ 1 };
      static const int ROW_INDEX{ 2 };
      for (int i = 0; i < RPM_CACHE_COUNT; ++i) {
        const RPMCache& rpmCache{rpmCaches[i]};
        drawAdafruit.drawIntR(rpmCache.rpm, 4, i + 1, 6);
        drawAdafruit.drawFloat(rpmCache.vValue, 11, i + 1);
        drawAdafruit.drawFloat(rpmCache.iValue, 16, i + 1);
      }
    };

    void displayA() {
      drawAdafruit.drawChar("T", 0, 0, 1);
      drawAdafruit.drawInt(powerNum, 1, 0);
    };

    void pushButton2() {
      nextFlag = true;
    }

    void reset() {
      for (int i = 0; i < RPM_CACHE_COUNT; ++i) {
        rpmCaches[i].reset();
      }
      powerNum = 0;
    }

    void next() {
      if (nextFlag) {
        powerNum = (powerNum + 1) % TABLE_COUNT;
        for (int i = 0; i < RPM_CACHE_COUNT; ++i) {
          rpmCaches[i].reset();
        }
        nextFlag = false;
      }
    };
  };

  struct TorqueCheckParam : public BaseCheckParam {
    enum class StateMode {
      SleepMode = 0,
      WaitMode,
      CalcMode,
      MaxStateMode,
    };

    float waitTime{ 2000.f };
    float calcTime{ 2000.f };

    bool onFlag{ false };

    bool blinkFlag{ false };

    unsigned long drawMillisBuf{ 0 };
    unsigned long modeMillisBuf{ 0 };

    int tableIndex{ 0 };
    static constexpr int TABLE[] = { 30, 100, 250 };
    static constexpr int TABLE_COUNT{ sizeof(TABLE) / sizeof(int) };

    RPMCache rpmCaches[TABLE_COUNT]{};

    StateMode currentMode{ StateMode::SleepMode };

    RotateCounter rotateCounter{};
    ValueCounter iValueCounter{};
    ValueCounter vValueCounter{};

    float iOffsetVoltage{ 0.f };

    void pushButton2() {
      onFlag = true;
    }

    void start() {
      currentMode = StateMode::WaitMode;
      tableIndex = 0;
      modeMillisBuf = millis();

      for (int i = 0; i < TABLE_COUNT; ++i) {
        rpmCaches[i].reset();
      }
    }

    void loop() {
      if (onFlag) {
        start();
        onFlag = false;
      }
    };

    int getPower() {
      int power = constrain(TABLE[tableIndex], 0, 255);
      return power;
    }

    void reset() {
      for (int i = 0; i < TABLE_COUNT; ++i) {
        rpmCaches[i].reset();
      }
      currentMode = StateMode::SleepMode;
      tableIndex = 0;
    }

    void next() {
      if (currentMode == StateMode::WaitMode) {
        rotateCounter.reset();
        vValueCounter.reset();
        iValueCounter.reset();
        currentMode = StateMode::CalcMode;
      } else if (currentMode == StateMode::CalcMode) {
        RPMCache& currentCache = rpmCaches[tableIndex];
        currentCache.rpm = rotateCounter.calcRPM();
        currentCache.vValue = Controller::calcVValue(vValueCounter.calcValue());
        currentCache.iValue = Controller::calcIValue(iValueCounter.calcValue(), iOffsetVoltage);

        if (tableIndex == TABLE_COUNT - 1) {
          currentMode = StateMode::SleepMode;
          tableIndex = 0;
        } else {
          currentMode = StateMode::WaitMode;
          tableIndex = (tableIndex + 1) % TABLE_COUNT;
        }
      }
    };

    void display() {
      blinkFlag = !blinkFlag;

      static char sleepMessage[] = "sleep";
      static char waitMessage[] = "wait";
      static char calcMessage[] = "calc";
      if (currentMode == StateMode::WaitMode) {
        int sizeChar = sizeof(waitMessage);
        drawAdafruit.drawChar(waitMessage, 0, 0, sizeChar);
      } else if (currentMode == StateMode::CalcMode) {
        int sizeChar = sizeof(calcMessage);
        drawAdafruit.drawChar(calcMessage, 0, 0, sizeChar);
      } else if (currentMode == StateMode::SleepMode) {
        int sizeChar = sizeof(sleepMessage);
        drawAdafruit.drawChar(sleepMessage, 0, 0, sizeChar);
      }

      // drawAdafruit.drawInt(tableIndex, 0, 0);

      static const int OFFSET{ 1 };
      static const int ROW_INDEX{ 2 };

      float baseRpm = rpmCaches[0].rpm;
      for (int i = 0; i < TABLE_COUNT; ++i) {
        const RPMCache& rpmCache{rpmCaches[i]};
        int rpmRate{100};
        if (baseRpm > 10.f)
        {
          rpmRate = constrain((rpmCache.rpm / baseRpm) * 100.f, 0, 100);
        }

        drawAdafruit.drawIntR(rpmRate, 0, i + 1, 3);
        drawAdafruit.drawIntR(rpmCache.rpm, 4, i + 1, 6);
        drawAdafruit.drawFloat(rpmCache.vValue, 11, i + 1);
        drawAdafruit.drawFloat(rpmCache.iValue, 16, i + 1);

        const int colIndex{ i + OFFSET };


        int blinkType{ 0 };
        if (currentMode == StateMode::CalcMode && i == tableIndex) {
          blinkType = 1;
        } else if (currentMode == StateMode::WaitMode && i == tableIndex) {
          blinkType = 2;
        }

        if (currentMode != StateMode::SleepMode && i == tableIndex)
        {
        const char* arrowMessage{ getDrawArrow(blinkFlag, blinkType) };
        drawAdafruit.drawChar(arrowMessage, 0, colIndex, ROW_INDEX);
        }
        // drawAdafruit.drawInt(rpmCaches[i].maxRpm, 9, i + 1);
      }
    };

    float currentMills() {
      if (currentMode == StateMode::WaitMode) {
        return waitTime;
      } else if (currentMode == StateMode::CalcMode) {
        return calcTime;
      }
    };

    bool isChangeMode(float millis) {
      const float modeMisslisDiff{ millis - modeMillisBuf };
      if (modeMisslisDiff > currentMills()) {
        modeMillisBuf = millis;
        return true;
      } else {
        return false;
      }
    }

    bool isDraw(float millis) {
      const float drawMillisDiff{ millis - drawMillisBuf };
      if (drawMillisDiff > ONE_FRAME_MS) {
        drawMillisBuf = millis;
        return true;
      } else {
        return false;
      }
    }
  };

  static constexpr int LONG_PUSH_MILLIS{ 1000 };
  static constexpr int WRITE_POWER_PIN{ 20 };
  static constexpr int PUSH_BUTTON1{ 10 };
  static constexpr int PUSH_BUTTON2{ 9 };

  unsigned long loopSubMillis{ 0 };

  unsigned long cachedPushButtonMillis{ 0 };

  FreeCheckParam freeCheckParam{};
  TorqueCheckParam torqueCheckParam{};

  int buttonCount{ 0 };

  CheckMode checkMode{ CheckMode::FreeCheckMode };

  struct ButtonStatus{

    void init(const int inPinID)
    {
      pinID = inPinID;
    }

    int check()
    {
      int val = digitalRead(pinID);
      if (val == LOW) {
        buttonFlag = true;
      } else {
        buttonFlag = false;
      }

      bool releaseFlag{false};

      if (buttonFlag && (buttonOldFlag != buttonFlag))
      {
        pushedMillis = millis();
      }
      else if (!buttonFlag && (buttonOldFlag != buttonFlag)) {
        releaseFlag = true;
      }
      buttonOldFlag = buttonFlag;

      if (releaseFlag)
      {
        if ((millis() - pushedMillis) > LONG_PUSH_MILLIS)
        {
          return 2;
        }
        else
        {
          return 1;
        }
      }

      return 0;
    }

    int pinID{ 0 };
    bool buttonFlag{ false };
    bool buttonOldFlag{ false };
    unsigned long pushedMillis{0};
  };

  ButtonStatus button1Status{};
  ButtonStatus button2Status{};

  unsigned long iOffset{ 3400 };

  void drawMode() {
    String modeName;
    if (checkMode == CheckMode::FreeCheckMode) {
      modeName = String("mode1");
    } else if (checkMode == CheckMode::TorqueCheckMode) {
      modeName = String("mode2");
    }

    drawAdafruit.drawString(modeName, 0, 0);
  }

  void changeModeDisplay() {
    drawAdafruit.clearDisplay();
    drawMode();
    drawAdafruit.display();

    unsigned long iOffset = calibI();
    if (checkMode == CheckMode::FreeCheckMode) {
      freeCheckParam.reset();
      freeCheckParam.iOffsetVoltage = voltageMapping.getVoltage(iOffset) * 0.5f;
    } else if (checkMode == CheckMode::TorqueCheckMode) {
      torqueCheckParam.reset();
      torqueCheckParam.iOffsetVoltage = voltageMapping.getVoltage(iOffset) * 0.5f;
    }

    drawAdafruit.clearDisplay();
  }

  unsigned long calibI() {
    ValueCounter iValue{};

    for (int i = 0; i < 100; ++i) {
      int iVolt{ analogRead(I_READ_PIN)};
      iValue.readVolt(iVolt);
      delay(10);
    }

    return iValue.calcValue();
  }

  void loopSub() {

    if (button1Status.check() == 1) {
      cachedPushButtonMillis = millis();

      checkMode = static_cast<CheckMode>((static_cast<int>(checkMode) + 1) % static_cast<int>(CheckMode::MaxCheckMode));
      int checkModeInt = static_cast<uint8_t>(checkMode);
      preferences.putInt("check_mode", checkModeInt);
      changeModeDisplay();
    }

    if (button2Status.check() == 1) {
      cachedPushButtonMillis = millis();

      if (checkMode == CheckMode::FreeCheckMode) {
        freeCheckParam.pushButton2();
      } else if (checkMode == CheckMode::TorqueCheckMode) {
        torqueCheckParam.pushButton2();
      }
    }
  }

  void loopMain() {
    int volt{ analogRead(READ_PIN) };
    int vVolt{ analogRead(V_READ_PIN) };
    int iVolt{ analogRead(I_READ_PIN) };
    if (checkMode == CheckMode::FreeCheckMode) {
      freeCheckParam.rotateCounter.readVolt(volt);
      freeCheckParam.vValueCounter.readVolt(vVolt);
      freeCheckParam.iValueCounter.readVolt(iVolt);
    } else if (checkMode == CheckMode::TorqueCheckMode) {
      torqueCheckParam.rotateCounter.readVolt(volt);
      torqueCheckParam.vValueCounter.readVolt(vVolt);
      torqueCheckParam.iValueCounter.readVolt(iVolt);
    }
  };

public:

  void setup() {
    pinMode(READ_PIN, ANALOG);
    pinMode(V_READ_PIN, ANALOG);
    pinMode(I_READ_PIN, ANALOG);

    pinMode(WRITE_POWER_PIN, OUTPUT);
    pinMode(PUSH_BUTTON1, INPUT);
    pinMode(PUSH_BUTTON2, INPUT);

    button1Status.init(PUSH_BUTTON1);
    button2Status.init(PUSH_BUTTON2);

    preferences.begin("torque_checker_app", false);
    int checkModeInt = preferences.getInt("check_mode", 0);

    checkMode = static_cast<CheckMode>(checkModeInt);  // EEPROM.read(MODE_ADDRESS));
    drawAdafruit.clearDisplay();
    drawMode();
    drawAdafruit.display();
    delay(500);
    drawAdafruit.clearDisplay();

    changeModeDisplay();
  };

  void loopWhile() {
    loopMain();

    const unsigned long tempMillis{ millis() };
    if (tempMillis - loopSubMillis > ONE_FRAME_MS) {
      loopSub();
      loopSubMillis = tempMillis;
    }

    if (checkMode == CheckMode::FreeCheckMode) {
      // const float rpmMisslisDiff{millis() - freeCheckParam.rpmMillisBuf};
      if (freeCheckParam.isExecuteB(millis())) {
        freeCheckParam.rotateCounter.sleep(millis());
        freeCheckParam.displayB();

        freeCheckParam.rotateCounter.start(millis());
      }


      if (freeCheckParam.isExecuteA(millis()))  // voltMillisDiff > ONE_FRAME_MS)
      {
        freeCheckParam.rotateCounter.sleep(millis());

        freeCheckParam.next();
        freeCheckParam.displayA();
        drawAdafruit.display();

        analogWrite(WRITE_POWER_PIN, freeCheckParam.getPower());

        freeCheckParam.rotateCounter.start(millis());
      }
    } else if (checkMode == CheckMode::TorqueCheckMode) {

      if (torqueCheckParam.isChangeMode(millis())) {
        torqueCheckParam.rotateCounter.sleep(millis());

        torqueCheckParam.next();
        torqueCheckParam.rotateCounter.start(millis());
      }

      if (torqueCheckParam.isDraw(millis())) {
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

void setup() {

  delay(500);

  Serial.begin(115200);
  Serial.printf("start\r\n");

  drawAdafruit.setupDisplay();

  drawAdafruit.clearDisplay();

  controller.setup();
}

void loop() {

  while (true) {
    controller.loopWhile();
  }

  return;
  // put your main code here, to run repeatedly:
}
