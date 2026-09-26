/**
 *  @filename   :   epd4in2_V2.cpp
 *  @brief      :   Implements for Dual-color e-paper library
 *  @author     :   Yehui from Waveshare
 *
 *  Copyright (C) Waveshare     August 10 2017
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documnetation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to  whom the Software is
 * furished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS OR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdlib.h>
#include "epd4in2_V2.h"

Epd::~Epd() {
};

Epd::Epd() {
    reset_pin = RST_PIN;
    dc_pin = DC_PIN;
    cs_pin = CS_PIN;
    busy_pin = BUSY_PIN;
    width = EPD_WIDTH;
    height = EPD_HEIGHT;
};

int Epd::Init(void) {
    /* this calls the peripheral hardware interface, see epdif */
    if (IfInit() != 0) {
        return -1;
    }
    /* EPD hardware init start */
    Reset();
    ReadBusy();

    SendCommand(0x12);
    ReadBusy();

    SendCommand(0x21);
    SendData(0x40);
    SendData(0x00);

    SendCommand(0x3C);
    SendData(0x05);

    SendCommand(0x11);
    SendData(0x03);

    SendCommand(0x44); 
    SendData(0x00);
    SendData(0x31); 
    
    SendCommand(0x45);
    SendData(0x00);
    SendData(0x00);  
    SendData(0x2B);
    SendData(0x01);

    SendCommand(0x4E); 
    SendData(0x00);

    SendCommand(0x4F); 
    SendData(0x00);
    SendData(0x00);  
    ReadBusy();
    
    /* EPD hardware init end */
    return 0;
}

/**
 *  @brief: basic function for sending commands
 */
void Epd::SendCommand(unsigned char command) {
    DigitalWrite(dc_pin, LOW);
    SpiTransfer(command);
}

/**
 *  @brief: basic function for sending data
 */
void Epd::SendData(unsigned char data) {
    DigitalWrite(dc_pin, HIGH);
    SpiTransfer(data);
}

/**
 *  @brief: Wait until the busy_pin goes HIGH
 */
void Epd::ReadBusy(void) {
    while(DigitalRead(busy_pin) == 1) {      //1: busy, 0: idle
        // Wait
    }      
}

/**
 *  @brief: module reset. 
 *          often used to awaken the module in deep sleep, 
 *          see Epd::Sleep();
 */
void Epd::Reset(void) {
    DigitalWrite(reset_pin, HIGH);
    DelayMs(100);
    DigitalWrite(reset_pin, LOW);
    DelayMs(2);
    DigitalWrite(reset_pin, HIGH);
    DelayMs(100);   
}

/**
 *  @brief: Turn On Display
 */
void Epd::TurnOnDisplay(void)
{
    SendCommand(0x22);
	SendData(0xF7);
    SendCommand(0x20);
    ReadBusy();
}

void Epd::TurnOnDisplay_Partial(void)
{
    SendCommand(0x22);
	SendData(0xFF);
    SendCommand(0x20);
    ReadBusy();
}

void Epd::Display_Base(const unsigned char* Image)
{
	unsigned int Width, Height;
	Width = (width % 8 == 0)? (width / 8 ): (width / 8 + 1);
	Height = height;

	SendCommand(0x24);   //Write Black and White image to RAM
	for (unsigned int j = 0; j < Height; j++) {
        for (unsigned int i = 0; i < Width; i++) {
            SendData(pgm_read_byte(&Image[i + j * Width]));
        }
	}
	SendCommand(0x26);   //Write Black and White image to RAM
	for (unsigned int j = 0; j < Height; j++) {
        for (unsigned int i = 0; i < Width; i++) {
            SendData(pgm_read_byte(&Image[i + j * Width]));
        }
	}
	TurnOnDisplay();	
}

void Epd::Display_Partial(unsigned char* Image, unsigned int  Xstart, unsigned int  Ystart, unsigned int  Xend, unsigned int  Yend)
{

    unsigned int  i, Width;
    unsigned int  IMAGE_COUNTER;

    if((Xstart % 8 + Xend % 8 == 8 && Xstart % 8 > Xend % 8) || Xstart % 8 + Xend % 8 == 0 || (Xend - Xstart)%8 == 0)
	{
        Xstart = Xstart / 8 ;
        Xend = Xend / 8;
	}
	else
	{
        Xstart = Xstart / 8 ;
        Xend = Xend % 8 == 0 ? Xend / 8 : Xend / 8 + 1;
	}
	
	Width = Xend -  Xstart;
	IMAGE_COUNTER = Width * (Yend-Ystart);
    Serial.println(IMAGE_COUNTER);

	Xend -= 1;
	Yend -= 1;	
	//Reset
	Reset();

	SendCommand(0x3C); //BorderWavefrom
	SendData(0x80);	
    
    SendCommand(0x21); 
	SendData(0x00);
	SendData(0x00);

	SendCommand(0x3C); 
	SendData(0x80); 
	//	    
	SendCommand(0x44);       // set RAM x address start/end, in page 35
	SendData(Xstart & 0xff);    // RAM x address start at 00h;
	SendData(Xend & 0xff);    // RAM x address end at 0fh(15+1)*8->128 
	SendCommand(0x45);       // set RAM y address start/end, in page 35
	SendData(Ystart & 0xff);    // RAM y address start at 0127h;
	SendData((Ystart>>8) & 0x01);    // RAM y address start at 0127h;
	SendData(Yend & 0xff);    // RAM y address end at 00h;
	SendData((Yend>>8) & 0x01); 

	SendCommand(0x4E);   // set RAM x address count to 0;
	SendData(Xstart & 0xff); 
	SendCommand(0x4F);   // set RAM y address count to 0X127;    
	SendData(Ystart & 0xff);
	SendData((Ystart>>8) & 0x01);


	SendCommand(0x24);   //Write Black and White image to RAM
	for (i = 0; i < IMAGE_COUNTER; i++) {
		SendData(Image[i]);
	}
	TurnOnDisplay_Partial();
}

void Epd::Sleep() {
    SendCommand(0x10);  
    SendData(0x01);        
    DelayMs(100);          
}



/* END OF FILE */
