// Spotify "Now Playing" with ALBUM COVER + 3 BUTTONS for ESP32 + 0.96" SSD1306 OLED (128x64, I2C)
//
// Buttons (each: one leg to the GPIO, the other leg to GND - no resistors needed):
//   G25 = previous   G26 = play/pause   G27 = next
// Needs a refresh token made WITH the "user-modify-playback-state" scope.
//
// Layout:
//   left  64x64 : album cover (black & white, dithered)
//   right 62x64 : song (scrolls), artist, play/pause, progress bar, time
//
// Libraries (Arduino IDE -> Library Manager):
//   - Adafruit SSD1306 (+ Adafruit GFX, BusIO)   [installed]
//   - ArduinoJson by Benoit Blanchon (v7)        [installed]
//   - TJpg_Decoder by Bodmer                      [NEW - install this]
//
// Board: ESP32 Dev Module.

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <base64.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <TJpg_Decoder.h>

// ================== FILL THESE IN ==================
const char* WIFI_SSID     = "";      // must be 2.4 GHz WiFi (or phone hotspot)
const char* WIFI_PASSWORD = "";
const char* CLIENT_ID     = "";
const char* CLIENT_SECRET = "";
const char* REFRESH_TOKEN = "";
// ===================================================

Adafruit_SSD1306 display(128, 64, &Wire, -1);

// ---- Spotify state ----
String accessToken = "";
unsigned long tokenExpiresAt = 0;
bool hasTrack = false;
bool isPlaying = false;
String songName = "";
String artistName = "";
long progressMs = 0;
long durationMs = 0;
unsigned long lastFetchMillis = 0;
bool fetchedOnce = false;

// ---- Album art ----
const int ART = 64;                 // cover size on screen (64x64)
uint8_t gray[ART * ART];            // greyscale pixels from the JPEG
uint8_t artBitmap[ART * ART / 8];   // final 1-bit picture for the OLED
bool hasArt = false;
String currentArtUrl = "";
String pendingArtUrl = "";

// ---- Buttons ----
const int BTN_PREV = 25;
const int BTN_PLAY = 26;
const int BTN_NEXT = 27;
const int buttonPins[3] = {BTN_PREV, BTN_PLAY, BTN_NEXT};
bool lastButtonState[3] = {HIGH, HIGH, HIGH};
unsigned long lastButtonChange[3] = {0, 0, 0};
String flashText = "";              // short message like ">> Next"
unsigned long flashUntil = 0;

// Text area on the right
const int TEXT_X = 66;                  // x where text starts
const int TEXT_W = 128 - TEXT_X;            // width of text area (62 px = 10 characters)

// ---------- helpers ----------

String cleanText(const String& in) {
  String out = "";
  for (unsigned int i = 0; i < in.length(); i++) {
    char c = in[i];
    if (c >= 32 && c <= 126) out += c;
  }
  if (out.length() == 0) out = "?";
  return out;
}

String formatTime(long ms) {
  if (ms < 0) ms = 0;
  long totalSec = ms / 1000;
  char buf[8];
  snprintf(buf, sizeof(buf), "%ld:%02ld", totalSec / 60, totalSec % 60);
  return String(buf);
}

void showMessage(const char* line1, const char* line2 = "") {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Spotify");
  display.drawFastHLine(0, 11, 128, SSD1306_WHITE);
  display.setCursor(0, 20);
  display.println(line1);
  display.setCursor(0, 32);
  display.println(line2);
  display.display();
}

bool setupDisplay() {
  int pinSets[2][2] = {{21, 22}, {32, 33}};
  byte addrs[2] = {0x3C, 0x3D};
  for (int p = 0; p < 2; p++) {
    Wire.begin(pinSets[p][0], pinSets[p][1]);
    for (int a = 0; a < 2; a++) {
      Wire.beginTransmission(addrs[a]);
      if (Wire.endTransmission() == 0) {
        Serial.printf("OLED found at 0x%X on SDA=G%d SCK=G%d\n", addrs[a], pinSets[p][0], pinSets[p][1]);
        display.begin(SSD1306_SWITCHCAPVCC, addrs[a]);
        return true;
      }
    }
    Wire.end();
  }
  return false;
}

void connectWiFi() {
  showMessage("Connecting WiFi...", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 40) {
    delay(500);
    Serial.print(".");
    tries++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("\nWiFi connected, IP: ");
    Serial.println(WiFi.localIP());
    showMessage("WiFi connected!");
  } else {
    Serial.println("\nWiFi failed");
    showMessage("WiFi failed", "Check name/password");
  }
}

// ---------- album art: JPEG -> greyscale -> dithered 1-bit ----------

// Called by TJpg_Decoder for each decoded block of pixels (RGB565).
bool jpgToGray(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  for (int j = 0; j < h; j++) {
    int py = y + j;
    if (py < 0 || py >= ART) continue;
    for (int i = 0; i < w; i++) {
      int px = x + i;
      if (px < 0 || px >= ART) continue;
      uint16_t c = bitmap[j * w + i];
      int r = ((c >> 11) & 0x1F) * 255 / 31;
      int g = ((c >> 5) & 0x3F) * 255 / 63;
      int b = (c & 0x1F) * 255 / 31;
      gray[py * ART + px] = (r * 77 + g * 150 + b * 29) >> 8;
    }
  }
  return true;
}

// Stretch contrast, then Floyd-Steinberg dithering into artBitmap.
void ditherArt() {
  int lo = 255, hi = 0;
  for (int i = 0; i < ART * ART; i++) {
    if (gray[i] < lo) lo = gray[i];
    if (gray[i] > hi) hi = gray[i];
  }
  int range = hi - lo;
  if (range < 1) range = 1;

  static int16_t err[ART * ART];
  for (int i = 0; i < ART * ART; i++) err[i] = (int16_t)((gray[i] - lo) * 255 / range);

  memset(artBitmap, 0, sizeof(artBitmap));
  for (int y = 0; y < ART; y++) {
    for (int x = 0; x < ART; x++) {
      int i = y * ART + x;
      int oldV = err[i];
      int newV = oldV < 128 ? 0 : 255;
      if (newV) artBitmap[y * (ART / 8) + x / 8] |= (0x80 >> (x % 8));
      int e = oldV - newV;
      if (x + 1 < ART)               err[i + 1]       += e * 7 / 16;
      if (y + 1 < ART && x > 0)      err[i + ART - 1] += e * 3 / 16;
      if (y + 1 < ART)               err[i + ART]     += e * 5 / 16;
      if (y + 1 < ART && x + 1 < ART) err[i + ART + 1] += e * 1 / 16;
    }
  }
}

// Downloads the cover JPEG and converts it. Returns true on success.
bool loadAlbumArt(const String& url, int imgWidth) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient https;
  if (!https.begin(client, url)) return false;
  int code = https.GET();
  if (code != 200) {
    Serial.printf("Image error %d\n", code);
    https.end();
    return false;
  }

  int len = https.getSize();
  const int MAX_JPG = 60000;
  if (len <= 0 || len > MAX_JPG) {
    Serial.printf("Image size not usable: %d\n", len);
    https.end();
    return false;
  }

  uint8_t* buf = (uint8_t*)malloc(len);
  if (!buf) {
    https.end();
    return false;
  }

  WiFiClient* stream = https.getStreamPtr();
  int got = 0;
  unsigned long start = millis();
  while (got < len && millis() - start < 8000) {
    int avail = stream->available();
    if (avail > 0) {
      got += stream->readBytes(buf + got, min(avail, len - got));
    } else {
      delay(1);
    }
  }
  https.end();

  if (got != len) {
    Serial.println("Image download incomplete");
    free(buf);
    return false;
  }

  // Shrink big images so they fit in 64x64 (TJpg_Decoder can scale by 1, 2, 4 or 8)
  int scale = 1;
  while (imgWidth / scale > ART && scale < 8) scale *= 2;
  TJpgDec.setJpgScale(scale);

  memset(gray, 0, sizeof(gray));
  JRESULT res = TJpgDec.drawJpg(0, 0, buf, len);
  free(buf);
  if (res != JDR_OK) {
    Serial.printf("JPEG decode error %d\n", res);
    return false;
  }

  ditherArt();
  return true;
}

// ---------- Spotify ----------

bool refreshAccessToken() {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient https;
  if (!https.begin(client, "https://accounts.spotify.com/api/token")) return false;

  String auth = base64::encode(String(CLIENT_ID) + ":" + CLIENT_SECRET);
  https.addHeader("Authorization", "Basic " + auth);
  https.addHeader("Content-Type", "application/x-www-form-urlencoded");

  int code = https.POST(String("grant_type=refresh_token&refresh_token=") + REFRESH_TOKEN);
  if (code != 200) {
    Serial.printf("Token error %d: %s\n", code, https.getString().c_str());
    https.end();
    return false;
  }

  JsonDocument doc;
  deserializeJson(doc, https.getString());
  accessToken = doc["access_token"].as<String>();
  int expiresIn = doc["expires_in"] | 3600;
  tokenExpiresAt = millis() + (unsigned long)(expiresIn - 60) * 1000UL;
  https.end();
  Serial.println("Got new access token");
  return true;
}

int pendingArtWidth = 64;

void fetchNowPlaying() {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient https;
  https.begin(client, "https://api.spotify.com/v1/me/player/currently-playing");
  https.addHeader("Authorization", "Bearer " + accessToken);

  int code = https.GET();
  if (code == 200) {
    JsonDocument filter;
    filter["is_playing"] = true;
    filter["progress_ms"] = true;
    filter["item"]["name"] = true;
    filter["item"]["duration_ms"] = true;
    filter["item"]["artists"][0]["name"] = true;
    filter["item"]["album"]["images"][0]["url"] = true;
    filter["item"]["album"]["images"][0]["width"] = true;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, https.getString(), DeserializationOption::Filter(filter));
    if (!err && !doc["item"].isNull()) {
      songName   = cleanText(doc["item"]["name"].as<String>());
      artistName = cleanText(doc["item"]["artists"][0]["name"].as<String>());
      durationMs = doc["item"]["duration_ms"] | 0;
      progressMs = doc["progress_ms"] | 0;
      isPlaying  = doc["is_playing"] | false;
      hasTrack = true;

      // pick the smallest cover image (Spotify gives 640, 300 and 64 px)
      JsonArray images = doc["item"]["album"]["images"].as<JsonArray>();
      String bestUrl = "";
      int bestW = 100000;
      for (JsonObject img : images) {
        int w = img["width"] | 0;
        String u = img["url"].as<String>();
        if (w > 0 && w < bestW && u.length() > 0) {
          bestW = w;
          bestUrl = u;
        }
      }
      if (bestUrl.length() > 0 && bestUrl != currentArtUrl) {
        pendingArtUrl = bestUrl;
        pendingArtWidth = bestW;
      }
    } else {
      hasTrack = false;
    }
  } else if (code == 204) {
    hasTrack = false;
  } else if (code == 401) {
    tokenExpiresAt = 0;
  } else {
    Serial.printf("Player error %d\n", code);
  }
  https.end();
  lastFetchMillis = millis();
  fetchedOnce = true;
}

// ---------- buttons -> Spotify commands ----------

// method: "POST" or "PUT". Returns the HTTP status code.
int spotifyCommand(const char* method, const char* url) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient https;
  if (!https.begin(client, url)) return -1;
  https.addHeader("Authorization", "Bearer " + accessToken);
  https.addHeader("Content-Length", "0");
  int code = (strcmp(method, "PUT") == 0) ? https.PUT("") : https.POST("");
  if (code != 200 && code != 204) {
    Serial.printf("Command error %d: %s\n", code, https.getString().c_str());
  }
  https.end();
  return code;
}

void flash(const String& text) {
  flashText = text;
  flashUntil = millis() + 1200;
}

void onButton(int index) {
  int code = 0;
  if (index == 0) {
    flash("<< Prev");
    code = spotifyCommand("POST", "https://api.spotify.com/v1/me/player/previous");
  } else if (index == 1) {
    if (isPlaying) {
      flash("Pause");
      code = spotifyCommand("PUT", "https://api.spotify.com/v1/me/player/pause");
    } else {
      flash("Play");
      code = spotifyCommand("PUT", "https://api.spotify.com/v1/me/player/play");
    }
    if (code == 200 || code == 204) isPlaying = !isPlaying;
  } else {
    flash("Next >>");
    code = spotifyCommand("POST", "https://api.spotify.com/v1/me/player/next");
  }

  if (code == 401) tokenExpiresAt = 0;          // token expired
  if (code == 403) flash("Need new token");     // token made without control permission
  if (code == 404) flash("Open Spotify");       // no active device
  lastFetchMillis = millis() - 2500;            // refresh song info in ~0.5 s
}

void checkButtons() {
  for (int i = 0; i < 3; i++) {
    bool state = digitalRead(buttonPins[i]);
    if (state != lastButtonState[i] && millis() - lastButtonChange[i] > 50) {
      lastButtonChange[i] = millis();
      lastButtonState[i] = state;
      if (state == LOW) onButton(i);            // pressed (pin pulled to GND)
    }
  }
}

// ---------- drawing ----------

void drawPlaceholderArt() {
  display.fillRect(0, 0, ART, ART, SSD1306_BLACK);
  display.drawRect(0, 0, ART, ART, SSD1306_WHITE);
  // simple music note
  display.fillCircle(24, 42, 6, SSD1306_WHITE);
  display.fillRect(29, 16, 3, 27, SSD1306_WHITE);
  display.fillRect(29, 16, 14, 4, SSD1306_WHITE);
}

void drawScreen() {
  display.clearDisplay();
  display.setTextWrap(false);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  if (!hasTrack) {
    drawPlaceholderArt();
    display.setCursor(TEXT_X, 20);
    display.print(fetchedOnce ? "Nothing" : "Loading");
    display.setCursor(TEXT_X, 32);
    display.print(fetchedOnce ? "playing" : "...");
    display.display();
    return;
  }

  // --- song name (scrolls inside the right side) ---
  int songW = songName.length() * 6;
  if (songW <= TEXT_W) {
    display.setCursor(TEXT_X, 4);
    display.print(songName);
  } else {
    int gap = 24;
    int offset = (millis() / 45) % (songW + gap);
    display.setCursor(TEXT_X - offset, 4);
    display.print(songName);
    display.setCursor(TEXT_X - offset + songW + gap, 4);
    display.print(songName);
  }

  // --- artist (cut to 10 characters) ---
  display.setCursor(TEXT_X, 17);
  if (artistName.length() > 10) display.print(artistName.substring(0, 9) + ".");
  else display.print(artistName);

  // --- play / pause icon (or a short button message) ---
  if (millis() < flashUntil) {
    display.setCursor(TEXT_X, 30);
    display.print(flashText);
  } else if (isPlaying) {
    display.fillTriangle(TEXT_X, 29, TEXT_X, 37, TEXT_X + 7, 33, SSD1306_WHITE);
  } else {
    display.fillRect(TEXT_X, 29, 3, 9, SSD1306_WHITE);
    display.fillRect(TEXT_X + 5, 29, 3, 9, SSD1306_WHITE);
  }

  // --- progress bar ---
  long shown = progressMs;
  if (isPlaying) shown += (long)(millis() - lastFetchMillis);
  if (durationMs > 0 && shown > durationMs) shown = durationMs;

  display.drawRect(TEXT_X, 43, TEXT_W, 5, SSD1306_WHITE);
  if (durationMs > 0) {
    int fill = (int)((shown * (long)(TEXT_W - 2)) / durationMs);
    display.fillRect(TEXT_X + 1, 44, fill, 3, SSD1306_WHITE);
  }

  // --- time ---
  String left = formatTime(shown);
  String right = formatTime(durationMs);
  display.setCursor(TEXT_X, 54);
  display.print(left);
  display.setCursor(128 - right.length() * 6, 54);
  display.print(right);

  // --- album cover last, so scrolling text never covers it ---
  if (hasArt) {
    display.drawBitmap(0, 0, artBitmap, ART, ART, SSD1306_WHITE, SSD1306_BLACK);
  } else {
    drawPlaceholderArt();
  }

  display.display();
}

// ---------- main ----------

void setup() {
  Serial.begin(115200);
  delay(1000);

  if (!setupDisplay()) {
    Serial.println("OLED NOT FOUND - check wires");
    while (true) delay(1000);
  }

  for (int i = 0; i < 3; i++) pinMode(buttonPins[i], INPUT_PULLUP);

  TJpgDec.setSwapBytes(false);
  TJpgDec.setCallback(jpgToGray);

  showMessage("Starting...");
  connectWiFi();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
    delay(2000);
    return;
  }

  if (accessToken == "" || (long)(millis() - tokenExpiresAt) >= 0) {
    showMessage("Logging in to", "Spotify...");
    if (!refreshAccessToken()) {
      showMessage("Spotify login error", "See Serial Monitor");
      delay(10000);
      return;
    }
  }

  checkButtons();

  if (!fetchedOnce || millis() - lastFetchMillis > 3000) {
    fetchNowPlaying();
  }

  // new song -> download its cover (only once per album)
  if (pendingArtUrl.length() > 0) {
    String url = pendingArtUrl;
    pendingArtUrl = "";
    hasArt = loadAlbumArt(url, pendingArtWidth);
    currentArtUrl = url;
    if (!hasArt) Serial.println("Could not load cover, showing placeholder");
  }

  drawScreen();
  delay(30);
}
