#pragma once

enum class PushType : uint8_t
{
    None,
    ReleaseShort,
    ReleaseLong,
    Pushed,
    PushShort,
    PushLong,
    Max,
};

struct ButtonStatus
{
    static constexpr int LONG_PUSH_MILLIS{1000};

    void init(const int inPinID)
    {
        _pinId = inPinID;
    }

    PushType getVal() const
    {
        return _cachedType;
    }

    void update()
    {
        int val = digitalRead(_pinId);
        if (val == LOW)
        {
            _buttonFlag = true;
        }
        else
        {
            _buttonFlag = false;
        }

        bool releaseFlag{false};

        if (_buttonFlag && (_buttonOldFlag != _buttonFlag))
        {
            _pushedMillis = millis();
        }
        else if (!_buttonFlag && (_buttonOldFlag != _buttonFlag))
        {
            releaseFlag = true;
        }
        _buttonOldFlag = _buttonFlag;

        if (releaseFlag)
        {
            if ((millis() - _pushedMillis) > LONG_PUSH_MILLIS)
            {
                _cachedType = PushType::ReleaseLong;
                return;
            }
            else
            {
                _cachedType = PushType::ReleaseShort;
                return;
            }
        }
        else if (_buttonFlag)
        {
            if (millis() - _pushedMillis == 0)
            {
                _cachedType = PushType::Pushed;
                return;
            }
            else if ((millis() - _pushedMillis) < LONG_PUSH_MILLIS)
            {
                _cachedType = PushType::PushShort;
                return;
            }
            else
            {
                _cachedType = PushType::PushLong;
                return;
            }
        }

        _cachedType = PushType::None;
    }

    PushType _cachedType{0};
    int _pinId{0};
    bool _buttonFlag{false};
    bool _buttonOldFlag{false};
    unsigned long _pushedMillis{0};
};
