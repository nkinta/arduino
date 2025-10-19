#include <SPI.h>
#include <Wire.h>
#include <vector>

#include "display/draw_adafruit.h"

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

struct VoltageMapping {
  struct VoltPair {
    int input{ 0.f };
    float volt{ 0 };
  };

  const std::vector<VoltPair> mappingData{ { 0, 0.f }, {4094, 3.3f} };

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

struct BattreyStatus
{
  float currentV{0.f};
  float currentDischargeI{0.f};
  float targetV{0.f};
  float targetDischrgeI{0.f};
};

struct ButtonStatus{

  static constexpr int LONG_PUSH_MILLIS{ 1000 };

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

static constexpr float SEC_TO_RPM{ 60.f };

static constexpr float FPS{ 10.f };
static constexpr float SEC{ 1000.f };
static constexpr float ONE_FRAME_MS{ (1.f / FPS) * SEC };

class BatteryController {

  static constexpr uint8_t READ1_PIN{ 0 };
  static constexpr uint8_t READ2_PIN{ 1 };
  static constexpr uint8_t READ3_PIN{ 7 };
  static constexpr uint8_t READ4_PIN{ 6 };

  static constexpr int PUSH_BUTTON1{ 15 };
  static constexpr int PUSH_BUTTON2{ 14 };
  static constexpr int PUSH_BUTTON3{ 13 };
  static constexpr int PUSH_BUTTON4{ 12 };
  static constexpr int PUSH_BUTTON5{ 11 };

  ButtonStatus button1Status{};
  ButtonStatus button2Status{};
  ButtonStatus button3Status{};
  ButtonStatus button4Status{};
  ButtonStatus button5Status{};

  ValueCounter v1ValueCounter{};
  ValueCounter v2ValueCounter{};

  int readVolt{0};

  unsigned long loopSubMillis{ 0 };

public:
  void setup() {
    pinMode(PUSH_BUTTON1, INPUT);
    pinMode(PUSH_BUTTON2, INPUT);
    pinMode(PUSH_BUTTON3, INPUT);
    pinMode(PUSH_BUTTON4, INPUT);
    pinMode(PUSH_BUTTON5, INPUT);

    pinMode(READ1_PIN, INPUT);
    pinMode(READ2_PIN, INPUT);

    button1Status.init(PUSH_BUTTON1);
    button2Status.init(PUSH_BUTTON2);
    button3Status.init(PUSH_BUTTON3);
    button4Status.init(PUSH_BUTTON4);
    button5Status.init(PUSH_BUTTON5);
  }

  void loopMain() {
    int volt1{ analogRead(READ1_PIN) };
    v1ValueCounter.readVolt(volt1);
    int volt2{ analogRead(READ2_PIN) };
    v2ValueCounter.readVolt(volt2);

  };

  void loopSub() {

    // Serial.print("-\n");
    unsigned long v1Value{v1ValueCounter.calcValue()};
    drawAdafruit.drawFillLine(3);
    drawAdafruit.drawFloatR(voltageMapping.getVoltage(v1Value), 10, 3, 4, 3);

    //drawAdafruit.drawIntR(12341, 0, 0);
    //drawAdafruit.drawIntR(12342, 0, 1);
    //drawAdafruit.drawIntR(12343, 0, 3);
    drawAdafruit.display();

    const int check1Flag{button1Status.check()};
    if (check1Flag == 1) {
      drawAdafruit.drawString("push1", 0, 1);
      Serial.print("push1\n");
    }

    const int check2Flag{button2Status.check()};
    if (check2Flag == 1) {
      drawAdafruit.drawString("push2", 0, 1);
      Serial.print("push2\n");
    }

    const int check3Flag{button3Status.check()};
    if (check3Flag == 1) {
      drawAdafruit.drawString("push3", 0, 1);
      Serial.print("push3\n");
    }

    const int check4Flag{button4Status.check()};
    if (check4Flag == 1) {
      drawAdafruit.drawString("push4", 0, 1);
      Serial.print("push4\n");
    }

    const int check5Flag{button5Status.check()};
    if (check5Flag == 1) {
      drawAdafruit.drawString("push5", 0, 1);
      Serial.print("push5\n");
    }



  }

  void loopWhile() {

    loopMain();

    const unsigned long tempMillis{ millis() };
    if (tempMillis - loopSubMillis > ONE_FRAME_MS) {
      loopSub();
      loopSubMillis = tempMillis;
    }

  };

};

BatteryController controller;
// the setup function runs once when you press reset or power the board
void setup() {
  // initialize digital pin LED_BUILTIN as an output.
  pinMode(LED_BUILTIN, OUTPUT);

  Serial.begin(115200);
  Serial.print("start\n");

  controller.setup();

  drawAdafruit.setupDisplay();


}

// the loop function runs over and over again forever
void loop() {

  while (true) {
    controller.loopWhile();
  }

  return;

  digitalWrite(LED_BUILTIN, HIGH);  // turn the LED on (HIGH is the voltage level)
  delay(100);                      // wait for a second
  digitalWrite(LED_BUILTIN, LOW);   // turn the LED off by making the voltage LOW
  delay(100);              // wait for a second
}