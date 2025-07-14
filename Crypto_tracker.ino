#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <Wire.h>
#include <HTTPClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "secrets.h"  // WiFi Configuration (WiFi name and Password)
#include <ArduinoJson.h>
#define SCREEN_WIDTH 128     // OLED display width, in pixels
#define SCREEN_HEIGHT 64     // OLED display height, in pixels
#define OLED_RESET -1        // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C  ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

const char* ssid = SSID;
const char* password = WIFI_PASSWORD;

const int httpsPort = 443;
// Powered by CoinDesk -
const String url = "https://min-api.cryptocompare.com/data/price?fsym=BTC&tsyms=USD";
const String historyURL = "https://min-api.cryptocompare.com/data/v2/histoday?fsym=BTC&tsym=USD&limit=1";
const String cryptoCode = "BTC";

WiFiClient client;
HTTPClient http;

// Variables to save date and time
String formattedDate;
String dayStamp;
String timeStamp;

bool test = true;

void setup() {
  Serial.begin(115200);

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;  // Don't proceed, loop forever
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.println("Connecting to WiFi...");
  display.display();

  WiFi.begin(ssid, password);

  Serial.print("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  display.println("Connected to: ");
  display.print(ssid);
  display.display();
  delay(1500);
  display.clearDisplay();
  display.display();
}

void loop() {
  double currentPrice = 0.0;    // Initialize to 0.0 for safety
  double yesterdayPrice = 0.0; // Initialize to 0.0 for safety

  // --- 1. Fetch Current Price ---
  //Serial.print("Fetching current price from: ");
  //Serial.println(url);

  http.begin(url);
  int httpCode = http.GET();
  StaticJsonDocument<200> doc; // Smaller buffer is fine for this simple JSON
  DeserializationError error = deserializeJson(doc, http.getString());


  if (httpCode == HTTP_CODE_OK) { // Check HTTP status first
    if (error) {
      Serial.print(F("deserializeJson Failed (Current Price): "));
      Serial.println(error.f_str());
    } else {
      currentPrice = doc["USD"].as<double>(); // Direct conversion to double
      Serial.print("Current Price (USD): ");
      Serial.println(currentPrice, 2); // Print with 2 decimal places
    }
  } else {
    Serial.print("HTTP Request Failed (Current Price) with code: ");
    Serial.println(httpCode);
    Serial.println("Response: " + http.getString());
  }
  http.end(); // Close the HTTP connection for current price

  // --- 2. Fetch Historical Data ---
  Serial.print("Fetching historical data from: ");
  Serial.println(historyURL);

  // Use a larger buffer for historical data (array of objects)
  StaticJsonDocument<4000> historyDoc; // 4000 bytes should be sufficient for limit=10 (11 entries)
  http.begin(historyURL);
  int historyHttpCode = http.GET();
  DeserializationError historyError = deserializeJson(historyDoc, http.getString());

    if (historyHttpCode == HTTP_CODE_OK) { // Check HTTP status first
      if (historyError) {
        Serial.print(F("deserializeJson Failed (History): "));
        Serial.println(historyError.f_str());
      } else {
        Serial.print("History HTTP Status Code: ");
        Serial.println(historyHttpCode);

        // Correctly access the "Data" array nested within the "Data" object.
        // The variable historicalDataArray is correctly declared as JsonArray.
        JsonArray historicalDataArray = historyDoc["Data"]["Data"].as<JsonArray>();
        for (JsonVariant close : historicalDataArray) {
          Serial.println(close.as<JsonObject>()["close"].as<double>());//Prints the closing price of todays price. 
        }


        // Check if the array contains at least two data points (yesterday and today).
        if (historicalDataArray.size() >= 2) {
          // The last element (index size - 1) is typically the most recent day (current partial day).
          // The second to last element (index size - 2) is the last complete day (yesterday's close).
          // Access the specific JsonObject for yesterday, then its "close" field.
          yesterdayPrice = historicalDataArray[historicalDataArray.size() - 2 ]["close"].as<double>();
          Serial.print("Yesterday's Close Price (from history): ");
          Serial.println(yesterdayPrice, 2); // Print with 2 decimal places
          //test = false;
        } else {
          Serial.println("Not enough historical data points to determine yesterday's price.");
          // yesterdayPrice remains 0.0 if not enough data, which is handled by subsequent checks.
        }
      }
    } else {
      Serial.print("HTTP Request Failed (History) with code: ");
      Serial.println(historyHttpCode);
      Serial.println("Response: " + http.getString());
    }
  //}

  http.end(); // Close the HTTP connection for historical data

  // --- 3. Calculate and Display Percent Change ---
  // Ensure we have valid data (prices are greater than 0) before calculating percent change
  
  if (currentPrice > 0 && yesterdayPrice > 0) {
    bool isUp = (currentPrice >= yesterdayPrice); // Use >= to include no change as 'up'
    double percentChange;

    if (yesterdayPrice != 0) { // Safety check against division by zero
      if (isUp) {
        percentChange = ((currentPrice - yesterdayPrice) / yesterdayPrice) * 100.0;
      } else {
        percentChange = ((yesterdayPrice - currentPrice) / yesterdayPrice) * 100.0;
      }
    } else {
      percentChange = 0.0; // Cannot calculate if yesterdayPrice is zero
      Serial.println("Error: yesterdayPrice is zero, cannot calculate percentage change.");
    }

    Serial.print("Percent Change: ");
    Serial.println(percentChange, 2); // Print with 2 decimal places
    
    // --- Display on OLED ---
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    printCenter("BTC/USD", 0, 0);

    display.setTextSize(2);
    printCenter("$" + String(currentPrice, 2), 0, 25); // Display current price, formatted to 2 decimal places

    String dayChangeString = "24hr. Change: ";
    if (!isUp) { // If price went down, add '-'
      dayChangeString += "-";
    }
    dayChangeString += String(percentChange, 2) + "%"; // Display percentage change, formatted to 2 decimal places
    display.setTextSize(1);
    printCenter(dayChangeString, 0, 55);
    display.display();

  } else {
    // If data fetching failed or insufficient, display an error message
    Serial.println("Insufficient data to calculate and display price change.");
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.println("Data fetch failed");
    display.println("or insufficient.");
    display.display();
  }
  
  delay(5000); // Wait 5 seconds before next update cycle
  
}

void printCenter(const String buf, int x, int y) {
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(buf, x, y, &x1, &y1, &w, &h);  //calc width of new string
  display.setCursor((x - w / 2) + (SCREEN_WIDTH / 2), y);
  display.print(buf);
}
