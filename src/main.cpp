#include <ESPAsyncWebServer.h>
#include <WiFi.h>

// Access Point Configuration
const char *apSSID = "ESP32-Access-Point";
const char *apPassword = "12345678";

bool wifiConnecting = false;
bool networkServerStarted = false;

String ssid;
String password;
String customPassword;

// Initial Web Server (for AP mode)
AsyncWebServer *apServer = nullptr;
// New Web Server (for STA mode)
AsyncWebServer *networkServer = nullptr;

// Forward declaration for the network server function
void startNetworkServer();

void setup() {
    Serial.begin(115200);

    // Start Access Point
    WiFi.softAP(apSSID, apPassword);
    Serial.println("Access Point gestartet");
    Serial.print("AP IP: ");
    Serial.println(WiFi.softAPIP());

    // Configure initial server for AP mode
    apServer = new AsyncWebServer(80);
    apServer->on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(
            200, "text/html",
            "<form action=\"/submit\" method=\"POST\">"
            "SSID: <input type=\"text\" name=\"ssid\"><br>"
            "Passwort für SSID: <input type=\"text\" name=\"password\"><br>"
            "Custom Passwort: <input type=\"text\" name=\"customPassword\"><br>"
            "<input type=\"submit\" value=\"Submit\"></form>");
    });

    apServer->on("/submit", HTTP_POST, [](AsyncWebServerRequest *request) {
        // Extract form parameters
        ssid = request->getParam("ssid", true)->value();
        password = request->getParam("password", true)->value();
        customPassword = request->getParam("customPassword", true)->value();

        Serial.println("SSID: " + ssid);
        Serial.println("Passwort: " + password);
        Serial.println("Custom Passwort: " + customPassword);

        request->send(200, "text/html", 
                      "Einstellungen gespeichert. Verbinde zu " + ssid + "...");

        WiFi.softAPdisconnect(false);

        // Shutdown the Access Point and stop the AP server
        apServer->end();
        delete apServer; // Fully delete the server instance
        apServer = nullptr;

        WiFi.disconnect(true);  // Clean up any lingering connections
        WiFi.mode(WIFI_STA);    // Switch to STA mode
        Serial.println("Access Point deaktiviert.");
        wifiConnecting = true;

        // Start connecting to the provided WiFi network
        WiFi.begin(ssid.c_str(), password.c_str());
    });

    apServer->begin();
}

void loop() {
    if (wifiConnecting) {
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("Verbunden!");
            Serial.print("STA IP: ");
            Serial.println(WiFi.localIP());
            wifiConnecting = false;

            // Start the new network server
            if (!networkServerStarted) {
                delay(2000);
                startNetworkServer();
                networkServerStarted = true;
            }
        } else {
            static unsigned long lastAttempt = 0;
            if (millis() - lastAttempt > 1000) {  // Retry every second
                lastAttempt = millis();
                Serial.println("Verbindung wird hergestellt...");
            }
        }
    }
}

void startNetworkServer() {
    // Dynamically allocate a new server instance
    networkServer = new AsyncWebServer(80);

    networkServer->on("/password", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/plain", customPassword);
    });

    networkServer->begin();
    Serial.println("Netzwerk-Server gestartet.");
}
