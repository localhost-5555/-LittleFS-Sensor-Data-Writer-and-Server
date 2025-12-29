#include <FS.h>
#include <LittleFS.h>
#include <time.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
#include "secrets.h"
#include "lfs_functions.h" // LittleFS custom library with basic functions

#define DHTPIN D4 // Signal pin used in WeMos D1 mini ESP8266 for DHT11 sensor 
#define DHTTYPE DHT11

DHT_Unified dht(DHTPIN, DHTTYPE);
ESP8266WebServer server(80);

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

// Handle file download
void handleDownload() {
    File file = LittleFS.open("/data.csv", "r");
    if (!file) {
        server.send(404, "text/plain", "File not found");
        return;
    }
    
    server.sendHeader("Content-Disposition", "attachment; filename=data.csv");
    server.streamFile(file, "text/csv");
    file.close();
}

// Handle data retrieval from data.csv
void getData() {
    File file = LittleFS.open("/data.csv", "r");
    if (!file) {
        server.send(404, "text/plain", "File not found");
        return;
    }
    server.streamFile(file, "text/csv");
    file.close();
}

// Handle web page to display file contents
void handleDashboard() {
    String html = R"rawliteral(
    <!DOCTYPE html>
    <html>
        <head>
            <title>LittleFS Sensor Data Writer and Server</title>
            <style>
                *{
                    box-sizing: border-box;
                    margin: 0;
                    padding: 0;
                }
                body {
                    display: flex;
                    flex-direction: column;
                    justify-content: center;
                    font-family:Arial;
                    margin:2rem;
                    text-align: center;
                    gap: 2rem;
                }
                .button {
                    display: inline-block;
                    margin: 10px 5px 0 0;
                    padding: 10px 20px;
                    color: white;
                    text-decoration: none;
                    border-radius: 5px;
                    width: 120px;
                }
                .download {
                    background: #34D399;
                }
                .download:hover {
                    background: #17c384;
                }
                .danger {
                    background: #fe4d41;
                }
                .danger:hover {
                    background: #e94337;
                }
                #csv-table tr:first-child td {
                    font-weight: bold;
                    background-color: #c6e2d7;
                }
                #csv-table td{
                    background-color: #deede7;
                    padding: 1rem;
                    border: 1px solid white;
                    border-radius: 5%;
                    font-size: large;
                }
            </style>
        </head>
        <body>
            <h1>LittleFS Sensor Data Writer and Server</h1>
            <div>
                <a class="button download" href='/download'>Download CSV</a>
                <a class="button danger" id="delete" href='/delete'>Delete Data</a>
            </div>
            <table id="csv-table">
                
            </table>
            <script>
                // Wait for DOM to be ready
                document.addEventListener('DOMContentLoaded', () => {
                    
                    // Ask for confirmation before deleting data.csv file
                    document.getElementById('delete').addEventListener('click', (e) => {
                        e.preventDefault()
                        res = confirm('Your about to delete your data, please confirm')
                        if(res){
                            console.log('data deleted');
                            location.pathname = '/delete';
                        }

                    })

                    async function loadCSVData() {
                        const response = await fetch('/data');
                        const csvText = await response.text();
                        
                        console.log(csvText);
                        const rows = csvText.split('\n');
                        console.log(rows.length);
                        const table = document.getElementById('csv-table');
                        table.innerHTML = ""; // Clear existing data

                        // Add data to table
                        rows.forEach(row => {
                            const columns = row.split(',');
                            if (columns.length > 1) { // Skip empty lines
                            const tr = document.createElement('tr');
                            columns.forEach(cell => {
                                const td = document.createElement('td');
                                td.textContent = cell;
                                tr.appendChild(td);
                            });
                            table.appendChild(tr);
                            }
                        });
                    }

                    // Load data when page opens
                    loadCSVData();
                });
            </script>
        </body>
    </html>
    )rawliteral";
    server.send(200, "text/html", html);
}

// Handle file deletion
void handleDelete() {
    if (LittleFS.remove("/data.csv")) {
        // Recreate with header
        File file = LittleFS.open("/data.csv", "w");
        if (file) {
            file.println("Temperature(C),Humidity(%),Timestamp");
            file.close();
        }
        server.send(200, "text/html", "<h1>Data cleared!</h1><a href='/'>Back to home</a>");
    } else {
        server.send(500, "text/plain", "Failed to delete file");
    }
}

void setupWebServer() {
    // Setup web server routes
    server.on("/", handleDashboard);
    server.on("/download", handleDownload);
    server.on("/delete", handleDelete);
    server.on("/data", getData);

    server.begin();
    Serial.println("Web server started");
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
        Serial.print("IP address: ");   
        Serial.println(WiFi.localIP());
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

    setupWebServer();

    // Initialize DHT sensor
    dht.begin();
    Serial.println("DHT11 sensor initialized");
}

void loop() {
    server.handleClient();  // Handle incoming web requests (non-blocking)
    
    // Check if it's time to read sensors (3 minutes = 180000 ms)
    static unsigned long lastReadTime = 0;
    unsigned long currentTime = millis();
    
    if (currentTime - lastReadTime >= 10000) {  // 3 minutes
        lastReadTime = currentTime;
        
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

        // Format CSV line
        String timestamp = getTimestamp();
        snprintf(buffer, sizeof(buffer), "%.1f,%.1f,%s\n", temp, hum, timestamp.c_str());

        // Append to file
        appendFile("/data.csv", buffer);
        
        Serial.print("Logged: ");
        Serial.print(buffer);
    }
    
    delay(10);  // Small delay to prevent watchdog timeout, doesn't block server
}