#pragma once

#include <cstdint>

#undef SERIAL_DEBUG_ON
// #define SERIAL_DEBUG_ON

#undef  OLD_PCB
#define V2_PCB
#undef V1_PCB

namespace DisplayConst
{
  static constexpr char CHAR_DATA_ARROW[] = ">";
  static constexpr char CHAR_DATA_ARROW_NEW[] = {0x1A, 0x00}; // "->"
  static constexpr char CHAR_DATA_UP[] = {0x18, 0x00};
  static constexpr char CHAR_DATA_DOWN[] = {0x19, 0x00};

}

static constexpr float VOLT3_3{3.3f};
static constexpr int SAVEDATA_ID{0xABCE};

#if defined(V1_PCB) || defined(V2_PCB)
    static constexpr uint8_t READ1_PIN{18};
    static constexpr uint8_t READ2_PIN{17};
#else
    static constexpr uint8_t READ1_PIN{0};
    static constexpr uint8_t READ2_PIN{1};
#endif
    static constexpr uint8_t READ3_PIN{6};
    static constexpr uint8_t READ4_PIN{7};

    static constexpr uint8_t WRITE1_PIN{2};
    static constexpr uint8_t WRITE2_PIN{3};
    static constexpr uint8_t WRITE3_PIN{8};
    static constexpr uint8_t WRITE4_PIN{9};

#if defined(V1_PCB)
    static constexpr int PUSH_BUTTON_L{15}; // Left
    static constexpr int PUSH_BUTTON_D{0}; // Down
    static constexpr int PUSH_BUTTON_U{10}; // Up
    static constexpr int PUSH_BUTTON_R{13}; // Right
    static constexpr int PUSH_BUTTON_A{14}; // A Button 誤ってDeepsleepの無限ループ状態になった場合、書き込みできるようになる // Arduino15\packages\SiliconLabs\hardware\silabs\3.0.0\variants\xiao_mg24\pins_arduino.h // DEEP_SLEEP_ESCAPE_PIN
    static constexpr int PUSH_BUTTON_B{16}; // B Button
    static constexpr int PUSH_BUTTON_ON{1};

    static constexpr int PUSH_DISCHARGE_NO1{PUSH_BUTTON_L};
    static constexpr int PUSH_DISCHARGE_NO2{PUSH_BUTTON_A};
    static constexpr int PUSH_DISCHARGE_NO3{PUSH_BUTTON_R};
    static constexpr int PUSH_DISCHARGE_NO4{PUSH_BUTTON_B};

    static constexpr int WAKE_UP_PIN{PUSH_BUTTON_U}; // 14 16 0 10

#elif defined(V2_PCB)
    static constexpr int PUSH_BUTTON_L{15}; // Left
    static constexpr int PUSH_BUTTON_D{0}; // Down
    static constexpr int PUSH_BUTTON_U{1}; // Up
    static constexpr int PUSH_BUTTON_R{14}; // Right,誤ってDeepsleepの無限ループ状態になった場合、書き込みできるようになる // Arduino15\packages\SiliconLabs\hardware\silabs\3.0.0\variants\xiao_mg24\pins_arduino.h // DEEP_SLEEP_ESCAPE_PIN
    static constexpr int PUSH_BUTTON_A{13}; // A Button
    static constexpr int PUSH_BUTTON_B{16}; // B Button
    static constexpr int PUSH_BUTTON_ON{10};

    static constexpr int PUSH_DISCHARGE_NO1{PUSH_BUTTON_L};
    static constexpr int PUSH_DISCHARGE_NO2{PUSH_BUTTON_R};
    static constexpr int PUSH_DISCHARGE_NO3{PUSH_BUTTON_A};
    static constexpr int PUSH_DISCHARGE_NO4{PUSH_BUTTON_B};

    static constexpr int WAKE_UP_PIN{PUSH_BUTTON_ON}; // 14 16 0 10
#else
    static constexpr int PUSH_BUTTON_L{15};
    static constexpr int PUSH_BUTTON_D{14};
    static constexpr int PUSH_BUTTON_U{13}; // Arduino15\packages\SiliconLabs\hardware\silabs\3.0.0\variants\xiao_mg24\pins_arduino.h // DEEP_SLEEP_ESCAPE_PIN
    static constexpr int PUSH_BUTTON_R{12}; // 12(SAND11_RX) or 10
    static constexpr int PUSH_BUTTON_A{11};

    static constexpr int WAKE_UP_PIN{PUSH_BUTTON_D};
#endif

