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

enum class DisChargeMode {
  DisplayMode,
  DischargeMode,
  DischargeStopMode,
};

struct BatteryStatus
{
  public:
  BatteryStatus(uint8_t inReadPin, uint8_t inWritePin, uint8_t inBatteryIndex) : readPin{inReadPin}, writePin{inWritePin}, batteryIndex{inBatteryIndex} {};

  void read() {
    const int volt{ analogRead(readPin) };
    valueCounter.readVolt(volt);
  };

  void write() {
    // analogWrite(writePin, 50);
  }; 

  void setup()
  {
    pinMode(readPin, INPUT);
    pinMode(writePin, OUTPUT);
  };

  void display()
  {
    unsigned long v1Value{valueCounter.calcValue()};
    if (activeFlag)
    {
      drawAdafruit.drawString(">", 5 * batteryIndex, 0);
    }
    drawAdafruit.drawFloatR(voltageMapping.getVoltage(v1Value), 5 * (batteryIndex + 1), 0, 4, 2);
  };

  ValueCounter valueCounter{};

  bool activeFlag{false};
  uint8_t batteryIndex;
  uint8_t readPin{0};
  uint8_t writePin{0};
  float currentV{0.f};
  float currentDischargeI{0.f};
  float targetV{0.f};
  float targetDischargeI{0.f};
  DisChargeMode Mode;
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
  static constexpr uint8_t READ3_PIN{ 6 };
  static constexpr uint8_t READ4_PIN{ 7 };

  static constexpr uint8_t WRITE1_PIN{ 3 };
  static constexpr uint8_t WRITE2_PIN{ 4 };
  static constexpr uint8_t WRITE3_PIN{ 9 };
  static constexpr uint8_t WRITE4_PIN{ 10 };

  static constexpr int PUSH_BUTTON1{ 15 };
  static constexpr int PUSH_BUTTON2{ 14 };
  static constexpr int PUSH_BUTTON3{ 13 };
  static constexpr int PUSH_BUTTON4{ 12 };
  static constexpr int PUSH_BUTTON5{ 11 };

  ButtonStatus buttonLStatus{};
  ButtonStatus buttonRStatus{};
  ButtonStatus buttonUStatus{};
  ButtonStatus buttonDStatus{};
  ButtonStatus buttonCStatus{};

  std::vector<BatteryStatus> batteryStatuses{
  BatteryStatus{READ1_PIN, WRITE1_PIN, 0},
  BatteryStatus{READ2_PIN, WRITE2_PIN, 1},
  BatteryStatus{READ3_PIN, WRITE3_PIN, 2},
  BatteryStatus{READ4_PIN, WRITE4_PIN, 3}};

  unsigned long loopSubMillis{ 0 };

  size_t currentBatteryIndex{0};

public:
  BatteryController() {
    batteryStatuses[currentBatteryIndex].activeFlag = true;
  };

  void setup() {
    pinMode(PUSH_BUTTON1, INPUT);
    pinMode(PUSH_BUTTON2, INPUT);
    pinMode(PUSH_BUTTON3, INPUT);
    pinMode(PUSH_BUTTON4, INPUT);
    pinMode(PUSH_BUTTON5, INPUT);

    buttonLStatus.init(PUSH_BUTTON1);
    buttonRStatus.init(PUSH_BUTTON4);
    buttonUStatus.init(PUSH_BUTTON3);
    buttonDStatus.init(PUSH_BUTTON2);
    buttonCStatus.init(PUSH_BUTTON5);

    for (auto& batteryStatus : batteryStatuses)
    {
      batteryStatus.setup();
    }
  }

  void loopMain() {
    for (auto& batteryStatus : batteryStatuses)
    {
      batteryStatus.read();
    }
  };

  void display() {

    drawAdafruit.drawFillLine(0);
    for (auto& batteryStatus : batteryStatuses)
    {
      batteryStatus.display();
    }
  }

  void changeTargetBattery(int shift) {
    currentBatteryIndex = (currentBatteryIndex + shift) % batteryStatuses.size();

    for (size_t index{0}; index < batteryStatuses.size(); ++index)
    {
      if (index== currentBatteryIndex)
      {
        batteryStatuses[index].activeFlag = true;
      }
      else
      {
        batteryStatuses[index].activeFlag = false;
      }
    }
  }

  void loopSub() {

    display();

    for (auto& batteryStatus : batteryStatuses)
    {
      batteryStatus.write();
    }

    //drawAdafruit.drawIntR(12341, 0, 0);
    //drawAdafruit.drawIntR(12342, 0, 1);
    //drawAdafruit.drawIntR(12343, 0, 3);
    drawAdafruit.display();

    int checkFlag{0};
    checkFlag = buttonLStatus.check();
    if (checkFlag == 1) {
      changeTargetBattery(-1);
    }

    checkFlag = buttonRStatus.check();
    if (checkFlag == 1) {
      changeTargetBattery(1);
    }

    checkFlag = buttonUStatus.check();
    if (checkFlag == 1) {
      drawAdafruit.drawString("push3", 0, 1);
      Serial.print("push3\n");
    }

    checkFlag = buttonDStatus.check();
    if (checkFlag == 1) {
      drawAdafruit.drawString("push4", 0, 1);
      Serial.print("push4\n");
    }

    checkFlag = buttonCStatus.check();
    if (checkFlag == 1) {
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