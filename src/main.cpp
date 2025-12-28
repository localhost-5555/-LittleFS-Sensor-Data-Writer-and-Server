#include <FS.h>
#include <LittleFS.h>
#include <time.h>
#include <ESP8266WiFi.h>

#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
#include "secrets.h"
#include "lfs_functions.h"

#define DHTPIN D4 // Signal pin used in WeMos D1 mini ESP8266 for DHT11 sensor 
#define DHTTYPE DHT11

DHT_Unified dht(DHTPIN, DHTTYPE);

const char *ssid = SECRET_SSID;
const char *pass = SECRET_PASS;

long timezone = -5; // Central America Time Zone
byte daysavetime = 0;

// Simple helper to get current time as formatted string
String getTimestamp() {
    time_t now = time(nullptr);
    struct tm *timeinfo = localtime(&now);
    char buffer[20];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
    return String(buffer);
}

void setup()
{
    Serial.begin(115200);
    delay(1000);
    
    Serial.print("Connecting to ");
    Serial.println(ssid);

    WiFi.begin(ssid, pass);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20)
    {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi connected");
    } else {
        Serial.println("\nWiFi connection failed");
    }

    // Configure time with NTP
    configTime(3600 * timezone, daysavetime * 3600, "time.nist.gov", "0.pool.ntp.org");
    
    // Wait for time to be set
    Serial.println("Waiting for NTP time...");
    time_t now = time(nullptr);
    int time_attempts = 0;
    while (now < 24 * 3600 && time_attempts < 20)
    {
        delay(500);
        Serial.print(".");
        now = time(nullptr);
        time_attempts++;
    }
    Serial.println();

    // Mount LittleFS
    if (!LittleFS.begin())
    {
        Serial.println("LittleFS mount failed");
        return;
    }
    Serial.println("LittleFS mounted");

    // Create CSV file with header if it doesn't exist
    if (!LittleFS.exists("/data.csv")) {
        File file = LittleFS.open("/data.csv", "w");
        if (file) {
            file.println("Temperature(C),Humidity(%),Timestamp");
            file.close();
            Serial.println("Created data.csv with header");
        }
    }

    // Initialize DHT sensor
    dht.begin();
    Serial.println("DHT11 sensor initialized");
    Serial.println("Custom fs library");
}

void loop() {
    delay(5000); // 5 second delay between readings
    
    // Check if time is valid
    time_t now = time(nullptr);
    if (now < 24 * 3600) {
        Serial.println("Time not set yet");
        return;
    }

    sensors_event_t event;
    char buffer[80];

    // Get Temperature
    dht.temperature().getEvent(&event);
    float temp = event.temperature;

    // Get Humidity
    dht.humidity().getEvent(&event);
    float hum = event.relative_humidity;

    if (isnan(temp) || isnan(hum)) {
        Serial.println("Error reading DHT sensor!");
        return;
    }

    // Format CSV line: Temp, Humidity, Timestamp
    String timestamp = getTimestamp();
    snprintf(buffer, sizeof(buffer), "%.1f,%.1f,%s\n", temp, hum, timestamp.c_str());

    // Append to file
    appendFile("/data.csv", buffer);
    
    // Print to serial for verification
    Serial.print("Logged: ");
    Serial.print(buffer);

    // Print data.csv content
    readFile("/data.csv");
}