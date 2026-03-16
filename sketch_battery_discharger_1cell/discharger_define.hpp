#pragma once

#undef SERIAL_DEBUG_ON
// #define SERIAL_DEBUG_ON

namespace DisplayConst
{
  static constexpr char CHAR_DATA_ARROW[] = ">";
  static constexpr char CHAR_DATA_ARROW_NEW[] = {0x1A, 0x00}; // "->"
  static constexpr char CHAR_DATA_UP[] = {0x18, 0x00};
  static constexpr char CHAR_DATA_DOWN[] = {0x19, 0x00};

}

static constexpr float VOLT3_3{3.3f};
static constexpr int SAVEDATA_ID{0xABCE};



