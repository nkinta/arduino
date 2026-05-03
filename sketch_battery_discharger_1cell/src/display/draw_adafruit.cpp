#include "draw_adafruit.hpp"

#include "../../discharger_define.hpp"

void DrawAdafruit::setDisplayTuneMenu(DrawAdafruit &drawAdafruit, String &&title, std::vector<String> &menuList, std::vector<String> &valueList, int targetIndex)
{
    int virOffset1{0};
    int virOffset2{0};
    int line{0};
    drawAdafruit.drawFillLine(line);
    drawAdafruit.drawStringC(title, line);
    ++line;

    virOffset1 = 2;
    virOffset2 = 12;

    constexpr int MAX_DISPLAY_LINE{4};
    int startIndex = std::max(0, targetIndex - MAX_DISPLAY_LINE);

    int index{0};
    for (int i{startIndex}; i < menuList.size(); ++i)
    {
        if (index > MAX_DISPLAY_LINE)
        {
            break;
        }

        drawAdafruit.drawFillLine(line);
        if (i == targetIndex)
        {
            drawAdafruit.drawChar(&DisplayConst::CHAR_DATA_ARROW[0], 0, line);
        }
        drawAdafruit.drawString(menuList[i], virOffset1, line);
        drawAdafruit.drawString(valueList[i], virOffset2, line);
        ++index;
        ++line;
    }

    while (line < 7)
    {
        drawAdafruit.drawFillLine(line);
        line++;
    }
}

void DrawAdafruit::drawBat(const int index)
{

    // https://javl.github.io/image2cpp/

    const unsigned char bat3[] PROGMEM = {
        0x7f, 0xfc, 0x80, 0x02, 0xbb, 0xbb, 0xbb, 0xb9, 0xbb, 0xb9, 0xbb, 0xbb, 0x80, 0x02, 0x7f, 0xfc,
    };

    const unsigned char bat2[] PROGMEM = {
        0x7f, 0xfc, 0x80, 0x02, 0xbb, 0x83, 0xbb, 0x81, 0xbb, 0x81, 0xbb, 0x83, 0x80, 0x02, 0x7f, 0xfc,
    };

    const unsigned char bat1[] PROGMEM = {
        0x7f, 0xfc, 0x80, 0x02, 0xb8, 0x03, 0xb8, 0x01, 0xb8, 0x01, 0xb8, 0x03, 0x80, 0x02, 0x7f, 0xfc,
    };

    const unsigned char bat0[] PROGMEM = {
        0x7f, 0xfc, 0x80, 0x02, 0x80, 0x03, 0x80, 0x01, 0x80, 0x01, 0x80, 0x03, 0x80, 0x02, 0x7f, 0xfc,
    };

    const unsigned char clear[] PROGMEM = {
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    };

    const unsigned char* bat_meter[4] = {
      bat0, bat1, bat2, bat3
    };

    // drawFillLine(6);
    // drawFloat(voltage, 10, 6);
    _display.drawBitmap(110, 55, clear, 16, 8, BLACK);
    _display.drawBitmap(110, 55, bat_meter[index], 16, 8, WHITE);
}

void DrawAdafruit::dumpDisplayAsPbm(Stream& out)
{
    uint8_t* buffer{_display.getBuffer()};

    out.print("P4\n");
    out.print(SCREEN_WIDTH);
    out.print(" ");
    out.print(SCREEN_HEIGHT);
    out.print("\n");

    for (int y = 0; y < SCREEN_HEIGHT; ++y)
    {
        for (int xByte = 0; xByte < (SCREEN_WIDTH / 8); ++xByte)
        {
            uint8_t outByte{0};
            for (int bit = 0; bit < 8; ++bit)
            {
                const int x{xByte * 8 + bit};
                const int page{y / 8};
                const int bitInPage{y % 8};
                const uint8_t src{buffer[page * SCREEN_WIDTH + x]};
                const bool pixelOn{((src >> bitInPage) & 0x01) != 0};
                if (pixelOn)
                {
                    outByte |= static_cast<uint8_t>(0x80 >> bit);
                }
            }
            out.write(outByte);
        }
    }
}
