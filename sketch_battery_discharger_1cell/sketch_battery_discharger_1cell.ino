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

enum class DisChargeMode : uint8_t {
  DisplayNone,
  DischargeNormal,
  DischargeStop,
  Max,
};

enum class DisplaySettingMode : uint8_t {
  Display,
  ModeChangeSetting,
  DischargeVSetting,
  DischargeISetting,
  Max,
};

enum class ActiveOrSleep : uint8_t {
  None,
  Active,
  Sleep,
  SleepDelRead,
  SleepHoldRead,
  Max,
};

struct BatteryStatus
{
  static constexpr int START_LINE{1};
  static constexpr int SETTING_MENU_START_COL{7};
  static constexpr int SETTING_MENU_OFFSET_COL{5};

  static constexpr int abc[]{10};

  static constexpr ActiveOrSleep READ_MODE_LOOPS[] = {
    ActiveOrSleep::Active, ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None,
    ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None,
    ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None,

    ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None,
    ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None,
    ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None,

    ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None,
    ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None,
    ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None,

    ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None,
    ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None,
    ActiveOrSleep::Sleep,ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::SleepDelRead, ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::None, ActiveOrSleep::SleepHoldRead
  };

  static constexpr int32_t sizeOfMode{sizeof(READ_MODE_LOOPS)};

  unsigned long loopCount{ 0 };

  ActiveOrSleep currentActiveOrSleep{ActiveOrSleep::Active};

  void write() {
    if (disChargeMode == DisChargeMode::DisplayNone)
    {
      analogWrite(writePin, 0);
    }
    else if (disChargeMode == DisChargeMode::DischargeNormal)
    {
      if (V > targetV)
      {
        I = targetI;
        int intValue = calcPWMValue(I);
        analogWrite(writePin, intValue);
      }
      else
      {
        I = 0.f;
        int intValue = calcPWMValue(I);
        analogWrite(writePin, intValue);
      }
    }
    else if (disChargeMode == DisChargeMode::DischargeStop)
    {
      if (V > targetV)
      {
        I = targetI;
        int intValue = calcPWMValue(I);
        analogWrite(writePin, intValue);
      }
      else
      {
        I = 0.f;
        int intValue = calcPWMValue(I);
        analogWrite(writePin, intValue);
      }
    }
  }; 

  public:

  BatteryStatus(uint8_t inReadPin, uint8_t inWritePin, uint8_t inBatteryIndex) : readPin{inReadPin}, writePin{inWritePin}, batteryIndex{inBatteryIndex} {};

  static int calcPWMValue(float ampere)
  {
    static const float REG{0.1f};
    static const float REG_RATE{ (5.1f + 5.1f + 1.f) / 1.f };
    static const float VOLT3_3{3.3f};
    const float volt{ampere * REG * REG_RATE};
    const float voltRate{volt / VOLT3_3};

    return std::clamp(static_cast<int>(255.f * voltRate), 0, 255);
  };

  void read() {
    const int volt{ analogRead(readPin) };
    valueCounter.readVolt(volt);
  };

  void setup()
  {
    pinMode(readPin, INPUT);
    pinMode(writePin, OUTPUT);
  };

  void loopSub()
  {
    ActiveOrSleep tempActiveOrSleep{READ_MODE_LOOPS[(++loopCount) % sizeOfMode]};
    if (currentActiveOrSleep != ActiveOrSleep::None)
    {
      currentActiveOrSleep = tempActiveOrSleep;
    }

  }

  void display(DisplaySettingMode displaySettingMode)
  {
    unsigned long v1Value{valueCounter.calcValue()};
    V = voltageMapping.getVoltage(v1Value);

    if (settingFlag)
    {
      drawAdafruit.drawString(">", 5 * batteryIndex, 0);
    }
    drawAdafruit.drawFloatR(V, 5 * (batteryIndex + 1), 0, 4, 2);

    if (settingFlag)
    {
      if (displaySettingMode == DisplaySettingMode::Display)
      {
        displayDetail();
      }
      else if (displaySettingMode == DisplaySettingMode::ModeChangeSetting)
      {
        displayModeChangeSetting();
      }
      else if (displaySettingMode == DisplaySettingMode::DischargeVSetting)
      {
        displayDischargeV();
      }
      else if (displaySettingMode == DisplaySettingMode::DischargeISetting)
      {
        displayDischargeI();
      }
      else
      {
        displayNone();
      }

    }
  };

  void shiftParam(DisplaySettingMode displaySettingMode, int shift)
  {
    if (displaySettingMode == DisplaySettingMode::ModeChangeSetting)
    {
      const int nextModeIndex{(static_cast<int>(disChargeMode) + shift) % static_cast<int>(DisChargeMode::Max)};
      disChargeMode = static_cast<DisChargeMode>(nextModeIndex);
    }
    else if (displaySettingMode == DisplaySettingMode::DischargeVSetting)
    {
      targetV += 0.01f * static_cast<float>(shift);
    }
    else if (displaySettingMode == DisplaySettingMode::DischargeISetting)
    {
      targetI += 0.1f * static_cast<float>(shift);
    }
  }

  void displayNone()
  {
    int line{START_LINE};
    drawAdafruit.drawFillLine(line);
    ++line;
    drawAdafruit.drawFillLine(line);
    ++line;
    drawAdafruit.drawFillLine(line);
    ++line;
    drawAdafruit.drawFillLine(line);
  }

  void displayModeChangeSetting()
  {
    int vir_offset{0};
    int line{START_LINE};
    static std::vector<String> modeNames{String("None"), String("DiscA"), String("DiscB")};
    String modeName;
    drawAdafruit.drawFillLine(line);
    vir_offset = SETTING_MENU_START_COL;
    drawAdafruit.drawString("   |     ", vir_offset, line);

    ++line;
    drawAdafruit.drawFillLine(line);
    vir_offset = SETTING_MENU_START_COL;
    drawAdafruit.drawString(modeNames[(uint8_t)disChargeMode], SETTING_MENU_START_COL + 1, line);

    ++line;
    drawAdafruit.drawFillLine(line);
    vir_offset = SETTING_MENU_START_COL;
    drawAdafruit.drawString("   |     ", vir_offset, line);

    ++line;
    drawAdafruit.drawFillLine(line);
    ++line;
    drawAdafruit.drawFillLine(line);
  }

  void displayDischargeI()
  {
    int vir_offset{0};
    int line{START_LINE};
    drawAdafruit.drawFillLine(line);
    vir_offset = SETTING_MENU_START_COL;
    drawAdafruit.drawString("   |     ", vir_offset, line);

    ++line;
    drawAdafruit.drawFillLine(line);
    vir_offset = SETTING_MENU_START_COL;
    vir_offset += SETTING_MENU_OFFSET_COL;
    drawAdafruit.drawFloatR(targetI, vir_offset, line, SETTING_MENU_OFFSET_COL, 3);
    drawAdafruit.drawString("A", vir_offset, line);

    ++line;
    drawAdafruit.drawFillLine(line);
    vir_offset = SETTING_MENU_START_COL;
    drawAdafruit.drawString("   |     ", vir_offset, line);

    ++line;
    drawAdafruit.drawFillLine(line);
    ++line;
    drawAdafruit.drawFillLine(line);
  }

  void displayDischargeV()
  {
    int vir_offset{0};
    int line{START_LINE};
    drawAdafruit.drawFillLine(line);
    vir_offset = SETTING_MENU_START_COL;
    drawAdafruit.drawString("   |     ", vir_offset, line);

    ++line;
    drawAdafruit.drawFillLine(line);
    vir_offset = SETTING_MENU_START_COL;
    vir_offset += SETTING_MENU_OFFSET_COL;
    drawAdafruit.drawFloatR(targetV, vir_offset, line, SETTING_MENU_OFFSET_COL, 3);
    drawAdafruit.drawString("V", vir_offset, line);

    ++line;
    drawAdafruit.drawFillLine(line);
    vir_offset = SETTING_MENU_START_COL;
    drawAdafruit.drawString("   |     ", vir_offset, line);

    ++line;
    drawAdafruit.drawFillLine(line);
    ++line;
    drawAdafruit.drawFillLine(line);
  }

  void displayDetail()
  {
    static constexpr int DISPLAY_MENU_START_COL{3};
    static constexpr int DISPLAY_MENU_OFFSET_COL{5};
    int vir_offset{0};
    int line{START_LINE};
    static std::vector<String> modeNames{String("None"), String("DiscA"), String("DiscB")};
    String modeName;
    drawAdafruit.drawFillLine(line);
    vir_offset = DISPLAY_MENU_START_COL;
    drawAdafruit.drawString(modeNames[(uint8_t)disChargeMode], vir_offset + 5, line);

    ++line;
    drawAdafruit.drawFillLine(line);
    vir_offset = DISPLAY_MENU_START_COL;
    vir_offset += DISPLAY_MENU_OFFSET_COL;
    drawAdafruit.drawFloatR(V, vir_offset, line, DISPLAY_MENU_OFFSET_COL, 3);
    drawAdafruit.drawString("V", vir_offset, line);

    vir_offset += 2;
    drawAdafruit.drawString("=>", vir_offset, line);

    vir_offset += 3;
    vir_offset += DISPLAY_MENU_OFFSET_COL;
    drawAdafruit.drawFloatR(targetV, vir_offset, line, DISPLAY_MENU_OFFSET_COL, 3);
    drawAdafruit.drawString("V", vir_offset, line);

    ++line;
    drawAdafruit.drawFillLine(line);
    vir_offset = DISPLAY_MENU_START_COL;
    vir_offset += DISPLAY_MENU_OFFSET_COL;
    drawAdafruit.drawFloatR(I, vir_offset, line, DISPLAY_MENU_OFFSET_COL, 3);
    drawAdafruit.drawString("A", vir_offset, line);

    vir_offset += 2;
    drawAdafruit.drawString("=>", vir_offset, line);

    vir_offset += 3;
    vir_offset += SETTING_MENU_OFFSET_COL;
    drawAdafruit.drawFloatR(targetI, vir_offset, line, SETTING_MENU_OFFSET_COL, 3);
    drawAdafruit.drawString("A", vir_offset, line);

    ++line;
    drawAdafruit.drawFillLine(line);
    if (0)
    {
      drawAdafruit.drawInt(calcPWMValue(targetI), SETTING_MENU_START_COL, line);
      drawAdafruit.drawString("PWM", SETTING_MENU_START_COL + SETTING_MENU_OFFSET_COL, line);
    }

  }

  ValueCounter valueCounter{};

  ActiveOrSleep activeOrSleep{ActiveOrSleep::Active};

  bool settingFlag{false};
  uint8_t batteryIndex;
  uint8_t readPin{0};
  uint8_t writePin{0};

  float V{0.f};
  float targetV{1.35f};
  float I{0.0f};
  float targetI{0.2f};

  DisChargeMode disChargeMode{DisChargeMode::DisplayNone};
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

static constexpr float FPS{ 30.f };
static constexpr float SEC{ 1000.f };
static constexpr float ONE_FRAME_MS{ (1.f / FPS) * SEC };

int modeIndex = 0;

class BatteryController {

  static constexpr uint8_t READ1_PIN{ 0 };
  static constexpr uint8_t READ2_PIN{ 1 };
  static constexpr uint8_t READ3_PIN{ 6 };
  static constexpr uint8_t READ4_PIN{ 7 };

  static constexpr uint8_t WRITE1_PIN{ 2 };
  static constexpr uint8_t WRITE2_PIN{ 3 };
  static constexpr uint8_t WRITE3_PIN{ 8 };
  static constexpr uint8_t WRITE4_PIN{ 9 };

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

  DisplaySettingMode displaySettingMode{DisplaySettingMode::Display};

  unsigned long loopSubMillis{ 0 };

  size_t currentBatteryIndex{0};

  unsigned long loopSubCount{ 0 };

public:
  BatteryController() {
    batteryStatuses[currentBatteryIndex].settingFlag = true;
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
      batteryStatus.display(displaySettingMode);
    }
  }

  void changeTargetBattery(int shift)
  {
    currentBatteryIndex = (currentBatteryIndex + shift) % batteryStatuses.size();

    for (size_t index{0}; index < batteryStatuses.size(); ++index)
    {
      if (index== currentBatteryIndex)
      {
        batteryStatuses[index].settingFlag = true;
      }
      else
      {
        batteryStatuses[index].settingFlag = false;
      }
    }
  }

  void changeDisplayMode(int shift)
  {
    const int nextModeIndex{(static_cast<int>(displaySettingMode) + shift) % static_cast<int>(DisplaySettingMode::Max)};

    displaySettingMode = static_cast<DisplaySettingMode>(nextModeIndex);
  }

  void shiftParam(int shift)
  {
    batteryStatuses[currentBatteryIndex].shiftParam(displaySettingMode, shift);
  }

  void loopSub()
  {
    digitalWrite(LED_BUILTIN, (++loopSubCount % 2));

    for (auto& batteryStatus : batteryStatuses)
    {
      batteryStatus.loopSub();
    }

    display();
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
      shiftParam(1);
    }

    checkFlag = buttonDStatus.check();
    if (checkFlag == 1) {
      shiftParam(-1);
    }

    checkFlag = buttonCStatus.check();
    if (checkFlag == 1) {
      changeDisplayMode(1);
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

  // Serial.begin(115200);
  // Serial.print("start\n");

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