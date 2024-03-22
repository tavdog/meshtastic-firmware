#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#define USB_VID 0x303a
#define USB_PID 0x1001

#define EXTERNAL_NUM_INTERRUPTS 46
#define NUM_DIGITAL_PINS 48
#define NUM_ANALOG_INPUTS 20

#define analogInputToDigitalPin(p) (((p) < 20) ? (analogChannelToDigitalPin(p)) : -1)
#define digitalPinToInterrupt(p) (((p) < 48) ? (p) : -1)
#define digitalPinHasPWM(p) (p < 46)

// The default Wire will be mapped to PMU and RTC
static const uint8_t SDA = -1;
static const uint8_t SCL = -1;

static const uint8_t SS = 10;
static const uint8_t MOSI = 11;
static const uint8_t MISO = 13;
static const uint8_t SCK = 12;

static const uint8_t TP_RESET = 21;
static const uint8_t TP_INIT = 16;

// ST7789 IPS TFT 170x320
static const uint8_t LCD_BL = 38;
static const uint8_t LCD_D0 = 39;
static const uint8_t LCD_D1 = 40;
static const uint8_t LCD_D2 = 41;
static const uint8_t LCD_D3 = 42;
static const uint8_t LCD_D4 = 45;
static const uint8_t LCD_D5 = 46;
static const uint8_t LCD_D6 = 47;
static const uint8_t LCD_D7 = 48;
static const uint8_t LCD_WR = 8;
static const uint8_t LCD_RD = 9;
static const uint8_t LCD_DC = 7;
static const uint8_t LCD_CS = 6;
static const uint8_t LCD_RES = 5;
static const uint8_t LCD_POWER_ON = 15;

// Default SPI will be mapped to Radio
// static const uint8_t MISO = 3;
// static const uint8_t SCK = 5;
// static const uint8_t MOSI = 6;
// static const uint8_t SS = 7;

// #define MOSI (11)
// #define SCK (14)
// #define MISO (2)
// #define CS (13)

// #define SDCARD_CS                   SPI_CS

#endif /* Pins_Arduino_h */
