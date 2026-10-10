#include <SPI.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Adafruit_GFX.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>
#include "epd4in2_V2.h"
#include "secrets.h"
#include "config.h"

// Waveshare's driver treats 1 = white, 0 = black,
// so we draw on the canvas with the same convention.
#define WHITE 1
#define BLACK 0

#define WIFI_HOSTNAME    "subway-display"
#define WIFI_TIMEOUT_MS  20000   // give up on one connect attempt after this
#define WIFI_RECHECK_MS  10000   // how often loop() looks for a dropped link

Epd epd;
GFXcanvas1 canvas(400, 300);   // 15 KB off-screen buffer

// One display line: "Q  to Canal  3".
struct Row {
  char    line[4];
  char    label[24];
  int     mins[MAX_MINS];
  uint8_t n_mins;
};

Row      rows[MAX_ROWS];
uint8_t  n_rows  = 0;    // rows actually filled by the last good fetch
uint32_t updated = 0;    // "updated" epoch from the server

// Human-readable stamp for the header. The server formats it, since it knows
// the real time and the timezone; the ESP32 has no clock of its own. It is
// only replaced on a successful fetch, so it visibly ages if polling breaks.
char updated_text[32] = "";

uint32_t shown_fp  = 0;  // fingerprint of what the panel currently shows
uint32_t last_full = 0;  // millis() of the last full refresh
uint32_t last_draw = 0;  // millis() of the last refresh of any kind

// Blocks until associated or timeout. Safe to call again to reconnect.
bool connectWiFi(uint32_t timeout_ms) {
  if (WiFi.status() == WL_CONNECTED) return true;

  Serial.printf("WiFi: connecting to \"%s\"", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(WIFI_HOSTNAME);
  WiFi.setSleep(false);          // always on USB power, so keep the radio awake
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  uint32_t started = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - started < timeout_ms) {
    delay(250);
    Serial.print('.');
  }
  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.printf("WiFi: failed, status %d\n", WiFi.status());
    return false;
  }
  Serial.printf("WiFi: connected in %lu ms\n", millis() - started);
  Serial.printf("  ip       %s\n", WiFi.localIP().toString().c_str());
  Serial.printf("  gateway  %s\n", WiFi.gatewayIP().toString().c_str());
  Serial.printf("  rssi     %d dBm\n", WiFi.RSSI());
  Serial.printf("  hostname %s\n", WiFi.getHostname());
  return true;
}

// GET ARRIVALS_URL and parse it into rows[]. Leaves rows[] untouched on any
// failure, so a blip doesn't wipe the numbers we already have.
bool fetchArrivals() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("fetch: skipped, no WiFi");
    return false;
  }

  HTTPClient http;
  http.setTimeout(HTTP_TIMEOUT_MS);
  http.setConnectTimeout(HTTP_TIMEOUT_MS);
  if (!http.begin(ARRIVALS_URL)) {
    Serial.println("fetch: begin() failed, bad URL?");
    return false;
  }

  uint32_t started = millis();
  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    Serial.printf("fetch: HTTP %d (%s)\n", code, http.errorToString(code).c_str());
    http.end();
    return false;
  }

  // Parse straight off the socket so the body never lands in a String.
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, http.getStream());
  http.end();
  if (err) {
    Serial.printf("fetch: bad JSON, %s\n", err.c_str());
    return false;
  }

  JsonArray json_rows = doc["rows"].as<JsonArray>();
  if (json_rows.isNull()) {
    Serial.println("fetch: no \"rows\" array in response");
    return false;
  }

  uint8_t parsed = 0;
  for (JsonObject json_row : json_rows) {
    if (parsed >= MAX_ROWS) break;
    Row &row = rows[parsed];
    strlcpy(row.line,  json_row["line"]  | "?", sizeof(row.line));
    strlcpy(row.label, json_row["label"] | "",  sizeof(row.label));
    row.n_mins = 0;
    for (JsonVariant m : json_row["mins"].as<JsonArray>()) {
      if (row.n_mins >= MAX_MINS) break;
      row.mins[row.n_mins++] = m.as<int>();
    }
    parsed++;
  }
  n_rows  = parsed;
  updated = doc["updated"] | 0;
  strlcpy(updated_text, doc["updated_text"] | "", sizeof(updated_text));

  Serial.printf("fetch: %u row(s) in %lu ms, updated %lu\n",
                n_rows, millis() - started, (unsigned long)updated);
  for (uint8_t i = 0; i < n_rows; i++) {
    Serial.printf("  %-2s %-14s", rows[i].line, rows[i].label);
    for (uint8_t j = 0; j < rows[i].n_mins; j++) {
      Serial.printf("%s%d", j ? ", " : "", rows[i].mins[j]);
    }
    Serial.println(rows[i].n_mins ? " min" : "(none)");
  }
  return true;
}

// Adafruit GFX custom fonts draw from the baseline, so centering means
// subtracting both x1 and y1 from getTextBounds.
void drawCentered(const char *s, int16_t cx, int16_t cy) {
  int16_t x1, y1; uint16_t w, h;
  canvas.getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
  canvas.setCursor(cx - w / 2 - x1, cy - h / 2 - y1);
  canvas.print(s);
}

// Vertically centered on cy, with the text's left or right edge at x.
void drawAligned(const char *s, int16_t x, int16_t cy, bool right_edge) {
  int16_t x1, y1; uint16_t w, h;
  canvas.getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
  canvas.setCursor((right_edge ? x - w : x) - x1, cy - h / 2 - y1);
  canvas.print(s);
}

void drawRow(const Row &row, int16_t top, int16_t height) {
  int16_t cy = top + height / 2;
  int16_t r  = min(34, height / 2 - 6);

  // Line bullet: filled circle with the letter knocked out in white.
  canvas.fillCircle(46, cy, r, BLACK);
  canvas.setFont(&FreeSansBold24pt7b);
  canvas.setTextColor(WHITE);
  drawCentered(row.line, 46, cy);

  canvas.setTextColor(BLACK);

  // Destination label.
  canvas.setFont(&FreeSansBold18pt7b);
  drawAligned(row.label, 96, cy, false);

  // Next train: one big number, right-aligned.
  char head[8] = "--";
  if (row.n_mins > 0) snprintf(head, sizeof(head), "%d", row.mins[0]);
  canvas.setFont(&FreeSansBold24pt7b);
  drawAligned(head, 388, row.n_mins > 1 ? cy - 14 : cy, true);

  // Trains after that go on their own line underneath, so the big number
  // never reads as part of the list.
  if (row.n_mins > 1) {
    char tail[24];
    snprintf(tail, sizeof(tail), "then %d", row.mins[1]);
    for (uint8_t i = 2; i < row.n_mins; i++) {
      char part[8];
      snprintf(part, sizeof(part), ", %d", row.mins[i]);
      strlcat(tail, part, sizeof(tail));
    }
    canvas.setFont(&FreeSansBold12pt7b);
    drawAligned(tail, 388, cy + 30, true);
  }
}

// Status strip across the top. Labelled "Updated" so it never reads as the
// current time -- it is when the numbers below were last fetched.
void drawHeader() {
  char stamp[48];
  if (updated_text[0]) {
    snprintf(stamp, sizeof(stamp), "Updated %s", updated_text);
  } else {
    strlcpy(stamp, "Waiting for first update", sizeof(stamp));
  }
  canvas.setFont(&FreeSansBold12pt7b);
  drawCentered(stamp, 200, HEADER_H / 2 - 2);
  canvas.fillRect(0, HEADER_H - 2, 400, 2, BLACK);
}

void draw() {
  canvas.fillScreen(WHITE);
  canvas.setTextColor(BLACK);
  drawHeader();

  int16_t body_top = HEADER_H;
  int16_t body_h   = 300 - HEADER_H;

  if (n_rows == 0) {
    canvas.setFont(&FreeSansBold18pt7b);
    drawCentered(WiFi.status() == WL_CONNECTED ? "waiting for data" : "no wifi",
                 200, body_top + body_h / 2);
    return;
  }

  int16_t height = body_h / n_rows;
  for (uint8_t i = 0; i < n_rows; i++) {
    int16_t top = body_top + i * height;
    drawRow(rows[i], top, height);
    if (i > 0) canvas.fillRect(16, top - 1, 368, 3, BLACK);   // divider
  }
}

// A partial refresh diffs the new image against the "old image" plane in the
// controller's RAM. Init() hardware-resets the controller and Sleep() is deep
// sleep -- both lose that RAM, so neither may happen between partials, or each
// update composites onto the leftovers of the last one.
//
// So: re-init and re-establish the base only on a full refresh, and leave the
// controller awake in between. It idles at microamps and we are on USB power.
void refreshDisplay(bool full) {
  draw();
  if (full) {
    epd.Init();                             // reset, then write both RAM planes
    epd.Display_Base(canvas.getBuffer());   // ~2 s, flashes, becomes the base
  } else {
    epd.Display_Partial(canvas.getBuffer(), 0, 0, 400, 300);
  }
  Serial.printf("display: %s refresh, %u row(s)\n", full ? "full" : "partial", n_rows);
}

// Cheap fingerprint of what's on screen, so we only redraw on a real change.
uint32_t rowsFingerprint() {
  uint32_t h = 2166136261u;
  for (uint8_t i = 0; i < n_rows; i++) {
    for (const char *p = rows[i].line;  *p; p++) h = (h ^ (uint8_t)*p) * 16777619u;
    for (const char *p = rows[i].label; *p; p++) h = (h ^ (uint8_t)*p) * 16777619u;
    for (uint8_t j = 0; j < rows[i].n_mins; j++) h = (h ^ (uint32_t)rows[i].mins[j]) * 16777619u;
  }
  return h ^ n_rows;
}

void setup() {
  Serial.begin(115200);
  delay(500);                    // let the USB serial monitor attach
  Serial.println("\nsubway display booting");

  connectWiFi(WIFI_TIMEOUT_MS);  // non-fatal: still show the panel if it fails
  fetchArrivals();

  refreshDisplay(true);          // full refresh on boot
  shown_fp  = rowsFingerprint();
  last_full = millis();
  last_draw = millis();
}

void loop() {
  static uint32_t last_check = 0;
  static uint32_t last_poll  = 0;

  if (millis() - last_check >= WIFI_RECHECK_MS) {
    last_check = millis();
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi: link lost, reconnecting");
      connectWiFi(WIFI_TIMEOUT_MS);
    }
  }

  if (millis() - last_poll >= POLL_INTERVAL_MS) {
    last_poll = millis();
    if (fetchArrivals()) {
      uint32_t now_fp    = rowsFingerprint();
      bool     do_full   = millis() - last_full >= FULL_REFRESH_MS;
      bool     stale_hdr = millis() - last_draw >= STAMP_MAX_AGE_MS;
      if (now_fp != shown_fp || do_full || stale_hdr) {
        refreshDisplay(do_full);
        shown_fp  = now_fp;
        last_draw = millis();
        if (do_full) last_full = millis();
      }
    }
  }

  delay(100);
}
