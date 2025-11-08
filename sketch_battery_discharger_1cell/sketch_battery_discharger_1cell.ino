#include <SPI.h>
#include <Wire.h>
#include <vector>
#include <EEPROM.h>

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
  DischargeNone,
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

enum class TimeStatus : uint8_t {
  None = 0,
  Active = 1,
  SleepStart = 2,
  SleepStartRead = 3,
  SleepEnd = 4,
  Max,
};

enum class BatteryStatus : uint8_t {
  None,
  Active,
  Sleep,
  Stop,
  NoBat,
  Max,
};

struct BatteryInfo
{
  static constexpr int START_LINE{1};
  static constexpr int SETTING_MENU_START_COL{7};
  static constexpr int SETTING_MENU_OFFSET_COL{5};

  static constexpr int abc[]{10};

  static constexpr int8_t DISCHARGE_MODE_LOOPS[] = {
    2, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    3, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

    4, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

    1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

    1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 };

  static constexpr float ACTIVE_RATE{3.f / 4.f};

  static constexpr int8_t NONE_MODE_LOOPS[] = {
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0,

    1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0,

    1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0,

    1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 };

  unsigned long loopCount{ 0 };
  unsigned long displayCount{ 0 };

  TimeStatus currentTimeStatus{TimeStatus::Active};

  static float calcI(const float targetI, const float V, const float targetV, const DisChargeMode disChargeMode) {
    float resultI{0};
    if (disChargeMode == DisChargeMode::DischargeNormal)
    {
      if (V > targetV)
      {
        resultI = targetI;
      }
      else
      {
        resultI = 0.f;
      }
    }
    else if (disChargeMode == DisChargeMode::DischargeStop)
    {
      if (V > targetV)
      {
        resultI = targetI;
      }
      else
      {
        resultI = 0.f;
      }
    }

    return resultI;
  }; 

  static int calcPWMValue(float ampere)
  {
    static const float REG{0.1f};
    static const float REG_RATE{ (5.1f + 5.1f + 1.f) / 1.f };
    static const float VOLT3_3{3.3f};
    const float volt{ampere * REG * REG_RATE};
    const float voltRate{volt / VOLT3_3};

    return std::clamp(static_cast<int>(255.f * voltRate * (1 / ACTIVE_RATE)), 0, 255);
  };

  std::vector<String> modeNames{String("  None  "), String(" DiscNorm"), String(" DiscStop")};

  public:

  BatteryInfo(uint8_t inReadPin, uint8_t inWritePin, uint8_t inBatteryIndex) : readPin{inReadPin}, writePin{inWritePin}, batteryIndex{inBatteryIndex} {};

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

    if (nextBatteryStatus != currentBatteryStatus)
    {
      reset();
      currentBatteryStatus = nextBatteryStatus;
    }


    if (disChargeMode == DisChargeMode::DischargeNone)
    {
      currentTimeStatus = static_cast<TimeStatus>(NONE_MODE_LOOPS[(++loopCount) % sizeof(NONE_MODE_LOOPS)]);
      unsigned long temp{valueCounter.calcValue()};
      sleepV = voltageMapping.getVoltage(temp);
      V = sleepV;
      tunedI = 0;
      I = 0;
    }
    else
    {
      currentTimeStatus = static_cast<TimeStatus>(DISCHARGE_MODE_LOOPS[(++loopCount) % sizeof(DISCHARGE_MODE_LOOPS)]);

      if (currentTimeStatus == TimeStatus::None)
      {

      }
      else if (currentTimeStatus == TimeStatus::Active)
      {
        unsigned long temp{valueCounter.calcValue()};
        V = voltageMapping.getVoltage(temp);
        I = std::max(0.f, tunedI);
      }
      else if (currentTimeStatus == TimeStatus::SleepStart)
      {
        unsigned long temp{valueCounter.calcValue()};
        V = voltageMapping.getVoltage(temp);
        I = 0;
      }
      else if (currentTimeStatus == TimeStatus::SleepStartRead)
      {
        unsigned long temp{valueCounter.calcValue()};
        I = 0;
      }
      else if (currentTimeStatus == TimeStatus::SleepEnd)
      {
        unsigned long temp{valueCounter.calcValue()};
        sleepV = voltageMapping.getVoltage(temp);
        if (currentBatteryStatus == BatteryStatus::Stop && disChargeMode == DisChargeMode::DischargeStop)
        {
          tunedI = 0;
        }
        else
        {
          tunedI = calcI(targetI, sleepV, targetV, disChargeMode);
        }
        I = std::max(0.f, tunedI);
      }
    }


    if (currentBatteryStatus == BatteryStatus::Active || currentBatteryStatus == BatteryStatus::Stop || currentBatteryStatus == BatteryStatus::None)
    {
      if (V > 0.1f)
      {
        if (tunedI > 0)
        {
          nextBatteryStatus = BatteryStatus::Active;
        }
        else if (tunedI == 0)
        {
          nextBatteryStatus = BatteryStatus::Stop;
        }
      }
      else
      {
        nextBatteryStatus = BatteryStatus::NoBat;
      }
    }
    else if (currentBatteryStatus == BatteryStatus::NoBat)
    {
      if (V > 0.1f)
      {
        nextBatteryStatus = BatteryStatus::Active;
        startTime = millis();
      }
    }

    int intValue = calcPWMValue(I);
    analogWrite(writePin, intValue);

  };

  void reset()
  {
    tunedI = -1;
    loopCount = 0;
  };

  void display(DisplaySettingMode displaySettingMode)
  {

    ++displayCount;

    if (settingFlag)
    {
      drawAdafruit.drawString(">", 5 * batteryIndex, 0);
    }
    drawAdafruit.drawFloatR(sleepV, 5 * (batteryIndex + 1), 0, 4, 2);

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
      const int nextModeIndex{(static_cast<int>(DisChargeMode::Max) + static_cast<int>(disChargeMode) + shift) % static_cast<int>(DisChargeMode::Max)};
      disChargeMode = static_cast<DisChargeMode>(nextModeIndex);
      loopCount = 0;
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

    String modeName;
    drawAdafruit.drawFillLine(line);
    vir_offset = SETTING_MENU_START_COL;
    drawAdafruit.drawString("   |     ", vir_offset, line);

    ++line;
    drawAdafruit.drawFillLine(line);
    vir_offset = SETTING_MENU_START_COL;
    drawAdafruit.drawString(modeNames[(uint8_t)disChargeMode], SETTING_MENU_START_COL - 1, line);

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
    drawAdafruit.drawFillLine(line);
    vir_offset = DISPLAY_MENU_START_COL;
    drawAdafruit.drawString(modeNames[(uint8_t)disChargeMode], vir_offset + 3, line);

    ++line;
    drawAdafruit.drawFillLine(line);
    vir_offset = DISPLAY_MENU_START_COL;
    vir_offset += DISPLAY_MENU_OFFSET_COL;
    drawAdafruit.drawFloatR(sleepV, vir_offset, line, DISPLAY_MENU_OFFSET_COL, 3);
    drawAdafruit.drawString("V", vir_offset, line);

    vir_offset += 2;
    if ((disChargeMode != DisChargeMode::DischargeNone) && (tunedI > 0.f) && (displayCount % 2))
    {

    }
    else
    {
      drawAdafruit.drawString("=>", vir_offset, line);
    }

    vir_offset += 3;
    vir_offset += DISPLAY_MENU_OFFSET_COL;
    drawAdafruit.drawFloatR(targetV, vir_offset, line, DISPLAY_MENU_OFFSET_COL, 3);
    drawAdafruit.drawString("V", vir_offset, line);

    ++line;
    drawAdafruit.drawFillLine(line);
    vir_offset = DISPLAY_MENU_START_COL;
    vir_offset += DISPLAY_MENU_OFFSET_COL;
    float displayI{std::max(0.f, tunedI)};
    drawAdafruit.drawFloatR(displayI, vir_offset, line, DISPLAY_MENU_OFFSET_COL, 3);
    drawAdafruit.drawString("A", vir_offset, line);

    vir_offset += 2;
    drawAdafruit.drawString("=>", vir_offset, line);

    vir_offset += 3;
    vir_offset += SETTING_MENU_OFFSET_COL;
    drawAdafruit.drawFloatR(targetI, vir_offset, line, SETTING_MENU_OFFSET_COL, 3);
    drawAdafruit.drawString("A", vir_offset, line);

    if (0)
    {
      ++line;
      drawAdafruit.drawFillLine(line);
      drawAdafruit.drawInt(calcPWMValue(targetI), SETTING_MENU_START_COL, line);
      drawAdafruit.drawString("PWM", SETTING_MENU_START_COL + SETTING_MENU_OFFSET_COL, line);
    }

    if (0)
    {
      ++line;
      drawAdafruit.drawFillLine(line);
      vir_offset = DISPLAY_MENU_START_COL;
      vir_offset += DISPLAY_MENU_OFFSET_COL;
      drawAdafruit.drawFloatR(sleepV, vir_offset, line, DISPLAY_MENU_OFFSET_COL, 3);
      drawAdafruit.drawString("V", vir_offset, line);
    }

    if (0)
    {
      ++line;
      drawAdafruit.drawFillLine(line);
      vir_offset = DISPLAY_MENU_START_COL;
      vir_offset += DISPLAY_MENU_OFFSET_COL;
      drawAdafruit.drawFloatR(I, vir_offset, line, DISPLAY_MENU_OFFSET_COL, 3);
      drawAdafruit.drawString("A", vir_offset, line);
    }

    if (0)
    {
      ++line;
      drawAdafruit.drawFillLine(line);
      vir_offset = DISPLAY_MENU_START_COL;
      vir_offset += DISPLAY_MENU_OFFSET_COL;
      static std::vector<String> modeNames{String("None"), String("Active"), String("Sleep"), String("Stop"), String("NoBat"), String("Max")};
      drawAdafruit.drawString(modeNames[(uint8_t)currentBatteryStatus], vir_offset + 5, line);
    }

  }

  ValueCounter valueCounter{};

  BatteryStatus currentBatteryStatus{BatteryStatus::None};
  BatteryStatus nextBatteryStatus{BatteryStatus::None};

  bool settingFlag{false};

  uint8_t batteryIndex;
  uint8_t readPin{0};
  uint8_t writePin{0};
  unsigned long startTime{0};

  float V{0.f};
  float sleepV{0.f};
  float targetV{1.40f};
  float I{0.0f};
  float tunedI{0.2f};
  float targetI{0.2f};

  DisChargeMode disChargeMode{DisChargeMode::DischargeNone};
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

  std::vector<BatteryInfo> batteryStatuses{
  BatteryInfo{READ1_PIN, WRITE1_PIN, 0},
  BatteryInfo{READ2_PIN, WRITE2_PIN, 1},
  BatteryInfo{READ3_PIN, WRITE3_PIN, 2},
  BatteryInfo{READ4_PIN, WRITE4_PIN, 3}};

  DisplaySettingMode displaySettingMode{DisplaySettingMode::Display};

  unsigned long loopSubMillis{ 0 };

  size_t currentBatteryIndex{0};

  unsigned long loopSubCount{ 0 };

  struct SaveBattery
  {
    float targetV{0.f};
    float targetI{0.f};
    DisChargeMode disChargeMode{DisChargeMode::DischargeNone};
  }; 

  struct SaveData
  {
    int ver{0};
    SaveBattery battery[4];
  };

public:
  SaveData saveData{};
private:

  void saveRomData()
  {
    byte* p = (byte*) &saveData;

    for(int i = 0; i < sizeof(SaveData); i++)
    {
      EEPROM.write(i, *p);
      p++;
    }
  };

  void loadRomData()
  {
    byte* p = (byte*) &saveData;

    for(int i = 0; i < sizeof(SaveData); i++)
    {
      byte b = EEPROM.read(i);
      *p = b;
      p++;
    }
  };

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

    loadRomData();

    for (int i{0}; i < batteryStatuses.size(); ++i)
    {
      auto& batteryStatus{batteryStatuses[i]};
      if (saveData.ver != -1)
      {

        auto& saveBattery{saveData.battery[i]};

        batteryStatus.targetI = saveBattery.targetI;
        batteryStatus.targetV = saveBattery.targetV;
        batteryStatus.disChargeMode = saveBattery.disChargeMode;
      }
      batteryStatus.setup();
    }
  };

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
  };

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
  };

  void changeDisplayMode(int shift)
  {
    const int nextModeIndex{(static_cast<int>(displaySettingMode) + shift) % static_cast<int>(DisplaySettingMode::Max)};

    displaySettingMode = static_cast<DisplaySettingMode>(nextModeIndex);
  };

  void shiftParam(int shift)
  {
    batteryStatuses[currentBatteryIndex].shiftParam(displaySettingMode, shift);

    saveData.ver = 0;
    for (int i{0}; i < batteryStatuses.size(); ++i)
    {
      auto& batteryStatus{batteryStatuses[i]};
      auto& saveBattery{saveData.battery[i]};

      saveBattery.targetI = batteryStatus.targetI;
      saveBattery.targetV = batteryStatus.targetV;
      saveBattery.disChargeMode = batteryStatus.disChargeMode;
    }

    saveRomData();

  };

  void loopSub()
  {

    digitalWrite(LED_BUILTIN, (++loopSubCount % 3));

    for (auto& batteryStatus : batteryStatuses)
    {
      batteryStatus.loopSub();
    }

    if ((loopSubCount % 3) == 0)
    {
      display();
      drawAdafruit.display();
    }

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
  };

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
  // while (!Serial);

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