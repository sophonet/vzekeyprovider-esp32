#include <ESPAsyncWebServer.h>
#include <Ticker.h>
#include <DNSServer.h>
#include <WiFi.h>
#include <LittleFS.h>

// Access Point Configuration
const char *apSSID = "ZFS-Keyprovider-Access-Point";
const char *apPassword = "yFRAzS$wwB$4f";
const char *partner_host = "partner.example.com";

bool wifiConnecting = false;
bool networkServerStarted = false;

String ssid;
String password;
String enczfskey;

Ticker ledTicker;

DNSServer dnsServer;

IPAddress ip;

const int ledPin = LED_BUILTIN;  // Built-in LED pin
const int DNS_PORT = 53;

// Initial Web Server (for AP mode)
AsyncWebServer *apServer = nullptr;
// New Web Server (for STA mode)
AsyncWebServer *networkServer = nullptr;

// Forward declaration for the network server function
void startNetworkServer();

void toggleLED() {
  const int ledStatus = digitalRead(ledPin);
  if(ledStatus == HIGH) {
    digitalWrite(ledPin, LOW);
  } else {
    digitalWrite(ledPin, HIGH);
  }
}

boolean captivePortal(AsyncWebServerRequest *request) {
  if (ip.toString() != request->host()) {
    Serial.println("Request redirected to captive portal");
    Serial.println(String("http://") + ip.toString());
    AsyncWebServerResponse *response = request->beginResponse(302, "text/plain", "");
    response->addHeader("Location", String("http://") + ip.toString());
    request->send(response);
    return true;
  }
  return false;
}

void loginPage(AsyncWebServerRequest *request) {
    request->send(
        200, "text/html",
        "<html><head>"
        "<link rel=\"stylesheet\" href=\"/pure-min.css\">"
        "<title>ZFS Key Provider</title>"
        "<style> .form-container { display: flex; justify-content: center; }</style>"
        "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"></head>"
        "<body><div class=\"form-container\"><form class=\"pure-form pure-form-stacked center\" action=\"/set_password\" method=\"POST\">"
        "<fieldset>"
        "<label for=\"ssid\">WLAN SSID</label>"
        "<input type=\"text\" name=\"ssid\" />"
        "<label for=\"password\">WLAN password</label>"
        "<input type=\"password\" name=\"password\" />"
        "<label for=\"secret\">Encrypted ZFS Key</label>"
        "<input type=\"text\" name=\"enczfskey\" />"
        "<button type=\"submit\" class=\"pure-button pure-button-primary\">Launch</button></fieldset></form></div></body></html>");
}

void setup() {
    Serial.begin(115200);

    if(!LittleFS.begin(true)){
        Serial.println("An Error has occurred while mounting SPIFFS");
        return;
    }

    pinMode(ledPin, OUTPUT);

    // Start Access Point
    WiFi.softAP(apSSID, apPassword);
    ip = WiFi.softAPIP();
    Serial.println("Access Point gestartet");
    Serial.print("AP IP: ");
    Serial.println(ip);

    dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
    dnsServer.start(DNS_PORT, "*", ip); // Start DNS server on port 53, responding to all domains with AP IP
  
    // Configure initial server for AP mode
    apServer = new AsyncWebServer(80);
    apServer->onNotFound(loginPage);

    apServer->on("/pure-min.css", HTTP_GET, [](AsyncWebServerRequest *request){
        Serial.println("Requesting pure-min.css");
        request->send(LittleFS, "/pure-min.css", "text/css");
    });

    apServer->on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (captivePortal(request)) { 
          return;
        } else {
            loginPage(request);
        }
    });

    apServer->on("/submit", HTTP_POST, [](AsyncWebServerRequest *request) {
        // Extract form parameters
        ssid = request->getParam("ssid", true)->value();
        password = request->getParam("password", true)->value();
        enczfskey = request->getParam("enczfskey", true)->value();

        Serial.println("SSID: " + ssid);
        Serial.println("Passwort: " + password);
        Serial.println("Custom Passwort: " + enczfskey);

        request->send(200, "text/html",
                      "Einstellungen gespeichert. Verbinde zu " + ssid + "...");

        WiFi.softAPdisconnect(false);

        // Shutdown the Access Point and stop the AP server
        apServer->end();
        delete apServer;  // Fully delete the server instance
        apServer = nullptr;

        WiFi.disconnect(true);  // Clean up any lingering connections
        WiFi.mode(WIFI_STA);    // Switch to STA mode
        Serial.println("Access Point deaktiviert.");
        wifiConnecting = true;

        ledTicker.detach();

        // Start connecting to the provided WiFi network
        WiFi.begin(ssid.c_str(), password.c_str());
    });

    // Toggle LED every second in AP mode
    ledTicker.attach(2, toggleLED);

    apServer->begin();
}

void loop() {
    dnsServer.processNextRequest();
    if (wifiConnecting) {
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("Verbunden!");
            Serial.print("STA IP: ");
            Serial.println(WiFi.localIP());

            ledTicker.detach();
            digitalWrite(ledPin, LOW);

            wifiConnecting = false;

            // Start the new network server
            if (!networkServerStarted) {
                delay(10000);
                startNetworkServer();
                networkServerStarted = true;
                digitalWrite(ledPin, HIGH);
            }
        } else {
            static unsigned long lastAttempt = 0;
            if (millis() - lastAttempt > 1000) {  // Retry every second
                lastAttempt = millis();
                toggleLED();
                Serial.println("Verbindung wird hergestellt...");
            }
        }
    }
}

void startNetworkServer() {
    // Dynamically allocate a new server instance
    networkServer = new AsyncWebServer(80);

    networkServer->on("/password", HTTP_GET,
                      [](AsyncWebServerRequest *request) {
                        if (request->host() != partner_host) {
                            request->send(403, "text/plain", "Forbidden");
                            return;
                        }
                          request->send(200, "text/plain", enczfskey);
                      });

    networkServer->begin();
    Serial.println("Netzwerk-Server gestartet.");
}
