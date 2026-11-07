// Spotify "Now Playing" display for ESP32 + 0.96" SSD1306 OLED (128x64, I2C)
// Shows: song name (scrolls if long), artist, play/pause icon, progress bar, time.
//
// Libraries needed (Arduino IDE -> Library Manager):
//   - Adafruit SSD1306 (+ Adafruit GFX, BusIO)   [already installed]
//   - ArduinoJson by Benoit Blanchon (version 7)
//
// Board: ESP32 Dev Module. Upload speed: 115200 if uploads fail.

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <base64.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ================== FILL THESE IN ==================
const char* WIFI_SSID     = "";      // must be 2.4 GHz WiFi (or phone hotspot)
const char* WIFI_PASSWORD = "";
const char* CLIENT_ID     = "";
const char* CLIENT_SECRET = "";
const char* REFRESH_TOKEN = "";
// ===================================================

Adafruit_SSD1306 display(128, 64, &Wire, -1);

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

// ---------- helpers ----------

// The OLED font only has basic English characters, so drop anything else.
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
  int m = totalSec / 60;
  int s = totalSec % 60;
  char buf[8];
  snprintf(buf, sizeof(buf), "%d:%02d", m, s);
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

// Finds the OLED on G21/G22 or G32/G33 (whichever your wires use).
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

// ---------- Spotify ----------

bool refreshAccessToken() {
  WiFiClientSecure client;
  client.setInsecure();  // skip certificate check (fine for a hobby project)
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

void fetchNowPlaying() {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient https;
  https.begin(client, "https://api.spotify.com/v1/me/player/currently-playing");
  https.addHeader("Authorization", "Bearer " + accessToken);

  int code = https.GET();
  if (code == 200) {
    // Only keep the fields we need (saves memory)
    JsonDocument filter;
    filter["is_playing"] = true;
    filter["progress_ms"] = true;
    filter["item"]["name"] = true;
    filter["item"]["duration_ms"] = true;
    filter["item"]["artists"][0]["name"] = true;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, https.getString(), DeserializationOption::Filter(filter));
    if (!err && !doc["item"].isNull()) {
      songName   = cleanText(doc["item"]["name"].as<String>());
      artistName = cleanText(doc["item"]["artists"][0]["name"].as<String>());
      durationMs = doc["item"]["duration_ms"] | 0;
      progressMs = doc["progress_ms"] | 0;
      isPlaying  = doc["is_playing"] | false;
      hasTrack = true;
    } else {
      hasTrack = false;  // e.g. an ad or podcast with no track info
    }
  } else if (code == 204) {
    hasTrack = false;    // nothing is playing
  } else if (code == 401) {
    tokenExpiresAt = 0;  // token expired: get a new one next loop
  } else {
    Serial.printf("Player error %d\n", code);
  }
  https.end();
  lastFetchMillis = millis();
  fetchedOnce = true;
}

// ---------- drawing ----------

void drawScreen() {
  display.clearDisplay();
  display.setTextWrap(false);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.print("Spotify");

  if (!hasTrack) {
    display.drawFastHLine(0, 11, 128, SSD1306_WHITE);
    display.setCursor(0, 26);
    display.print(fetchedOnce ? "Nothing playing" : "Loading...");
    display.display();
    return;
  }

  // play / pause icon (top right)
  if (isPlaying) {
    display.fillTriangle(118, 0, 118, 8, 126, 4, SSD1306_WHITE);
  } else {
    display.fillRect(118, 0, 3, 9, SSD1306_WHITE);
    display.fillRect(123, 0, 3, 9, SSD1306_WHITE);
  }
  display.drawFastHLine(0, 11, 128, SSD1306_WHITE);

  // song name: scrolls if it doesn't fit (each character is 6 px wide)
  int songW = songName.length() * 6;
  if (songW <= 128) {
    display.setCursor(0, 17);
    display.print(songName);
  } else {
    int gap = 30;
    int offset = (millis() / 40) % (songW + gap);
    display.setCursor(-offset, 17);
    display.print(songName);
    display.setCursor(-offset + songW + gap, 17);
    display.print(songName);
  }

  // artist (cut off if too long)
  display.setCursor(0, 29);
  if (artistName.length() > 21) display.print(artistName.substring(0, 20) + ".");
  else display.print(artistName);

  // progress: keep counting between Spotify checks so the bar moves smoothly
  long shown = progressMs;
  if (isPlaying) shown += (long)(millis() - lastFetchMillis);
  if (durationMs > 0 && shown > durationMs) shown = durationMs;

  display.drawRect(0, 42, 128, 6, SSD1306_WHITE);
  if (durationMs > 0) {
    int fill = (int)((shown * 124L) / durationMs);
    display.fillRect(2, 44, fill, 2, SSD1306_WHITE);
  }

  String left = formatTime(shown);
  String right = formatTime(durationMs);
  display.setCursor(0, 54);
  display.print(left);
  display.setCursor(128 - right.length() * 6, 54);
  display.print(right);

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

  if (!fetchedOnce || millis() - lastFetchMillis > 3000) {
    fetchNowPlaying();
  }

  drawScreen();
  delay(30);
}
