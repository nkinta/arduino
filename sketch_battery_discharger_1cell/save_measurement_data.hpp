#pragma once

#include <cstdint>

struct SaveMeasurementData
{
    static constexpr int SAVEDATA_ID{0xABD0};
    static constexpr int SAVEDATA_ADDRESS{0x200};

    int _id{SAVEDATA_ID};
    int _ver{1};
    uint16_t _discSeconds{60};
    uint16_t _restSeconds{60};
    float _current{2.f};
};
