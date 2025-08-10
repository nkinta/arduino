#pragma once

#include "display/draw_adafruit.h"

static constexpr int WRITE_POWER_PIN{ 20 };

static constexpr int POWER_MAP[] = { 30, 95, 100, 105, 110, 120, 250 };

static constexpr float SEC_TO_RPM{ 60.f };

static constexpr float TIRE_DIA{23.f};
static constexpr float GEAR_RATE{1.f / 4.f};

static constexpr float FPS{ 15.f };
static constexpr float SEC{ 1000.f };
static constexpr float ONE_FRAME_MS{ (1.f / FPS) * SEC };

DrawAdafruit drawAdafruit;

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

struct RunSimCache {

  double meter{0.f};
  double watt{0.f};
  double sec{0.f};

  void reset()
  {
    meter = 0.f;
    watt = 0.f;
    sec = 0.f;
  }

};

struct EvalTiming{

  EvalTiming(float timingDiff){
    nextDiff = timingDiff;
  };

public:
  bool isExecute(float millis) {
    const float diff{ millis - oldMillis };
    if (diff > nextDiff) {
      oldMillis = millis;
      return true;
    } else {
      return false;
    }
  }

  float nextDiff{0.f};

private:
  float oldMillis{0.f};
};


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

  static constexpr int POLE{ 14 };
  static constexpr int ROT_RATE{ POLE / 2 };
  static constexpr int GENE_GEAR_RATE{ 24 / 8 };
  static constexpr float PULSE_RATE{ static_cast<float>(GENE_GEAR_RATE) / static_cast<float>(ROT_RATE) };

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
      result = count * PULSE_RATE * SEC_TO_RPM * (SEC / static_cast<float>(totalDiffMillis));
    }
    reset();

    return result;
  }

  float calcRPS() {
    float result{0};
    if (totalDiffMillis > 0)
    {
      result = count * PULSE_RATE * (SEC / static_cast<float>(totalDiffMillis));
    }
    reset();

    return result;
  }  

  void reset() {
    count = 0;
    totalDiffMillis = 0;
  }
};

struct BaseCheckParam {
  RotateCounter rotateCounter{};
  ValueCounter iValueCounter{};
  ValueCounter vValueCounter{};

  float iOffsetVoltage{ 0.f };

  static void drawRpm(int OFFSET, int lineIndex, const RPMCache& rpmCache)
  {
      drawAdafruit.drawFillLine(2 * lineIndex + OFFSET);
      drawAdafruit.drawRPM(rpmCache.rpm, 10, 2 * lineIndex + OFFSET);

      drawAdafruit.drawFillLine(2 * lineIndex + OFFSET + 1);
      drawAdafruit.drawV(rpmCache.vValue, 8, 2 * lineIndex + OFFSET + 1);
      drawAdafruit.drawI(max(rpmCache.iValue, 0.f), 15, 2 * lineIndex + OFFSET + 1);
  };

};

struct FreeCheckParam : public BaseCheckParam {

  static constexpr int TABLE[] = { 0, 1, 2, 3, 4, 5, 6};
  static constexpr int TABLE_COUNT{ sizeof(TABLE) / sizeof(int) };

  int powerNum{ 0 };

  bool nextFlag{ false };

  EvalTiming evalATiming{ONE_FRAME_MS};
  EvalTiming evalBTiming{SEC};

  static constexpr int RPM_CACHE_COUNT{ 2 };

  RPMCache rpmCaches[RPM_CACHE_COUNT]{};

  void reset() {
    for (int i = 0; i < RPM_CACHE_COUNT; ++i) {
      rpmCaches[i].reset();
    }
    powerNum = 0;
  };

  void pushButton2() {
    nextFlag = true;
  };

  void execB() {
    RPMCache& currentCache{ rpmCaches[0] };
    currentCache.rpm = rotateCounter.calcRPM();
    currentCache.vValue = calcVValue(vValueCounter.calcValue());
    currentCache.iValue = calcIValue(iValueCounter.calcValue(), iOffsetVoltage);

    RPMCache& maxCache{ rpmCaches[1] };
    if (currentCache.rpm > maxCache.rpm) {
      maxCache = currentCache;
    }

    static const int OFFSET{ 1 };
    static const int ROW_INDEX{ 2 };
    for (int i = 0; i < RPM_CACHE_COUNT; ++i) {
      drawRpm(OFFSET, i, rpmCaches[i]);
    }
  };

  void execA() {
    next();
    drawAdafruit.drawChar("T", 0, 0, 1);
    drawAdafruit.drawInt(powerNum, 1, 0);
    analogWrite(WRITE_POWER_PIN, getPower());
  };

private:

  int getPower() {
    int power = constrain(POWER_MAP[TABLE[powerNum]], 0, 255);
    return power;
  };

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

struct RunSimCheckParam : public BaseCheckParam {
  enum class StateMode {
    SleepMode = 0,
    RunMode
  };

  struct RunStatus{
    float meter{0.f};
    int powerIndex{0.f};
  };

  struct RunProfile{
    String name{};
    std::vector<RunStatus> runStatuses;
  };

  static constexpr float METER_RATE{GEAR_RATE * TIRE_DIA * PI * (1.f / 1000.f)};
  static constexpr float KIRO_RATE{METER_RATE / 1000.f};

  std::vector<RunProfile> runProfiles{
    {String("P0"), {{0.f, 6}, {2.f, 1}, {10.f, 3}, {25.f, 2}}},
    {String("P1"), {{0.f, 6}, {2.f, 2}, {10.f, 4}, {25.f, 3}}},
    {String("P2"), {{0.f, 5}, {3.f, 2}, {16.f, 6}, {18.f, 2}, {26.f, 6}, {28.f, 1}, {30.f, 3}}},
    {String("T0"), {{0.f, 0}}},
    {String("T1"), {{0.f, 1}}},
    {String("T2"), {{0.f, 2}}},
    {String("T3"), {{0.f, 3}}},
    {String("T4"), {{0.f, 4}}},
    {String("T5"), {{0.f, 5}}}
    };

  float oneCycleMeter{40.f}; 
  unsigned int cycleNumber{5};

  unsigned int currentStatusIndex{0};
  unsigned int currentProfileIndex{0};

  bool onStartFlag{ false };
  bool onChangeFlag{ false };

  EvalTiming evalATiming{ONE_FRAME_MS};
  EvalTiming evalBTiming{2 * SEC};

  StateMode currentMode{ StateMode::SleepMode };

  RunSimCache runSimCache{};
  RPMCache currentCache{};

  void reset() {
    runSimCache.reset();
    currentCache.reset();
    currentMode = StateMode::SleepMode;
    currentStatusIndex = 0;
    currentProfileIndex = 0;
  }

  void pushButton2() {
    onChangeFlag = true;
  }

  void pushButtonLong2() {
    onStartFlag = true;
  }  

  void execA() {
    loop();
    display();
    analogWrite(WRITE_POWER_PIN, getPower());
  }

  void execB() {

  }

private:

  void loop() {
    if (onStartFlag) {
      if (currentMode == StateMode::SleepMode) {
        currentMode = StateMode::RunMode;
        // reset();
        start();
      }
      else if (currentMode == StateMode::RunMode) {
        currentMode = StateMode::SleepMode;
      }
      onStartFlag = false;
      return;
    }

    if (onChangeFlag)
    {
      currentProfileIndex = (currentProfileIndex + 1) % runProfiles.size();
      currentStatusIndex = 0;
      onChangeFlag = false;
      return;
    }


    currentCache.rpm = rotateCounter.calcRPS();
    currentCache.vValue = calcVValue(vValueCounter.calcValue());
    currentCache.iValue = calcIValue(iValueCounter.calcValue(), iOffsetVoltage);

    if (currentMode == StateMode::RunMode)
    {
      constexpr float TOTAL_RATE{METER_RATE * (1.f / FPS)};
      const float diffMeter{currentCache.rpm * TOTAL_RATE};
      const float newMeter{runSimCache.meter + diffMeter};
      float rate{1.f};

      const float runMeter{oneCycleMeter * cycleNumber};
      if (newMeter > runMeter)
      {
        rate = (newMeter - runMeter) / diffMeter;
        runSimCache.meter = runMeter;
        currentMode = StateMode::SleepMode;
      }
      else
      {
        runSimCache.meter = newMeter;
      }

      constexpr float TOTAL_WATT_HOUR_RATE{1.f / (FPS * 60.f * 60.f)};
      runSimCache.watt += rate * currentCache.vValue * currentCache.iValue * TOTAL_WATT_HOUR_RATE;

      runSimCache.sec += rate * (1.f /FPS);
    }

  };

  void display() {
    static String sleepStr{"sleep"};
    static String RunStr{"run  "};

    if (currentMode == StateMode::RunMode) {
      drawAdafruit.drawString(RunStr, 0, 0);

      const std::vector<RunStatus>& runStatuses{runProfiles[currentProfileIndex].runStatuses};

      const int statusSize{runStatuses.size()};
      drawAdafruit.drawIntR((currentStatusIndex / statusSize) + 1, 11, 0);
      drawAdafruit.drawChar("C", 10, 0, 1);

      drawAdafruit.drawIntR(runStatuses[currentStatusIndex % statusSize].powerIndex, 14, 0);
      drawAdafruit.drawChar("T", 13, 0, 1);

    } else if (currentMode == StateMode::SleepMode) {
      drawAdafruit.drawString(sleepStr, 0, 0);
      drawAdafruit.drawString("         ", 6, 0);

      drawAdafruit.drawString(runProfiles[currentProfileIndex].name, 7, 0);
    }

    const int meterDrawRow{1};
    static float WATT_TO_MAH{(1.f / 2.4f) * 1000.f};

    drawAdafruit.drawFloatR(runSimCache.meter, 5, meterDrawRow, 5, 1);
    drawAdafruit.drawChar("m", 5, meterDrawRow, 1);

    drawAdafruit.drawFloatR(runSimCache.sec, 11, meterDrawRow, 5, 1);
    drawAdafruit.drawChar("s", 11, meterDrawRow, 1);

    const float milliAmpHour{runSimCache.watt * WATT_TO_MAH}; 
    drawAdafruit.drawFloatR(milliAmpHour, 17, meterDrawRow, 5, 1);
    drawAdafruit.drawChar("mah", 17, meterDrawRow, 3);

    const int rpmDrawRow{2};
    const float kiroMeter = currentCache.rpm * ((SEC_TO_RPM * 60.f) * KIRO_RATE);
    drawAdafruit.drawFloatR(kiroMeter, 4, rpmDrawRow, 4, 1);
    drawAdafruit.drawChar("km/h", 4, rpmDrawRow, 3);
    drawAdafruit.drawFloatR(currentCache.vValue, 13, rpmDrawRow, 4, 2);
    drawAdafruit.drawChar("v", 13, rpmDrawRow, 1);

    drawAdafruit.drawFloatR(max(currentCache.iValue, 0.f), 19, rpmDrawRow, 3, 1);
    drawAdafruit.drawChar("a", 19, rpmDrawRow, 1);

    const int rpmResultRow{3};
    const float runMeter{oneCycleMeter * cycleNumber};
    if (currentMode == StateMode::RunMode) {
      drawAdafruit.drawClear(rpmResultRow);
    } else if (currentMode == StateMode::SleepMode) {
      if (runSimCache.meter < runMeter)
      {
        drawAdafruit.drawClear(rpmResultRow);
      }
      else
      {
        static float AVE_KIROMETER_RATE{60.f * 60.f * (1.f / 1000.f)};
        const float averageMilliSec{runSimCache.meter / runSimCache.sec};
        const float averagekiroMeter{AVE_KIROMETER_RATE * averageMilliSec};
        drawAdafruit.drawFloatR(averagekiroMeter, 4, rpmResultRow, 4, 1);
        drawAdafruit.drawChar("km/h", 4, rpmResultRow, 3);

        drawAdafruit.drawFloatR(averageMilliSec, 12, rpmResultRow, 4, 1);
        drawAdafruit.drawChar("m/s", 12, rpmResultRow, 4);

        static float AVE_AMP{60.f * 60.f * (1.f / 1000.f)};
        const float averageAmp{AVE_AMP * (milliAmpHour / runSimCache.sec)};
        drawAdafruit.drawFloatR(averageAmp, 19, rpmResultRow, 3, 1);
        drawAdafruit.drawChar("a", 19, rpmResultRow, 1);
      }
    }

  }

  void start() {
    evalATiming.isExecute(millis());
    evalBTiming.isExecute(millis());    
    currentMode = StateMode::RunMode;
    currentStatusIndex = 0;
    runSimCache.reset();
  }

  int getPower() {
    int power{0};
    if (currentMode == StateMode::RunMode)
    {
      const std::vector<RunStatus>& runStatuses{runProfiles[currentProfileIndex].runStatuses};
      int statusSize{runStatuses.size()};

      unsigned int nextStatus{(currentStatusIndex + 1) % statusSize};
      unsigned int nextCycle{(currentStatusIndex + 1) / statusSize};

      if (runSimCache.meter > (runStatuses[nextStatus].meter + (oneCycleMeter * nextCycle)))
      {
        currentStatusIndex = currentStatusIndex + 1;
      }
      int powerIndex{runStatuses[currentStatusIndex % statusSize].powerIndex};
      power = POWER_MAP[powerIndex];
    }

    return power;
  }

};


struct TorqueCheckParam : public BaseCheckParam {
  enum class StateMode {
    SleepMode = 0,
    WaitMode,
    CalcMode,
    MaxStateMode,
  };

  bool onFlag{ false };
  bool blinkFlag{ false };

  EvalTiming evalATiming{ONE_FRAME_MS};
  EvalTiming evalBTiming{2 * SEC};

  int tableIndex{ 0 };
  static constexpr int TABLE[] = { 0, 3, 6 };
  static constexpr int TABLE_COUNT{ sizeof(TABLE) / sizeof(int) };

  RPMCache rpmCaches[TABLE_COUNT]{};

  StateMode currentMode{ StateMode::SleepMode };

  float iOffsetVoltage{ 0.f };

  void reset() {
    for (int i = 0; i < TABLE_COUNT; ++i) {
      rpmCaches[i].reset();
    }
    tableIndex = 0;
  }

  void pushButton2() {
    onFlag = true;
  }

  void execA() {
    loop();
    display();
    analogWrite(WRITE_POWER_PIN, getPower());
  }

  void execB() {
    next();
  }

private:
  void start() {
    evalATiming.isExecute(millis());
    evalBTiming.isExecute(millis());
    currentMode = StateMode::WaitMode;
    tableIndex = 0;

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
    int power = constrain(POWER_MAP[TABLE[tableIndex]], 0, 255);
    return power;
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
      currentCache.vValue = calcVValue(vValueCounter.calcValue());
      currentCache.iValue = calcIValue(iValueCounter.calcValue(), iOffsetVoltage);

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

    static String sleepStr{"sleep"};
    static String waitStr{"wait "};
    static String calcStr{"calc "};
    if (currentMode == StateMode::WaitMode) {
      drawAdafruit.drawString(waitStr, 0, 0);
    } else if (currentMode == StateMode::CalcMode) {
      drawAdafruit.drawString(calcStr, 0, 0);
    } else if (currentMode == StateMode::SleepMode) {
      drawAdafruit.drawString(sleepStr, 0, 0);
    }

    static const int OFFSET{ 1 };
    static const int ROW_NUM{ 2 };

    float baseRpm = rpmCaches[0].rpm;
    for (int i = 0; i < TABLE_COUNT; ++i) {
      const RPMCache& rpmCache{rpmCaches[i]};
      int rpmRate{100};
      if (baseRpm > 10.f)
      {
        rpmRate = constrain((rpmCache.rpm / baseRpm) * 100.f, 0, 100);
      }
      drawRpm(OFFSET, i, rpmCache);

      const int colIndex{ 2 * i + OFFSET };

      int blinkType{ 0 };
      if (currentMode == StateMode::CalcMode && i == tableIndex) {
        blinkType = 2;
      } else if (currentMode == StateMode::WaitMode && i == tableIndex) {
        blinkType = 1;
      }

      if (currentMode == StateMode::SleepMode)
      {
        drawAdafruit.drawIntR(rpmRate, 3, colIndex);
      }
      else
      {
        drawAdafruit.drawChar("   ", 0, colIndex, 3);
      }

      if (currentMode != StateMode::SleepMode && i == tableIndex)
      {
        const char* arrowMessage{ getDrawArrow(blinkFlag, blinkType) };
        drawAdafruit.drawChar(arrowMessage, 0, colIndex, ROW_NUM);
      }

      // drawAdafruit.drawInt(rpmCaches[i].maxRpm, 9, i + 1);
    }
  };
};