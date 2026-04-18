#pragma once

struct VoltageMapping
{
  struct VoltPair
  {
    int input{0.f};
    float volt{0};
  };

  float getVoltage(int input) const
  {
    const VoltPair *before{nullptr};
    for (const VoltPair &current : mappingData)
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
    return 0.f;
  }

  void initMapping(const std::vector<int> &customOffsetVolt)
  {
    mappingData = defaultMappingData;
    for (int i{0}; i < customOffsetVolt.size(); ++i)
    {
      mappingData[i].input -= customOffsetVolt[i];
    }
  }

private:
  static constexpr float REG_A = 1.f;
  static constexpr float REG_B = 100.f;
  static constexpr float REG_RATE = REG_B / (REG_A + REG_B);
  const std::vector<VoltPair> defaultMappingData{{0, 0.f}, {static_cast<int>(621 * REG_RATE), 0.5f}, {static_cast<int>(1241 * REG_RATE), 1.0f}, {static_cast<int>(1862 * REG_RATE), 1.5f}, {static_cast<int>(2482 * REG_RATE), 2.0f}, {static_cast<int>(4094 * REG_RATE), VOLT3_3}};
  std::vector<VoltPair> mappingData{defaultMappingData};
};