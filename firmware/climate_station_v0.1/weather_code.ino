#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// --- Wi-Fi Network Credentials ---
const char* WIFI_SSID = "name_wifi";
const char* WIFI_PASSWORD = "password";

// --- Backend Configuration ---
const char* FUNCTION_URL = "url_fuction";
const char* API_KEY = "security_key";

// --- OLED Display Configuration ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define OLED_ADDRESS  0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// --- DHT Sensors Pin Definition (ESP32 Layout) ---
#define DHTPIN_1  4     // Left sensor (D4)
#define DHTPIN_2  5     // Right sensor (D5)
#define DHTTYPE   DHT11

DHT dht1(DHTPIN_1, DHTTYPE);
DHT dht2(DHTPIN_2, DHTTYPE);

// --- LDR Sensors (ADC1, Wi-Fi safe) ---
const int LDR_PIN_1 = 34; // Left LDR (D34)
const int LDR_PIN_2 = 35; // Right LDR (D35)

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Initialize I2C for the OLED display
  Wire.begin(21, 22);

  // Initialize sensors
  dht1.begin();
  dht2.begin();

  // Initialize OLED
  if(!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println(F("OLED initialization failed"));
    for(;;);
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(F("CONNECTING TO WIFI..."));
  display.display();

  // Wi-Fi connection
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("\n[OK] WiFi connected");
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println(F("WIFI CONNECTED OK"));
  display.display();
  delay(2000);
}

void sendReading(float t1, float h1, float t2, float h2, int l1, int l2) {
  if (WiFi.status() != WL_CONNECTED) return;

  float t_avg = (t1 + t2) / 2.0;
  float h_avg = (h1 + h2) / 2.0;
  int l_avg = (l1 + l2) / 2;

  WiFiClientSecure client;
  client.setInsecure(); // Skips strict SSL certificate validation

  HTTPClient https;
  if (!https.begin(client, FUNCTION_URL)) {
    Serial.println("Could not start HTTPS connection");
    return;
  }

  https.addHeader("Content-Type", "application/json");
  https.addHeader("x-api-key", API_KEY);

  StaticJsonDocument<256> doc;
  doc["t1"] = t1;
  doc["h1"] = h1;
  doc["t2"] = t2;
  doc["h2"] = h2;
  doc["t_avg"] = t_avg;
  doc["h_avg"] = h_avg;
  doc["l1"] = l1;
  doc["l2"] = l2;
  doc["l_avg"] = l_avg;

  String body;
  serializeJson(doc, body);

  int httpCode = https.POST(body);

  if (httpCode > 0) {
    Serial.printf("HTTP %d: %s\n", httpCode, https.getString().c_str());
  } else {
    Serial.printf("POST error: %s\n", https.errorToString(httpCode).c_str());
  }

  https.end();
}

void loop() {
  // 1. Read the real DHT sensors
  float t1 = dht1.readTemperature();
  float h1 = dht1.readHumidity();
  float t2 = dht2.readTemperature();
  float h2 = dht2.readHumidity();

  if (isnan(t1) || isnan(h1) || isnan(t2) || isnan(h2)) {
    Serial.println(F("Error reading DHT sensors!"));
    delay(2000);
    return;
  }

  // 2. Read the real LDR sensors
  int ldr1 = analogRead(LDR_PIN_1);
  int ldr2 = analogRead(LDR_PIN_2);

  // Compute averages for the display
  float tempAvg = (t1 + t2) / 2.0;
  float humAvg = (h1 + h2) / 2.0;
  int lightAvg = (ldr1 + ldr2) / 2;

  // 3. Send data to the cloud via the HTTPS function
  sendReading(t1, h1, t2, h2, ldr1, ldr2);

  // 4. Show summary on the local OLED display
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println(F("CLOUD SYNC OK"));

  display.setCursor(0, 16);
  display.print(F("T.Avg: ")); display.print(tempAvg, 1); display.println(F(" C"));
  
  display.setCursor(0, 28);
  display.print(F("H.Avg: ")); display.print(humAvg, 1); display.println(F(" %"));

  display.setCursor(0, 40);
  display.print(F("Light: ")); display.print(lightAvg); display.println(F(" pts"));
  
  display.display();

  // Wait 5 minutes before the next reading and transmission
  delay(5 * 60 * 1000); 
}