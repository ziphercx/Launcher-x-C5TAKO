#include "hal/bright/bright.h"
#include "hal/device.h"
#include "hal/inputs/buttons.h"
#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <driver/rtc_io.h>
#include <esp_sleep.h>
#include <interface.h>

namespace {
bool sdMountedBeforeDisplay = false;
constexpr uint8_t directionPins[] = {UP_BTN, NEXT_BTN, DW_BTN, PREV_BTN};
DeviceButtons buttonsCfg() {
    const unsigned r = static_cast<unsigned>(rotation) % 4;
    return DeviceButtons{
        directionPins[(3 + r) % 4],
        directionPins[(1 + r) % 4],
        directionPins[r],
        directionPins[(2 + r) % 4],
        SEL_BTN
    };
}
const HalBrightCurve backlightCurve{0, 255, 1.0f};
} // namespace

void _setBrightness(uint8_t value) { hal_bright_set(TFT_BL, value, backlightCurve); }

void _setup_gpio() {
    hal_buttons_init(buttonsCfg(), 5);
    for (uint8_t pin : {TFT_CS, SDCARD_CS, CC1101_CS}) {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, HIGH);
    }
    pinMode(TFT_DC, OUTPUT);
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, LOW);
    pinMode(BUZZ_PIN, OUTPUT);
    digitalWrite(BUZZ_PIN, LOW);
    pinMode(LED, OUTPUT);
    digitalWrite(LED, LED_OFF); // XIAO user LED is active low, not addressable RGB.
    pinMode(BAT_VOLT_PIN, INPUT);
    pinMode(BAT_VOLT_PIN_EN, OUTPUT);
    digitalWrite(BAT_VOLT_PIN_EN, LOW);
    analogReadResolution(12);
    analogSetPinAttenuation(BAT_VOLT_PIN, ADC_11db);

    // Initialize SD before ST7789, as in XC5; all three devices use the same SPI instance.
    SPI.begin(TFT_SCLK, TFT_MISO, TFT_MOSI);
    sdMountedBeforeDisplay = SD.begin(SDCARD_CS, SPI, 4000000);
}

void _post_setup_gpio() {
    // main.cpp clears this flag between GPIO setup and TFT initialization.
    sdcardMounted = sdMountedBeforeDisplay;
    hal_bright_attach(TFT_BL);
    _setBrightness(bright);
}

void InputHandler() { hal_buttons_poll_5(buttonsCfg()); }

int getBattery() {
    static unsigned long sampledAt = 0;
    static int percent = 0;
    if (sampledAt && millis() - sampledAt < 3000) return percent;
    digitalWrite(BAT_VOLT_PIN_EN, HIGH);
    delay(3);
    uint32_t millivolts = 0;
    for (int i = 0; i < 16; ++i) millivolts += analogReadMilliVolts(BAT_VOLT_PIN);
    digitalWrite(BAT_VOLT_PIN_EN, LOW);
    const float voltage = millivolts * 2.0f / 16000.0f;
    constexpr float volts[] = {
        3.20f, 3.30f, 3.50f, 3.60f, 3.70f, 3.75f, 3.80f, 3.85f, 3.90f, 3.95f, 4.00f, 4.08f, 4.15f
    };
    constexpr int levels[] = {0, 2, 5, 10, 20, 30, 40, 55, 65, 75, 85, 95, 100};
    percent = 0;
    if (voltage >= 2.50f && voltage <= 4.35f) {
        percent = 100;
        for (unsigned i = 1; i < sizeof(volts) / sizeof(volts[0]); ++i) {
            if (voltage > volts[i]) continue;
            percent = constrain(
                (int)round(
                    levels[i - 1] +
                    (voltage - volts[i - 1]) * (levels[i] - levels[i - 1]) / (volts[i] - volts[i - 1])
                ),
                0,
                100
            );
            break;
        }
    }
    sampledAt = millis();
    return percent;
}

void powerOff() {
    // The carrier has no software power latch; power-off enters deep sleep, OK wakes it.
    while (digitalRead(SEL_BTN) == BTN_ACT) delay(10);
    _setBrightness(0);
    // Arduino_GFX wrapper writecommand/sleep are no-ops; send ST7789 SLPIN directly.
    SPI.beginTransaction(SPISettings(40000000, MSBFIRST, SPI_MODE3));
    digitalWrite(TFT_DC, LOW);
    digitalWrite(TFT_CS, LOW);
    SPI.transfer(0x10);
    digitalWrite(TFT_CS, HIGH);
    digitalWrite(TFT_DC, HIGH);
    SPI.endTransaction();
    delay(120);
    noTone(BUZZ_PIN);
    digitalWrite(BAT_VOLT_PIN_EN, LOW);
    rtc_gpio_pullup_en((gpio_num_t)SEL_BTN);
    rtc_gpio_pulldown_dis((gpio_num_t)SEL_BTN);
    esp_sleep_enable_ext1_wakeup_io(1ULL << SEL_BTN, ESP_EXT1_WAKEUP_ANY_LOW);
    esp_deep_sleep_start();
}

void reboot() { ESP.restart(); }
