#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Fonts/FreeSansBold24pt7b.h>
#include "epd4in2_V2.h"

// Waveshare's driver treats 1 = white, 0 = black,
// so we draw on the canvas with the same convention.
#define WHITE 1
#define BLACK 0

Epd epd;
GFXcanvas1 canvas(400, 300);   // 15 KB off-screen buffer

void drawRow(int16_t start_x, int16_t top, int16_t bottom, char * train, char * direction, char * time) {
  // figure out the middle line
  int16_t true_center = top + ( bottom - top ) / 2;
  int16_t x1, y1; uint16_t w, h;
  canvas.getTextBounds(train, 0, 0, &x1, &y1, &w, &h);
  canvas.fillCircle(start_x, true_center, w, BLACK);
  canvas.setCursor(start_x - x1 - w/2, true_center - y1 - h/2);
  canvas.setTextColor(WHITE);
  canvas.print(train);
  canvas.setTextColor(BLACK);
  canvas.printf("  %s: %s", direction, time);
} 

void draw() {
  canvas.fillScreen(WHITE);
  canvas.setFont(&FreeSansBold24pt7b);
  canvas.setTextColor(BLACK);
  drawRow(40, 0, 150, "Q", "Manhattan", "2");
  canvas.fillRect(16, 149, 368, 3, BLACK);   // divider
  drawRow(40, 150, 300, "C", "Manhattan", "3");
}

void setup() {
  Serial.begin(115200);
  epd.Init();
  draw();
  epd.Display_Base(canvas.getBuffer());   // full refresh, sets the base image
  epd.Sleep();
}

void loop() {
}