// epdif.cpp: derived from Waveshare's 4.2" e-Paper V2 driver.
// Copyright (C) Waveshare 2017. MIT licensed; see THIRD_PARTY_LICENSES.

#include "epdif.h"
#include <SPI.h>

EpdIf::EpdIf() {
};

EpdIf::~EpdIf() {
};

void EpdIf::DigitalWrite(int pin, int value) {
    digitalWrite(pin, value);
}

int EpdIf::DigitalRead(int pin) {
    return digitalRead(pin);
}

void EpdIf::DelayMs(unsigned int delaytime) {
    delay(delaytime);
}

void EpdIf::SpiTransfer(unsigned char data) {
    digitalWrite(CS_PIN, LOW);
    SPI.transfer(data);
    digitalWrite(CS_PIN, HIGH);
}

int EpdIf::IfInit(void) {
    static bool spiStarted = false;
    pinMode(CS_PIN, OUTPUT);
    pinMode(RST_PIN, OUTPUT);
    pinMode(DC_PIN, OUTPUT);
    pinMode(BUSY_PIN, INPUT);
    if (!spiStarted) {
        SPI.begin();
        SPI.beginTransaction(SPISettings(2000000, MSBFIRST, SPI_MODE0));
        spiStarted = true;
    }
    return 0;
}

