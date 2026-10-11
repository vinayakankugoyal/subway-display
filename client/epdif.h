// epdif.h: derived from Waveshare's 4.2" e-Paper V2 driver.
// Copyright (C) Waveshare 2017. MIT licensed; see THIRD_PARTY_LICENSES.

#ifndef EPDIF_H
#define EPDIF_H

#include <Arduino.h>

// Pin definition
#define RST_PIN         22
#define DC_PIN          21
#define CS_PIN          5
#define BUSY_PIN        4

class EpdIf {
public:
    EpdIf(void);
    ~EpdIf(void);

    static int  IfInit(void);
    static void DigitalWrite(int pin, int value); 
    static int  DigitalRead(int pin);
    static void DelayMs(unsigned int delaytime);
    static void SpiTransfer(unsigned char data);
};

#endif
