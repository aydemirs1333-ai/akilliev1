#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Wi-Fi bilgilerini kendi agina gore degistir.
const char* WIFI_SSID = "WIFI_ADI";
const char* WIFI_PASSWORD = "WIFI_SIFRESI";

// Ev agina baglanamazsa telefonun baglanmasi icin AP acilir.
const char* AP_SSID = "AkilliEv-ESP32";
const char* AP_PASSWORD = "12345678";

#define DHTPIN 4
#define DHTTYPE DHT11

// Pinler. Kendi devrene gore gerekirse degistir.
const int GAS_PIN = 34;
const int SOUND_PIN = 35;
const int LIGHT_PIN = 32;
const int WATER_PIN = 33;
const int FLAME_PIN = 27;
const int DOOR_PIN = 14;
const int MOTION_PIN = 12;

const int STRIP_LED_PIN = 13;
const int BUZZER_PIN = 25;
const int WARNING_LED_PIN = 26;
const int PUMP_PIN = 23;

// Cogu role kartinda aktif seviye LOW olabilir. Gerekirse ters cevir.
const int OUTPUT_ON = HIGH;
const int OUTPUT_OFF = LOW;

const int GAS_WARN_THRESHOLD = 300;
const int GAS_DANGER_THRESHOLD = 450;
const int LIGHT_LOW_THRESHOLD = 25;
const int HUMIDITY_LOW_THRESHOLD = 35;
const int WATER_LOW_THRESHOLD = 15;

WebServer server(80);
DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27, 20, 4);

struct SmartState {
    int gas = 0;
    int sound = 0;
    int light = 0;
    int water = 0;
    float temperature = 0;
    float humidity = 0;
    bool flame = false;
    bool door = false;
    bool motion = false;
    bool fan = false;
    bool stripLed = false;
    bool buzzer = false;
    bool warningLed = false;
    bool pump = false;
    String lcdLines[4];
};

SmartState smart;

bool manualStripLed = false;
bool manualBuzzer = false;
bool manualWarningLed = false;
bool manualPump = false;

unsigned long lastSensorRead = 0;
unsigned long lastLcdDraw = 0;

String fit20(String text) {
    if (text.length() > 20) {
        text = text.substring(0, 20);
    }

    while (text.length() < 20) {
        text += " ";
    }

    return text;
}

void sendCors() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
    server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
}

String contentType(const String& path) {
    if (path.endsWith(".html")) return "text/html";
    if (path.endsWith(".css")) return "text/css";
    if (path.endsWith(".js")) return "application/javascript";
    if (path.endsWith(".json")) return "application/json";
    if (path.endsWith(".png")) return "image/png";
    if (path.endsWith(".jpg")) return "image/jpeg";
    if (path.endsWith(".svg")) return "image/svg+xml";
    return "text/plain";
}

bool handleFileRead(String path) {
    if (path == "/") {
        path = "/index.html";
    }

    if (!SPIFFS.exists(path)) {
        return false;
    }

    File file = SPIFFS.open(path, "r");
    if (!file) {
        return false;
    }

    sendCors();
    server.streamFile(file, contentType(path));
    file.close();
    return true;
}

void updateOutputs() {
    digitalWrite(STRIP_LED_PIN, smart.stripLed ? OUTPUT_ON : OUTPUT_OFF);
    digitalWrite(BUZZER_PIN, smart.buzzer ? OUTPUT_ON : OUTPUT_OFF);
    digitalWrite(WARNING_LED_PIN, smart.warningLed ? OUTPUT_ON : OUTPUT_OFF);
    digitalWrite(PUMP_PIN, smart.pump ? OUTPUT_ON : OUTPUT_OFF);
}

void buildLcdLines() {
    if (smart.flame) {
        smart.lcdLines[0] = fit20("ALEV ALGILANDI");
        smart.lcdLines[1] = fit20("MUTFAK KONTROL ET");
        smart.lcdLines[2] = fit20("BUZZER VE LED AKTIF");
        smart.lcdLines[3] = fit20("HEMEN MUDAHALE ET");
        return;
    }

    if (smart.gas >= GAS_WARN_THRESHOLD) {
        smart.lcdLines[0] = fit20("GAZ UYARISI");
        smart.lcdLines[1] = fit20("PPM: " + String(smart.gas));
        smart.lcdLines[2] = fit20(smart.fan ? "TAHLIYE FANI ACIK" : "FAN KAPALI");
        smart.lcdLines[3] = fit20("HAVALANDIRMA YAP");
        return;
    }

    if (smart.door || smart.motion) {
        smart.lcdLines[0] = fit20("GIRIS HAREKETI");
        smart.lcdLines[1] = fit20(smart.door ? "KAPI ACIK" : "KAPI KAPALI");
        smart.lcdLines[2] = fit20(smart.motion ? "HAREKET ALGILANDI" : "HAREKET YOK");
        smart.lcdLines[3] = fit20("GUVENLIK KONTROL");
        return;
    }

    smart.lcdLines[0] = fit20("SIC " + String((int)smart.temperature) + "C NEM %" + String((int)smart.humidity));
    smart.lcdLines[1] = fit20("GAZ " + String(smart.gas) + " SES " + String(smart.sound));
    smart.lcdLines[2] = fit20("SU %" + String(smart.water) + " ISIK %" + String(smart.light));
    smart.lcdLines[3] = fit20("SISTEM NORMAL");
}

void drawLcd() {
    lcd.clear();
    for (int i = 0; i < 4; i++) {
        lcd.setCursor(0, i);
        lcd.print(fit20(smart.lcdLines[i]));
    }
}

int mapPercent(int raw) {
    return constrain(map(raw, 0, 4095, 0, 100), 0, 100);
}

int mapPercentReversed(int raw) {
    return constrain(map(raw, 4095, 0, 0, 100), 0, 100);
}

void readSensors() {
    int gasRaw = analogRead(GAS_PIN);
    int soundRaw = analogRead(SOUND_PIN);
    int lightRaw = analogRead(LIGHT_PIN);
    int waterRaw = analogRead(WATER_PIN);

    smart.gas = constrain(map(gasRaw, 0, 4095, 0, 1000), 0, 1000);
    smart.sound = constrain(map(soundRaw, 0, 4095, 0, 100), 0, 100);

    // LDR devresine gore ters olabilir. Ters okuyorsa mapPercent kullan.
    smart.light = mapPercentReversed(lightRaw);
    smart.water = mapPercent(waterRaw);

    float t = dht.readTemperature();
    float h = dht.readHumidity();

    if (!isnan(t)) smart.temperature = t;
    if (!isnan(h)) smart.humidity = h;

    // Alev modullerinin cogu LOW geldiginde algilama yapar.
    smart.flame = digitalRead(FLAME_PIN) == LOW;

    // Kapi manyetik sensorunde pull-up kullanildi. HIGH ise acik kabul edildi.
    smart.door = digitalRead(DOOR_PIN) == HIGH;
    smart.motion = digitalRead(MOTION_PIN) == HIGH;
}

void applyAutomation() {
    bool autoPump = smart.humidity > 0 && smart.humidity < HUMIDITY_LOW_THRESHOLD && smart.water > WATER_LOW_THRESHOLD;

    smart.fan = smart.gas >= GAS_WARN_THRESHOLD;
    smart.stripLed = manualStripLed || smart.light <= LIGHT_LOW_THRESHOLD || smart.motion;
    smart.warningLed = manualWarningLed || smart.flame || smart.gas >= GAS_WARN_THRESHOLD || smart.door;
    smart.buzzer = manualBuzzer || smart.flame || smart.gas >= GAS_DANGER_THRESHOLD;
    smart.pump = manualPump || autoPump;
}

void sendJsonResponse(const String& payload, int statusCode = 200) {
    sendCors();
    server.send(statusCode, "application/json", payload);
}

void handleOptions() {
    sendCors();
    server.send(204);
}

void handleStatus() {
    DynamicJsonDocument doc(1024);
    doc["gas"] = smart.gas;
    doc["sound"] = smart.sound;
    doc["flame"] = smart.flame;
    doc["water"] = smart.water;
    doc["temperature"] = round(smart.temperature);
    doc["humidity"] = round(smart.humidity);
    doc["light"] = smart.light;
    doc["door"] = smart.door;
    doc["motion"] = smart.motion;
    doc["fan"] = smart.fan;
    doc["stripLed"] = smart.stripLed;
    doc["buzzer"] = smart.buzzer;
    doc["warningLed"] = smart.warningLed;
    doc["pump"] = smart.pump;

    JsonArray lcdLines = doc.createNestedArray("lcdLines");
    for (int i = 0; i < 4; i++) {
        lcdLines.add(smart.lcdLines[i]);
    }

    String payload;
    serializeJson(doc, payload);
    sendJsonResponse(payload);
}

void handleDevice() {
    if (!server.hasArg("plain")) {
        sendJsonResponse("{\"ok\":false,\"message\":\"Body yok\"}", 400);
        return;
    }

    DynamicJsonDocument doc(256);
    DeserializationError error = deserializeJson(doc, server.arg("plain"));

    if (error) {
        sendJsonResponse("{\"ok\":false,\"message\":\"JSON hatali\"}", 400);
        return;
    }

    String device = doc["device"] | "";
    bool value = doc["value"] | false;

    if (device == "stripLed") {
        manualStripLed = value;
    } else if (device == "buzzer") {
        manualBuzzer = value;
    } else if (device == "warningLed") {
        manualWarningLed = value;
    } else if (device == "pump") {
        manualPump = value;
    } else {
        sendJsonResponse("{\"ok\":false,\"message\":\"Bilinmeyen cihaz\"}", 400);
        return;
    }

    applyAutomation();
    updateOutputs();
    buildLcdLines();
    drawLcd();

    sendJsonResponse("{\"ok\":true}");
}

void setupRoutes() {
    server.on("/", HTTP_GET, []() {
        if (!handleFileRead("/index.html")) {
            server.send(500, "text/plain", "index.html SPIFFS icinde bulunamadi");
        }
    });

    server.on("/styles.css", HTTP_GET, []() {
        if (!handleFileRead("/styles.css")) {
            server.send(404, "text/plain", "styles.css yok");
        }
    });

    server.on("/app.js", HTTP_GET, []() {
        if (!handleFileRead("/app.js")) {
            server.send(404, "text/plain", "app.js yok");
        }
    });

    server.on("/api/status", HTTP_OPTIONS, handleOptions);
    server.on("/api/device", HTTP_OPTIONS, handleOptions);
    server.on("/api/status", HTTP_GET, handleStatus);
    server.on("/api/device", HTTP_POST, handleDevice);

    server.onNotFound([]() {
        if (!handleFileRead(server.uri())) {
            server.send(404, "text/plain", "404");
        }
    });
}

void setupWifi() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    Serial.print("Wi-Fi baglaniyor");
    unsigned long startAttempt = millis();

    while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 15000) {
        delay(500);
        Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println();
        Serial.print("Baglandi. IP: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println();
        Serial.println("Ev agina baglanamadi, AP modu aciliyor...");
        WiFi.disconnect(true, true);
        WiFi.mode(WIFI_AP);
        WiFi.softAP(AP_SSID, AP_PASSWORD);
        Serial.print("AP IP: ");
        Serial.println(WiFi.softAPIP());
    }
}

void setup() {
    Serial.begin(115200);

    pinMode(FLAME_PIN, INPUT_PULLUP);
    pinMode(DOOR_PIN, INPUT_PULLUP);
    pinMode(MOTION_PIN, INPUT);

    pinMode(STRIP_LED_PIN, OUTPUT);
    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(WARNING_LED_PIN, OUTPUT);
    pinMode(PUMP_PIN, OUTPUT);

    digitalWrite(STRIP_LED_PIN, OUTPUT_OFF);
    digitalWrite(BUZZER_PIN, OUTPUT_OFF);
    digitalWrite(WARNING_LED_PIN, OUTPUT_OFF);
    digitalWrite(PUMP_PIN, OUTPUT_OFF);

    analogReadResolution(12);

    dht.begin();
    Wire.begin(21, 22);
    lcd.init();
    lcd.backlight();
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Akilli Ev Basliyor");

    if (!SPIFFS.begin(true)) {
        Serial.println("SPIFFS baslatilamadi");
    }

    setupWifi();
    setupRoutes();
    server.begin();

    readSensors();
    applyAutomation();
    buildLcdLines();
    updateOutputs();
    drawLcd();
}

void loop() {
    server.handleClient();

    if (millis() - lastSensorRead >= 800) {
        lastSensorRead = millis();
        readSensors();
        applyAutomation();
        updateOutputs();
    }

    if (millis() - lastLcdDraw >= 1000) {
        lastLcdDraw = millis();
        buildLcdLines();
        drawLcd();
    }
}
