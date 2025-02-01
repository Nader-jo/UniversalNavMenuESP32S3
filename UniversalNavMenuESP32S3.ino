#include <WiFiManager.h>
#include <TFT_eSPI.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <ESP32Time.h>
#include "credentials.h"
#include "midleFont.h"
#include "bigFont.h"
#include "tinyFont.h"
// https://api.gemini.com/v2/ticker/btcusd
// https://api.gemini.com/v2/ticker/ethusd
// https://api.gemini.com/v2/ticker/solusd

// https://api.openweathermap.org/data/2.5/weather?lat={lat}&lon={lon}&appid={API key} // get weather

static const int screenW = 320;
static const int screenH = 170;

#define PIN_BACKLIGHT 38
#define PIN_POWER 15
#define PIN_SELECT 0
#define PIN_NEXT 14

uint8_t screenCount = 0;
uint8_t currentScreenId = 0;
uint8_t nextScreenId = 0;
bool inSubMenu = false;
uint8_t subMenuIndex = 0;
int timeZone = 0;
const char *city = "";

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite spriteCurrent = TFT_eSprite(&tft);
TFT_eSprite spriteNext = TFT_eSprite(&tft);
ESP32Time rtc(0);
StaticJsonDocument<2048> menuItems;

const char *server = "https://raw.githubusercontent.com/Nader-jo/UniversalNavMenuESP32S3/refs/heads/develop/test-menu.json";
const char *ntpServer = "pool.ntp.org";
const char *locationServer = "http://ip-api.com/json?fields=status,city,offset,query";

void getData();
void buildScreen(TFT_eSprite &spr, uint8_t screenId);
void buildSubScreen(TFT_eSprite &spr, uint8_t screenId, uint8_t subIndex);
void transitionScreen(TFT_eSprite &oldSpr, TFT_eSprite &newSpr, uint8_t direction);
void drawSpriteFromJson(TFT_eSprite &spr, JsonObject doc);
uint16_t parseColor(const char *colorStr);
void drawWifiScreen(TFT_eSprite &spr);
void drawWiFiSignal(TFT_eSprite &spr, float x, float y, float r);

void setup()
{
  Serial.begin(115200);
  pinMode(PIN_POWER, OUTPUT);
  digitalWrite(PIN_POWER, HIGH);
  pinMode(PIN_SELECT, INPUT_PULLUP);
  pinMode(PIN_NEXT, INPUT_PULLUP);
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  ledcSetup(0, 10000, 8);
  ledcAttachPin(PIN_BACKLIGHT, 0);
  ledcWrite(0, 160);

  spriteCurrent.deleteSprite();
  spriteCurrent.createSprite(screenW, screenH);
  drawWifiScreen(spriteCurrent);
  spriteCurrent.loadFont(midleFont);
  spriteCurrent.setTextDatum(4);
  spriteCurrent.drawString("Connecting", screenW / 2, 155);
  spriteCurrent.pushSprite(0, 0);

  WiFiManager wifiManager;
  wifiManager.setConfigPortalTimeout(5000);
  if (!wifiManager.autoConnect(WifiName, WifiPassword))
  {
    spriteCurrent.drawRect(0, 150, screenW, 20, TFT_BLACK);
    spriteCurrent.drawString("Failed", screenW / 2, 155);
    spriteCurrent.pushSprite(0, 0);
    delay(3000);
    ESP.restart();
  }

  StaticJsonDocument<1024> locData;
  deserializeJson(locData, getData(locationServer));
  timeZone = locData["offset"] | 0;
  city = locData["city"];
  setTime();
  deserializeJson(menuItems, getData(server));
  screenCount = menuItems["menu"].size();
  spriteCurrent.deleteSprite();
  spriteCurrent.createSprite(screenW, screenH);
  buildScreen(spriteCurrent, currentScreenId);
  spriteCurrent.pushSprite(0, 0);
}

void loop()
{
  if (!inSubMenu && (digitalRead(PIN_NEXT) == LOW))
  {
    nextScreenId = (currentScreenId + 1) % screenCount;
    spriteNext.deleteSprite();
    spriteNext.createSprite(screenW, screenH);
    buildScreen(spriteNext, nextScreenId);
    transitionScreen(spriteCurrent, spriteNext, 0);
    spriteCurrent.deleteSprite();
    spriteCurrent.createSprite(screenW, screenH);
    buildScreen(spriteCurrent, nextScreenId);
    currentScreenId = nextScreenId;
  }

  if (digitalRead(PIN_SELECT) == LOW)
  {
    if (!inSubMenu)
    {
      JsonObject screenObj = menuItems["menu"][currentScreenId];
      JsonArray subMenuArr = screenObj["subMenu"].as<JsonArray>();
      if (!subMenuArr.isNull() && subMenuArr.size() > 0)
      {
        subMenuIndex = 0;
        spriteNext.deleteSprite();
        spriteNext.createSprite(screenW, screenH);
        buildSubScreen(spriteNext, currentScreenId, subMenuIndex);
        transitionScreen(spriteCurrent, spriteNext, 1);
        spriteCurrent.deleteSprite();
        spriteCurrent.createSprite(screenW, screenH);
        buildSubScreen(spriteCurrent, currentScreenId, subMenuIndex);
        inSubMenu = true;
      }
    }
    else
    {
      spriteNext.deleteSprite();
      spriteNext.createSprite(screenW, screenH);
      buildScreen(spriteNext, currentScreenId);
      transitionScreen(spriteCurrent, spriteNext, 3);
      spriteCurrent.deleteSprite();
      spriteCurrent.createSprite(screenW, screenH);
      buildScreen(spriteCurrent, currentScreenId);
      inSubMenu = false;
    }
  }
  if (inSubMenu)
  {
    spriteCurrent.deleteSprite();
    spriteCurrent.createSprite(screenW, screenH);
    buildSubScreen(spriteCurrent, currentScreenId, subMenuIndex);
    spriteCurrent.pushSprite(0, 0);
  }
}

void setTime()
{
  configTime(timeZone, 0, ntpServer);
  struct tm timeinfo;
  if (getLocalTime(&timeinfo))
  {
    rtc.setTimeStruct(timeinfo);
  }
}

String getData(String url)
{
  HTTPClient http;
  String payload = "";
  Serial.println(url);
  http.begin(url);
  int httpResponseCode = http.GET();
  if (httpResponseCode > 0)
  {
    payload = http.getString();
  }
  else
  {
    Serial.print("GET request failed: ");
    Serial.println(httpResponseCode);
  }
  http.end();
  Serial.println(payload);
  return payload;
}

void buildScreen(TFT_eSprite &spr, uint8_t screenId)
{
  JsonObject screenObj = menuItems["menu"][screenId];
  drawSpriteFromJson(spr, screenObj);
}

void buildSubScreen(TFT_eSprite &spr, uint8_t screenId, uint8_t subIndex)
{
  JsonObject screenObj = menuItems["menu"][screenId];
  JsonArray subMenuArr = screenObj["subMenu"].as<JsonArray>();
  if (subMenuArr.isNull() || subIndex >= subMenuArr.size())
  {
    spr.fillSprite(TFT_BLACK);
    return;
  }
  JsonObject subObj = subMenuArr[subIndex];
  drawSpriteFromJson(spr, subObj);
}

void transitionScreen(TFT_eSprite &oldSpr, TFT_eSprite &newSpr, uint8_t direction)
{
  const int steps = 10;
  for (int i = 0; i <= steps; i++)
  {
    int oldX = 0, oldY = 0;
    int newX = 0, newY = 0;
    switch (direction)
    {
    case 0:
      oldX = -(i * (screenW / steps));
      newX = screenW - (i * (screenW / steps));
      break;
    case 1:
      oldY = -(i * (screenH / steps));
      newY = screenH - (i * (screenH / steps));
      break;
    case 2:
      oldX = i * (screenW / steps);
      newX = -screenW + (i * (screenW / steps));
      break;
    case 3:
      oldY = i * (screenH / steps);
      newY = -screenH + (i * (screenH / steps));
      break;
    default:
      oldX = -(i * (screenW / steps));
      newX = screenW - (i * (screenW / steps));
      break;
    }
    oldSpr.pushSprite(oldX, oldY);
    newSpr.pushSprite(newX, newY);
    delay(15);
  }
}

void drawSpriteFromJson(TFT_eSprite &spr, JsonObject doc)
{
  if (doc.isNull())
  {
    spr.fillSprite(TFT_BLACK);
    return;
  }
  const char *bgColorStr = doc["backgroundColor"] | "0x0000";
  uint16_t bgColor = parseColor(bgColorStr);
  spr.fillSprite(bgColor);
  JsonArray elements = doc["elements"].as<JsonArray>();
  if (!elements.isNull())
  {
    for (JsonObject elem : elements)
    {
      const char *type = elem["type"] | "";
      if (strcmp(type, "fillSmoothRoundRect") == 0)
      {
        int x = elem["x"] | 0;
        int y = elem["y"] | 0;
        int w = elem["w"] | 0;
        int h = elem["h"] | 0;
        int r = elem["r"] | 0;
        const char *fgStr = elem["color"] | "0xFFFF";
        const char *bgStr = elem["bgColor"] | "0x0000";
        uint16_t fgColor = parseColor(fgStr);
        uint16_t bgRect = parseColor(bgStr);
        spr.fillSmoothRoundRect(x, y, w, h, r, fgColor, bgRect);
      }
      else if (strcmp(type, "text") == 0)
      {
        int x = elem["x"] | 0;
        int y = elem["y"] | 0;
        int size = elem["size"] | 1;
        int datum = elem["datum"] | 0;
        const char *txtColorStr = elem["color"] | "0xFFFF";
        const char *txtBgStr = elem["bgColor"] | "0x0000";
        const char *content = elem["content"] | "";
        uint16_t txtColor = parseColor(txtColorStr);
        uint16_t txtBg = parseColor(txtBgStr);
        if (size == 1)
          spr.loadFont(tinyFont);
        else if (size == 2)
          spr.loadFont(midleFont);
        else if (size == 3)
          spr.loadFont(bigFont);
        else
          spr.loadFont(midleFont);
        spr.setTextDatum(datum);
        spr.setTextColor(txtColor, txtBg);
        spr.drawString(content, x, y);
        spr.unloadFont();
      }
      else if (strcmp(type, "textDynamic") == 0)
      {
        const char *url = elem["url"] | "";
        const char *jsonPath = elem["jsonPath"] | "";
        const char *format = elem["format"] | "$v";
        unsigned int substringS = elem["substringS"] | 0;
        unsigned int substringE = elem["substringE"] | 0;
        int x = elem["x"] | 0;
        int y = elem["y"] | 0;
        int size = elem["size"] | 1;
        int datum = elem["datum"] | 0;
        uint16_t txtColor = parseColor(elem["color"] | "0xFFFF");
        uint16_t bgColor = parseColor(elem["bgColor"] | "0x0000");
        String finalText = "";
        if (String(url) != "getTime")
        {
          StaticJsonDocument<1024> fetchedData;
          deserializeJson(fetchedData, getData(url));

          String dynamicValue = getValueByPath(fetchedData, jsonPath);

          // If format="Temp: $v C" and dynamicValue="23.4", result="Temp: 23.4 C"
          finalText = format;
          finalText.replace("$v", dynamicValue);
        }
        else
        {
          finalText = rtc.getTime();
        }

        if (substringE != 0 || substringS != 0)
        {
          finalText = finalText.substring(substringS, substringE);
        }

        if (size == 1)
          spr.loadFont(tinyFont);
        else if (size == 2)
          spr.loadFont(midleFont);
        else if (size == 3)
          spr.loadFont(bigFont);
        else
          spr.loadFont(midleFont);
        spr.setTextDatum(datum);
        spr.setTextColor(txtColor, bgColor);
        spr.drawString(finalText, x, y);
        spr.unloadFont();
      }
      else if (strcmp(type, "fillTriangle") == 0)
      {
        int x0 = elem["x0"] | 0;
        int y0 = elem["y0"] | 0;
        int x1 = elem["x1"] | 0;
        int y1 = elem["y1"] | 0;
        int x2 = elem["x2"] | 0;
        int y2 = elem["y2"] | 0;
        const char *colorStr = elem["color"] | "0xFFFF";
        uint16_t colorVal = parseColor(colorStr);
        spr.fillTriangle(x0, y0, x1, y1, x2, y2, colorVal);
      }
      else if (strcmp(type, "fillRect") == 0)
      {
        int x = elem["x"] | 0;
        int y = elem["y"] | 0;
        int w = elem["w"] | 0;
        int h = elem["h"] | 0;
        const char *colorStr = elem["color"] | "0xFFFF";
        uint16_t colorVal = parseColor(colorStr);
        spr.fillRect(x, y, w, h, colorVal);
      }
      else if (strcmp(type, "drawRect") == 0)
      {
        int x = elem["x"] | 0;
        int y = elem["y"] | 0;
        int w = elem["w"] | 0;
        int h = elem["h"] | 0;
        const char *colorStr = elem["color"] | "0xFFFF";
        uint16_t colorVal = parseColor(colorStr);
        spr.drawRect(x, y, w, h, colorVal);
      }
      else if (strcmp(type, "fillCircle") == 0)
      {
        int x = elem["x"] | 0;
        int y = elem["y"] | 0;
        int r = elem["r"] | 0;
        const char *colorStr = elem["color"] | "0xFFFF";
        uint16_t colorVal = parseColor(colorStr);
        spr.fillCircle(x, y, r, colorVal);
      }
      else if (strcmp(type, "drawCircle") == 0)
      {
        int x = elem["x"] | 0;
        int y = elem["y"] | 0;
        int r = elem["r"] | 0;
        const char *colorStr = elem["color"] | "0xFFFF";
        uint16_t colorVal = parseColor(colorStr);
        spr.drawCircle(x, y, r, colorVal);
      }
      else if (strcmp(type, "drawLine") == 0)
      {
        int x0 = elem["x0"] | 0;
        int y0 = elem["y0"] | 0;
        int x1 = elem["x1"] | 0;
        int y1 = elem["y1"] | 0;
        const char *colorStr = elem["color"] | "0xFFFF";
        uint16_t colorVal = parseColor(colorStr);
        spr.drawLine(x0, y0, x1, y1, colorVal);
      }
      else if (strcmp(type, "fillSmoothCircle") == 0)
      {
        int x = elem["x"] | 0;
        int y = elem["y"] | 0;
        int r = elem["r"] | 0;
        const char *fgStr = elem["color"] | "0xFFFF";
        const char *bgStr = elem["bgColor"] | "0x0000";
        uint16_t fgColor = parseColor(fgStr);
        uint16_t bgColor = parseColor(bgStr);
        spr.fillSmoothCircle(x, y, r, fgColor, bgColor);
      }
      else if (strcmp(type, "drawSmoothArc") == 0)
      {
        int x = elem["x"] | 0;
        int y = elem["y"] | 0;
        int rOuter = elem["rOuter"] | 0;
        int rInner = elem["rInner"] | 0;
        int start = elem["start"] | 0;
        int end = elem["end"] | 0;
        const char *fgStr = elem["color"] | "0xFFFF";
        const char *bgStr = elem["bgColor"] | "0x0000";
        uint16_t fgColor = parseColor(fgStr);
        uint16_t bgColor = parseColor(bgStr);
        spr.drawSmoothArc(x, y, rOuter, rInner, start, end, fgColor, bgColor);
      }
    }
  }
}

uint16_t parseColor(const char *colorStr)
{
  return (uint16_t)strtol(colorStr, nullptr, 16);
}

void drawWifiScreen(TFT_eSprite &spr)
{
  spr.fillSprite(TFT_BLACK);
  drawWiFiSignal(spr, screenW / 2, screenH / 2 - 10, 80);
}

void drawWiFiSignal(TFT_eSprite &spr, float x, float y, float r)
{
  float gap = r / 3.2f;
  spr.fillSmoothCircle(x, y + r / 2, gap / 3, TFT_CYAN, TFT_BLACK);
  for (int i = 0; i < 3; i++)
  {
    float currentRadius = r - i * gap;
    spr.drawSmoothArc(x, y + r / 2, currentRadius, currentRadius - 10, 135, 225, TFT_CYAN, TFT_BLACK);
  }
}

String getValueByPath(JsonDocument &doc, const char *path)
{
  // If path is empty, just return an empty string
  if (!path || !strlen(path))
  {
    return String("");
  }

  // We'll copy 'path' into a temporary buffer because strtok() modifies the string.
  // Alternatively, you can do a manual parsing without strtok if you prefer.
  const size_t pathLen = strlen(path);
  char *tempPath = (char *)malloc(pathLen + 1);
  if (!tempPath)
  {
    // Memory allocation failed
    return String("");
  }
  strcpy(tempPath, path);

  // Use a pointer to traverse the JSON hierarchy
  JsonVariant currentVar = doc.as<JsonVariant>();

  // Split on '.'
  char *token = strtok(tempPath, ".");
  while (token != nullptr)
  {
    if (!currentVar.is<JsonObject>())
    {
      // Current element is not an object, so we can't go deeper
      free(tempPath);
      return String("");
    }

    // Descend into the JSON object with the current token
    currentVar = currentVar[token];
    if (currentVar.isNull())
    {
      // Key doesn't exist
      free(tempPath);
      return String("");
    }

    // Move on to next part
    token = strtok(nullptr, ".");
  }

  // Now currentVar should be the final element
  // Convert to string. If it's not a string, we'll try to convert anyway.
  String result;
  if (currentVar.is<const char *>())
  {
    // It's already a string
    result = currentVar.as<const char *>();
  }
  else
  {
    // Convert numeric or bool to string
    result = currentVar.as<String>();
  }

  free(tempPath);
  return result;
}
