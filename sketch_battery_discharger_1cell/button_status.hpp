#pragma once

enum class PushType : uint8_t
{
    None,
    ReleaseShort,
    ReleaseLong,
    PushShort,
    PushLong,
    Max,
};

struct ButtonStatus
{
    static constexpr int LONG_PUSH_MILLIS{1000};

    void init(const int inPinID)
    {
        pinID = inPinID;
    }

    PushType getVal() const
    {
        return cachedType;
    }

    void update()
    {
        int val = digitalRead(pinID);
        if (val == LOW)
        {
            buttonFlag = true;
        }
        else
        {
            buttonFlag = false;
        }

        bool releaseFlag{false};

        if (buttonFlag && (buttonOldFlag != buttonFlag))
        {
            pushedMillis = millis();
        }
        else if (!buttonFlag && (buttonOldFlag != buttonFlag))
        {
            releaseFlag = true;
        }
        buttonOldFlag = buttonFlag;

        if (releaseFlag)
        {
            if ((millis() - pushedMillis) > LONG_PUSH_MILLIS)
            {
                cachedType = PushType::ReleaseLong;
                return;
            }
            else
            {
                cachedType = PushType::ReleaseShort;
                return;
            }
        }
        else if (buttonFlag)
        {
            if ((millis() - pushedMillis) > LONG_PUSH_MILLIS)
            {
                cachedType = PushType::PushLong;
                return;
            }
            else
            {
                cachedType = PushType::PushShort;
                return;
            }
        }

        cachedType = PushType::None;
    }

    PushType cachedType{0};
    int pinID{0};
    bool buttonFlag{false};
    bool buttonOldFlag{false};
    unsigned long pushedMillis{0};
};
