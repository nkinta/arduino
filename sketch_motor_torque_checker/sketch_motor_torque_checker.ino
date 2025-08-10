#include "esp_bt_main.h"
#include "esp_bt.h"
#include "esp_wifi.h"
#include "Preferences.h"

#include <EEPROM.h>

#include "display/draw_adafruit.h"
#include "deep_sleep.h"
#include "check_mode.h"

Preferences preferences;

int MODE_ADDRESS = 0x0000;

class Controller {
  static constexpr uint8_t READ_PIN{ 3 };
  static constexpr uint8_t V_READ_PIN{ 2 };
  static constexpr uint8_t I_READ_PIN{ 4 };


private:

  enum class CheckMode {
    FreeCheckMode = 0,
    TorqueCheckMode,
    RunSimCheckMode,
    MaxCheckMode
  };

  static constexpr int LONG_PUSH_MILLIS{ 1000 };

  static constexpr int PUSH_BUTTON1{ 10 };
  static constexpr int PUSH_BUTTON2{ 9 };

  // bool displayInitFlag{false};

  unsigned long loopSubMillis{ 0 };
  unsigned long drawModeMillis{ 0 };
  unsigned long cachedPushButtonMillis{ 0 };

  FreeCheckParam freeCheckParam{};
  TorqueCheckParam torqueCheckParam{};
  RunSimCheckParam runSimCheckParam{};

  int buttonCount{ 0 };

  CheckMode checkMode{ CheckMode::FreeCheckMode };
  bool chageCheckModeFlag{true};

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

  void drawMode() {
    String modeName;
    if (checkMode == CheckMode::FreeCheckMode) {
      modeName = String("mode1");
    } else if (checkMode == CheckMode::TorqueCheckMode) {
      modeName = String("mode2");
    } else if (checkMode == CheckMode::RunSimCheckMode) {
      modeName = String("mode3");
    } else {
      modeName = String("no mode");
    }

    drawAdafruit.clearDisplay();
    drawAdafruit.drawString(modeName, 0, 0);
  }

  void changeModeReset() {
    if (checkMode == CheckMode::FreeCheckMode) {
      freeCheckParam.reset();
    } else if (checkMode == CheckMode::TorqueCheckMode) {
      torqueCheckParam.reset();
    } else if (checkMode == CheckMode::RunSimCheckMode) {
      runSimCheckParam.reset();
    }
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
      int checkModeInt = static_cast<int>(checkMode);
      Serial.println(checkModeInt);
      preferences.putInt("check_mode", checkModeInt);

      chageCheckModeFlag = true;
      changeModeReset();
    }

    const int checkFlag{button2Status.check()};
    if (checkFlag == 1) {
      cachedPushButtonMillis = millis();

      if (checkMode == CheckMode::FreeCheckMode) {
        freeCheckParam.pushButton2();
      } else if (checkMode == CheckMode::TorqueCheckMode) {
        torqueCheckParam.pushButton2();
      } else if (checkMode == CheckMode::RunSimCheckMode) {
        runSimCheckParam.pushButton2();
      }
    }
    else if (checkFlag == 2) {
      if (checkMode == CheckMode::RunSimCheckMode) {
        runSimCheckParam.pushButtonLong2();
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
    } else if (checkMode == CheckMode::RunSimCheckMode) {
      runSimCheckParam.rotateCounter.readVolt(volt);
      runSimCheckParam.vValueCounter.readVolt(vVolt);
      runSimCheckParam.iValueCounter.readVolt(iVolt);
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

    preferences.begin("torque_checker", false);
    const int checkModeInt = preferences.getInt("check_mode", 1);
    checkMode = static_cast<CheckMode>(checkModeInt);  // EEPROM.read(MODE_ADDRESS));
    Serial.println(checkModeInt);

    unsigned long iOffset = calibI();
    const float offsetVoltage{voltageMapping.getVoltage(iOffset) * 0.5f};
    {
      freeCheckParam.iOffsetVoltage = offsetVoltage;
      torqueCheckParam.iOffsetVoltage = offsetVoltage;
      runSimCheckParam.iOffsetVoltage = offsetVoltage;
    }
    changeModeReset();
  };

  void loopWhile() {

    loopMain();

    const unsigned long tempMillis{ millis() };
    if (tempMillis - loopSubMillis > ONE_FRAME_MS) {
      loopSub();
      loopSubMillis = tempMillis;
    }

    if (chageCheckModeFlag || !(tempMillis - drawModeMillis > 1000))
    {
      if (chageCheckModeFlag)
      {
        chageCheckModeFlag = false;
        drawModeMillis = millis();
        drawMode();
        drawAdafruit.display();
      }

      // drawAdafruit.clearDisplay();
      // drawAdafruit.adaDisplay.setTextSize(1);
    }
    else {

      if (checkMode == CheckMode::FreeCheckMode) {
        // const float rpmMisslisDiff{millis() - freeCheckParam.rpmMillisBuf};
        if (freeCheckParam.evalBTiming.isExecute(millis())) {
          freeCheckParam.rotateCounter.sleep(millis());
          freeCheckParam.execB();
          freeCheckParam.rotateCounter.start(millis());
        }

        if (freeCheckParam.evalATiming.isExecute(millis())) {
          freeCheckParam.rotateCounter.sleep(millis());
          freeCheckParam.execA();
          drawAdafruit.display();

          freeCheckParam.rotateCounter.start(millis());
        }
      } else if (checkMode == CheckMode::TorqueCheckMode) {

        if (torqueCheckParam.evalBTiming.isExecute(millis())) {
          torqueCheckParam.rotateCounter.sleep(millis());
          torqueCheckParam.execB();
          torqueCheckParam.rotateCounter.start(millis());
        }

        if (torqueCheckParam.evalATiming.isExecute(millis())) {
          torqueCheckParam.rotateCounter.sleep(millis());
          torqueCheckParam.execA();
          drawAdafruit.display();

          torqueCheckParam.rotateCounter.start(millis());
        }
      } else if (checkMode == CheckMode::RunSimCheckMode) {

        if (runSimCheckParam.evalBTiming.isExecute(millis())) {
          runSimCheckParam.rotateCounter.sleep(millis());
          runSimCheckParam.execB();
          runSimCheckParam.rotateCounter.start(millis());
        }

        if (runSimCheckParam.evalATiming.isExecute(millis())) {
          runSimCheckParam.rotateCounter.sleep(millis());
          runSimCheckParam.execA();
          drawAdafruit.display();

          runSimCheckParam.rotateCounter.start(millis());
        }
      }
    }
  };
};

Controller controller;

void setup() {

  Serial.begin(115200);
  Serial.printf("start\r\n");

  drawAdafruit.setupDisplay();

  controller.setup();
}

void loop() {

  while (true) {
    controller.loopWhile();
  }

  return;
  // put your main code here, to run repeatedly:
}
