// epd4in2_V2.h: derived from Waveshare's 4.2" e-Paper V2 driver.
// Copyright (C) Waveshare 2017. MIT licensed; see THIRD_PARTY_LICENSES.

#ifndef EPD4IN2_H
#define EPD4IN2_H

#include "epdif.h"

// Display resolution
#define EPD_WIDTH       400
#define EPD_HEIGHT      300

class Epd : EpdIf {
public:
    unsigned int width;
    unsigned int height;

    Epd();
    ~Epd();
    int  Init(void);
    void SendCommand(unsigned char command);
    void SendData(unsigned char data);
    void ReadBusy(void);
    void Reset(void);
    void TurnOnDisplay(void);
    void TurnOnDisplay_Partial(void);
    void Display_Base(const unsigned char* Image);
    void Display_Partial(unsigned char* Image, unsigned int  Xstart, unsigned int  Ystart, unsigned int  Xend, unsigned int  Yend);
    void Sleep(void);
	
	

private:
    unsigned int reset_pin;
    unsigned int dc_pin;
    unsigned int cs_pin;
    unsigned int busy_pin;
};

#endif /* EPD4IN2_H */

/* END OF FILE */
