#pragma once

namespace DisplayConst
{

  const std::vector<String> DISC_MODE_NAMES{String("Cont"), String("Stop")};

  static constexpr char CHAR_DATA_ARROW[] = ">";
  static constexpr char CHAR_DATA_ARROW_NEW[] = {0x1A, 0x00}; // "->"
  static constexpr char CHAR_DATA_UP[] = {0x18, 0x00};
  static constexpr char CHAR_DATA_DOWN[] = {0x19, 0x00};

}

static const float VOLT3_3{3.3f};

static constexpr int SAVEDATA_ID{0xABCE};

