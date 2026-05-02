#include "display/draw_adafruit.hpp"

#include "discharger_define.hpp"

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

void DrawAdafruit::drawBat(const float voltage)
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
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    };

    const unsigned char* bat_meter[4] = {
      bat0, bat1, bat2, bat3
    };

    uint8_t index{0};
    if (voltage > 4.05f)
    {
      index = 3;
    }
    else if (voltage > 3.85f)
    {
      index = 2;
    }
    else if (voltage > 3.6f)
    {
      index = 1;
    }
    else
    {
      index = 0;
    }

    // drawFillLine(6);
    // drawFloat(voltage, 10, 6);
    _display.drawBitmap(110, 55, clear, 16, 8, BLACK);
    _display.drawBitmap(110, 55, bat_meter[index], 16, 8, WHITE);
}
