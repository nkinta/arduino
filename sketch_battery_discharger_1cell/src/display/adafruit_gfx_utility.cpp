#include "adafruit_gfx_utility.hpp"

#include "../../discharger_define.hpp"

String AdafruitGfxUtility::formatFloatZeroPad(float value, int integerDigits, int decimalDigits)
{
    if (integerDigits < 1)
    {
        integerDigits = 1;
    }

    if (decimalDigits < 0)
    {
        decimalDigits = 0;
    }

    String formatted{String(value, decimalDigits)};
    const bool isNegative{formatted.startsWith("-")};
    const int signOffset{isNegative ? 1 : 0};
    const int dotIndex{formatted.indexOf('.')};
    const int integerEnd{dotIndex >= 0 ? dotIndex : formatted.length()};
    const int currentIntegerDigits{integerEnd - signOffset};
    const int zeroCount{max(0, integerDigits - currentIntegerDigits)};

    if (zeroCount <= 0)
    {
        return formatted;
    }

    String zeros;
    zeros.reserve(zeroCount);
    for (int i = 0; i < zeroCount; ++i)
    {
        zeros += '0';
    }

    if (isNegative)
    {
        return "-" + zeros + formatted.substring(1);
    }

    return zeros + formatted;
}

void AdafruitGfxUtility::setDisplayTuneMenu(Adafruit_SSD1306 &display, String &&title, std::vector<String> &menuList, std::vector<String> &valueList, int targetIndex)
{
    int virOffset1{0};
    int virOffset2{0};
    int line{0};
    drawFillLine(display, line);
    drawStringC(display, title, line);
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

        drawFillLine(display, line);
        if (i == targetIndex)
        {
            drawChar(display, &DisplayConst::CHAR_DATA_ARROW[0], 0, line);
        }
        drawString(display, menuList[i], virOffset1, line);
        drawString(display, valueList[i], virOffset2, line);
        ++index;
        ++line;
    }

    while (line < 7)
    {
        drawFillLine(display, line);
        line++;
    }
}

void AdafruitGfxUtility::drawCar(Adafruit_SSD1306 &display)
{
// 'untitled', 16x16px
unsigned char epd_bitmap_untitled[] PROGMEM = {
    0xff, 0xff, 0x7f, 0xff, 0xbf, 0xff, 0xa0, 0x3f, 0xdf, 0xdf, 0xbf, 0x23, 0xbf, 0xfd, 0xbf, 0xfe,
    0xa7, 0xe6, 0xdb, 0xda, 0xd8, 0x19, 0xe7, 0xe7, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};

// Array of all bitmaps for convenience. (Total bytes used to store images in PROGMEM = 48)
const int epd_bitmap_allArray_LEN = 1;
const unsigned char* epd_bitmap_allArray[1] = {
    epd_bitmap_untitled
};

display.drawBitmap(3, 3, &epd_bitmap_untitled[0], 16, 16, WHITE);
}

void AdafruitGfxUtility::drawBat(Adafruit_SSD1306 &display, const int index)
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
    display.drawBitmap(110, 55, clear, 16, 8, BLACK);
    display.drawBitmap(110, 55, bat_meter[index], 16, 8, WHITE);
}

void AdafruitGfxUtility::dumpDisplayAsPbm(Adafruit_SSD1306 &display, Stream& out)
{
    uint8_t* buffer{display.getBuffer()};

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
