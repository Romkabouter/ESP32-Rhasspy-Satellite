#include <Arduino.h>
//#include <WiFi.h>
#include "EverloopState.h"
#include "wishbone_bus.h"
#include "everloop.h"
#include "everloop_image.h"

static matrix_hal::Everloop everloop;
static matrix_hal::EverloopImage image1d;

// This is used to be able to change brightness, while keeping the colors appear
// the same Called gamma correction, check this
// https://learn.adafruit.com/led-tricks-gamma-correction/the-issue
const uint8_t PROGMEM gamma8[] = {
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   1,   1,
    1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   2,   2,   2,   2,
    2,   2,   2,   2,   3,   3,   3,   3,   3,   3,   3,   4,   4,   4,   4,
    4,   5,   5,   5,   5,   6,   6,   6,   6,   7,   7,   7,   7,   8,   8,
    8,   9,   9,   9,   10,  10,  10,  11,  11,  11,  12,  12,  13,  13,  13,
    14,  14,  15,  15,  16,  16,  17,  17,  18,  18,  19,  19,  20,  20,  21,
    21,  22,  22,  23,  24,  24,  25,  25,  26,  27,  27,  28,  29,  29,  30,
    31,  32,  32,  33,  34,  35,  35,  36,  37,  38,  39,  39,  40,  41,  42,
    43,  44,  45,  46,  47,  48,  49,  50,  50,  51,  52,  54,  55,  56,  57,
    58,  59,  60,  61,  62,  63,  64,  66,  67,  68,  69,  70,  72,  73,  74,
    75,  77,  78,  79,  81,  82,  83,  85,  86,  87,  89,  90,  92,  93,  95,
    96,  98,  99,  101, 102, 104, 105, 107, 109, 110, 112, 114, 115, 117, 119,
    120, 122, 124, 126, 127, 129, 131, 133, 135, 137, 138, 140, 142, 144, 146,
    148, 150, 152, 154, 156, 158, 160, 162, 164, 167, 169, 171, 173, 175, 177,
    180, 182, 184, 186, 189, 191, 193, 196, 198, 200, 203, 205, 208, 210, 213,
    215, 218, 220, 223, 225, 228, 231, 233, 236, 239, 241, 244, 247, 249, 252,
    255};

EverloopState::EverloopState(matrix_hal::WishboneBus *wb)
{
    everloop.Setup(wb);
}

void EverloopState::enterState()
{
    writeLeds();
    // int r = colors[0], g = colors[1], b = colors[2], w = colors[3], br = brightness * 90 / 100 + 10;
    // r = floor(br * r / 100);
    // r = pgm_read_byte(&gamma8[r]);
    // g = floor(br * g / 100);
    // g = pgm_read_byte(&gamma8[g]);
    // b = floor(br * b / 100);
    // b = pgm_read_byte(&gamma8[b]);
    // w = floor(br * w / 100);
    // w = pgm_read_byte(&gamma8[w]);    
    // for (matrix_hal::LedValue &led : image1d.leds) {
    //     led.red = r;
    //     led.green = g;
    //     led.blue = b;
    //     led.white = w;    
    // }
    // everloop.Write(&image1d);
}

bool EverloopState::run()
{
    //if (WiFi.isConnected()) {
       //  colors[0] = 0;
    //     colors[2] = 255;
    //} else {
    //    colors[0] = 0;
    //    colors[1] = 255;
    //}
    //update everloop and return

    // int r = colors[0], g = colors[1], b = colors[2], w = colors[3], br = brightness * 90 / 100 + 10;
    // r = floor(br * r / 100);
    // r = pgm_read_byte(&gamma8[r]);
    // g = floor(br * g / 100);
    // g = pgm_read_byte(&gamma8[g]);
    // b = floor(br * b / 100);
    // b = pgm_read_byte(&gamma8[b]);
    // w = floor(br * w / 100);
    // w = pgm_read_byte(&gamma8[w]);    
    // for (matrix_hal::LedValue &led : image1d.leds) {
    //     led.red = r;
    //     led.green = g;
    //     led.blue = b;
    //     led.white = w;    
    // }
    // everloop.Write(&image1d);    
    //writeLeds();    
    if (ran == true) {
        return false;
    }
    brightness = 100;
    ran = true;
    writeLeds();
    return true;

    // nothing detected stay in the current state
    //return false;
}
void EverloopState::exitState()
{

}

void EverloopState::writeLeds() {
    int r = colors[0], g = colors[1], b = colors[2], w = colors[3], br = brightness * 90 / 100 + 10;
    r = floor(br * r / 100);
    r = pgm_read_byte(&gamma8[r]);
    g = floor(br * g / 100);
    g = pgm_read_byte(&gamma8[g]);
    b = floor(br * b / 100);
    b = pgm_read_byte(&gamma8[b]);
    w = floor(br * w / 100);
    w = pgm_read_byte(&gamma8[w]);    
    for (matrix_hal::LedValue &led : image1d.leds) {
        led.red = r;
        led.green = g;
        led.blue = b;
        led.white = w;    
    }
    everloop.Write(&image1d);
}