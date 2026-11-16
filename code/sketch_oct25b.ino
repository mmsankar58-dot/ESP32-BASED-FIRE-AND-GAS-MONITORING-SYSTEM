#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <RTClib.h>
#include <DHT.h>

// =====================================================
// WIFI ACCESS POINT
// =====================================================
const char* ssid = "ESP32-Fire-Monitor";
const char* password = "FireAlarm123";

WebServer server(80);

// =====================================================
// PIN CONFIGURATION
// =====================================================

// Flame sensor digital output
#define FLAME_PIN 27

// DHT11 temperature and humidity sensor
#define DHT_PIN 16
#define DHT_TYPE DHT11

// DS3231 uses I2C
#define SDA_PIN 21
#define SCL_PIN 22

// LEDs
#define GREEN_LED 18
#define BLUE_LED  19
#define RED_LED   23

// Push button
#define BUTTON_PIN 26

// MQ-9 analog output through a voltage divider
#define MQ9_PIN 34

// Active buzzer controlled through a transistor
#define BUZZER_PIN 25

// =====================================================
// SENSOR OBJECTS
// =====================================================
DHT dht(DHT_PIN, DHT_TYPE);
RTC_DS3231 rtc;

// =====================================================
// SETTINGS
// =====================================================

// MQ-9 raw ADC threshold: tune after observing readings.
// This is NOT a calibrated ppm or smoke concentration.
int gasThreshold = 1800;

// Temperature alarm threshold
float temperatureThreshold = 50.0;

// Most flame sensor modules have active-LOW digital output.
const int FLAME_ACTIVE_LEVEL = LOW;

// =====================================================
// SENSOR VARIABLES
// =====================================================
float temperature = NAN;
float humidity = NAN;

int gasValue = 0;

bool flameDetected = false;
bool gasAlert = false;
bool temperatureAlert = false;
bool sensorError = false;

bool alarmCondition = false;
bool previousAlarmCondition = false;
bool alarmAcknowledged = false;

bool rtcAvailable = false;

// Button debounce variables
bool lastButtonReading = HIGH;
bool stableButtonState = HIGH;
unsigned long lastDebounceTime = 0;

const unsigned long debounceDelay = 50;

// Sensor sampling interval
unsigned long lastSensorRead = 0;
const unsigned long sensorInterval = 2000;

// =====================================================
// HTML WEBPAGE
// =====================================================
String makePage() {
  String page = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<meta http-equiv="refresh" content="3">
<title>ESP32 Fire and Gas Monitor</title>

<style>
* { box-sizing: border-box; }

body {
  margin: 0;
  padding: 20px 14px 30px;
  font-family: system-ui, -apple-system, "Segoe UI", Roboto, sans-serif;
  color: #e8eef5;
  background: radial-gradient(circle at top, #1b2a41 0%, #0b1220 60%) fixed;
  min-height: 100vh;
}

.container { max-width: 560px; margin: auto; }

header { text-align: center; margin-bottom: 18px; }
header h1 { margin: 0; font-size: 22px; letter-spacing: 2px; font-weight: 700; }
header p  { margin: 4px 0 0; font-size: 13px; color: #8da2b8; }

.card {
  --c: #38bdf8;
  background: rgba(255, 255, 255, 0.05);
  border: 1px solid rgba(255, 255, 255, 0.09);
  border-radius: 18px;
  padding: 16px;
  margin-bottom: 12px;
  backdrop-filter: blur(10px);
}

.card h2 {
  margin: 0 0 10px;
  font-size: 12px;
  font-weight: 600;
  letter-spacing: 1.5px;
  text-transform: uppercase;
  color: #8da2b8;
}

.normal  { --c: #34d399; }
.warning { --c: #fbbf24; }
.danger  { --c: #f87171; }

.hero {
  text-align: center;
  padding: 24px 16px;
  border-color: var(--c);
  box-shadow: 0 0 30px -8px var(--c);
}

.hero .state {
  display: inline-flex;
  align-items: center;
  gap: 10px;
  font-size: 30px;
  font-weight: 800;
  letter-spacing: 2px;
  color: var(--c);
}

.dot {
  width: 12px; height: 12px;
  border-radius: 50%;
  background: var(--c);
  animation: pulse 1.6s infinite;
}

@keyframes pulse {
  0%   { box-shadow: 0 0 0 0 var(--c); }
  70%  { box-shadow: 0 0 0 12px transparent; }
  100% { box-shadow: 0 0 0 0 transparent; }
}

.hero p { margin: 10px 0 0; color: #b6c4d3; font-size: 14px; }

.grid {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 12px;
  margin-bottom: 12px;
}
.grid .card { margin-bottom: 0; }

.value { font-size: 30px; font-weight: 700; }
.value small { font-size: 14px; color: #8da2b8; font-weight: 500; }

.status { font-size: 16px; font-weight: 700; color: var(--c); }

.note { margin: 8px 0 0; font-size: 12px; line-height: 1.5; color: #8da2b8; }

.bar {
  height: 8px;
  margin: 10px 0;
  border-radius: 8px;
  background: rgba(255, 255, 255, 0.08);
  overflow: hidden;
}
.bar span {
  display: block;
  height: 100%;
  width: GAS_PERCENT%;
  border-radius: 8px;
  background: var(--c);
}

button {
  width: 100%;
  margin-top: 12px;
  padding: 14px;
  border: 0;
  border-radius: 12px;
  font-size: 15px;
  font-weight: 700;
  color: #1a1300;
  background: linear-gradient(135deg, #fde047, #f59e0b);
  cursor: pointer;
}
button:active { transform: scale(0.98); }

.legend { display: flex; justify-content: space-around; font-size: 13px; color: #b6c4d3; }
.legend i {
  display: inline-block;
  width: 9px; height: 9px;
  border-radius: 50%;
  margin-right: 6px;
}

footer { text-align: center; margin-top: 18px; font-size: 12px; color: #6b7f94; line-height: 1.6; }
</style>
</head>

<body>
<div class="container">

<header>
  <h1>FIRE &amp; GAS MONITOR</h1>
  <p>ESP32 local sensor dashboard</p>
</header>

<div class="card hero STATUS_CLASS">
  <div class="state"><span class="dot"></span>OVERALL_STATUS</div>
  <p>OVERALL_MESSAGE</p>
</div>

<div class="grid">
  <div class="card">
    <h2>Temperature</h2>
    <div class="value">TEMPERATURE_VALUE</div>
    <p class="note">Alarm threshold: 50 °C</p>
  </div>
  <div class="card">
    <h2>Humidity</h2>
    <div class="value">HUMIDITY_VALUE</div>
  </div>
</div>

<div class="card FLAME_CLASS">
  <h2>Flame Sensor</h2>
  <div class="status">FLAME_STATUS</div>
</div>

<div class="card GAS_CLASS">
  <h2>MQ-9 Gas Sensor</h2>
  <div class="value">GAS_VALUE <small>/ 4095</small></div>
  <div class="bar"><span></span></div>
  <div class="status">GAS_STATUS</div>
  <p class="note">
    Relative sensor value, not a calibrated gas concentration or smoke measurement.
  </p>
</div>

<div class="card">
  <h2>RTC Clock (DS3231)</h2>
  <div class="value">RTC_TIME</div>
  <p class="note">RTC date: RTC_DATE</p>
</div>

<div class="card ACK_CLASS">
  <h2>Alarm</h2>
  <div class="status">ACK_STATUS</div>
  <p class="note">
    Acknowledging silences the buzzer while the current alarm remains active.
    A new alarm condition will reactivate it.
  </p>
  <button onclick="location.href='/ack'">Acknowledge Alarm</button>
</div>

<div class="card">
  <h2>Indicator LEDs</h2>
  <div class="legend">
    <span><i style="background:#34d399"></i>Normal</span>
    <span><i style="background:#38bdf8"></i>Running</span>
    <span><i style="background:#f87171"></i>Alarm</span>
  </div>
</div>

<footer>
  Connect to ESP32-Fire-Monitor to view this page.
</footer>

</div>
</body>
</html>
)rawliteral";

  // Replace unique placeholders with live sensor data.
  page.replace("STATUS_CLASS",
               alarmCondition ? "danger" : "normal");

  page.replace("OVERALL_STATUS",
               alarmCondition ? "ALARM" : "NORMAL");

  String overallMessage;

  if (sensorError) {
    overallMessage = "DHT11 reading unavailable. Check sensor wiring.";
  } else if (flameDetected) {
    overallMessage = "Flame sensor alarm detected!";
  } else if (temperatureAlert && gasAlert) {
    overallMessage = "High temperature and elevated MQ-9 reading!";
  } else if (temperatureAlert) {
    overallMessage = "Temperature is above the configured threshold!";
  } else if (gasAlert) {
    overallMessage = "MQ-9 reading is above its configured threshold!";
  } else {
    overallMessage = "No configured alarm condition detected.";
  }

  page.replace("OVERALL_MESSAGE", overallMessage);

  if (isnan(temperature)) {
    page.replace("TEMPERATURE_VALUE", "-- °C");
  } else {
    page.replace("TEMPERATURE_VALUE",
                 String(temperature, 1) + " °C");
  }

  if (isnan(humidity)) {
    page.replace("HUMIDITY_VALUE", "-- %");
  } else {
    page.replace("HUMIDITY_VALUE",
                 String(humidity, 1) + " %");
  }

  page.replace("FLAME_CLASS",
               flameDetected ? "danger" : "normal");

  page.replace("FLAME_STATUS",
               flameDetected ? "FLAME DETECTED" : "No flame detected");

  page.replace("GAS_VALUE", String(gasValue));

  page.replace("GAS_PERCENT",
               String(constrain((long)gasValue * 100 / 4095, 0, 100)));

  page.replace("GAS_CLASS",
               gasAlert ? "danger" : "normal");

  page.replace("GAS_STATUS",
               gasAlert ? "ELEVATED READING" : "Below configured threshold");

  String timeText = "Unavailable";
  String dateText = "Unavailable";

  if (rtcAvailable) {
    DateTime now = rtc.now();

    char timeBuffer[16];
    char dateBuffer[16];

    snprintf(timeBuffer, sizeof(timeBuffer),
             "%02d:%02d:%02d",
             now.hour(), now.minute(), now.second());

    snprintf(dateBuffer, sizeof(dateBuffer),
             "%04d-%02d-%02d",
             now.year(), now.month(), now.day());

    timeText = String(timeBuffer);
    dateText = String(dateBuffer);
  }

  page.replace("RTC_TIME", timeText);
  page.replace("RTC_DATE", dateText);

  page.replace("ACK_CLASS",
               alarmAcknowledged && alarmCondition
                 ? "warning" : "normal");

  String ackText;

  if (!alarmCondition) {
    ackText = "No active alarm";
  } else if (alarmAcknowledged) {
    ackText = "Alarm acknowledged; buzzer silenced";
  } else {
    ackText = "Active alarm; buzzer enabled";
  }

  page.replace("ACK_STATUS", ackText);

  return page;
}
// =====================================================
// READ SENSORS
// =====================================================
void readSensors() {
  // Flame module digital output
  flameDetected = (digitalRead(FLAME_PIN) == FLAME_ACTIVE_LEVEL);

  // DHT11 readings
  float newTemperature = dht.readTemperature();
  float newHumidity = dht.readHumidity();

  sensorError = isnan(newTemperature) || isnan(newHumidity);

  if (!isnan(newTemperature)) {
    temperature = newTemperature;
  }

  if (!isnan(newHumidity)) {
    humidity = newHumidity;
  }

  // MQ-9 analog output
  gasValue = analogRead(MQ9_PIN);
  gasAlert = (gasValue >= gasThreshold);

  // Temperature alarm
  temperatureAlert =
      !isnan(temperature) &&
      (temperature >= temperatureThreshold);

  // Overall alarm condition
  previousAlarmCondition = alarmCondition;

  alarmCondition =
      flameDetected ||
      gasAlert ||
      temperatureAlert;

  // A newly appearing alarm must not remain acknowledged.
  if (alarmCondition && !previousAlarmCondition) {
    alarmAcknowledged = false;
  }

  // A sensor error is shown on the dashboard and red LED,
  // but does not automatically activate the buzzer.
}

// =====================================================
// BUTTON HANDLING
// =====================================================
void handleButton() {
  bool reading = digitalRead(BUTTON_PIN);

  if (reading != lastButtonReading) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != stableButtonState) {
      stableButtonState = reading;

      // Button pressed: INPUT_PULLUP means LOW.
      if (stableButtonState == LOW && alarmCondition) {
        alarmAcknowledged = true;
      }
    }
  }

  lastButtonReading = reading;
}

// =====================================================
// OUTPUT CONTROL
// =====================================================
void updateOutputs() {
  // Blue LED: ESP32 system running
  digitalWrite(BLUE_LED, HIGH);

  // Green LED: no alarm and no DHT error
  digitalWrite(
    GREEN_LED,
    (!alarmCondition && !sensorError) ? HIGH : LOW
  );

  // Red LED: alarm condition or DHT sensor error
  digitalWrite(
    RED_LED,
    (alarmCondition || sensorError) ? HIGH : LOW
  );

  // Buzzer is driven through a transistor.
  // HIGH at GPIO25 switches the transistor ON.
  bool buzzerShouldSound =
      alarmCondition && !alarmAcknowledged;

  digitalWrite(
    BUZZER_PIN,
    buzzerShouldSound ? HIGH : LOW
  );
}

// =====================================================
// WEB HANDLERS
// =====================================================
void handleRoot() {
  server.send(200, "text/html", makePage());
}

void handleAcknowledge() {
  if (alarmCondition) {
    alarmAcknowledged = true;
  }

  server.sendHeader("Location", "/");
  server.send(303);
}

// =====================================================
// SETUP
// =====================================================
void setup() {
  Serial.begin(115200);

  // Pin modes
  pinMode(FLAME_PIN, INPUT);

  pinMode(GREEN_LED, OUTPUT);
  pinMode(BLUE_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // ESP32 ADC setup
  analogReadResolution(12);

  // Set ADC attenuation for a wider measurable range.
  // GPIO34 must NEVER receive 5 V directly.
  analogSetPinAttenuation(MQ9_PIN, ADC_11db);

  // Start DHT11
  dht.begin();

  // Start I2C and RTC
  Wire.begin(SDA_PIN, SCL_PIN);

  if (rtc.begin()) {
    rtcAvailable = true;

    // If the RTC lost power, set its time to compile time.
    // Set the correct computer date/time before compiling.
    if (rtc.lostPower()) {
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
  } else {
    rtcAvailable = false;
    Serial.println("DS3231 not detected. Check SDA/SCL wiring.");
  }

  // Start Wi-Fi access point
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);

  Serial.println();
  Serial.println("ESP32 Fire and Gas Monitor Started");
  Serial.print("Connect to Wi-Fi: ");
  Serial.println(ssid);
  Serial.print("Dashboard: http://");
  Serial.println(WiFi.softAPIP());

  // Web routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/ack", HTTP_GET, handleAcknowledge);

  server.begin();

  // Initial sensor reading
  readSensors();
  updateOutputs();
}

// =====================================================
// MAIN LOOP
// =====================================================
void loop() {
  server.handleClient();

  handleButton();

  if (millis() - lastSensorRead >= sensorInterval) {
    lastSensorRead = millis();

    readSensors();
    updateOutputs();

    Serial.print("Temperature: ");
    Serial.print(temperature);
    Serial.print(" C | Humidity: ");
    Serial.print(humidity);
    Serial.print(" % | Flame: ");
    Serial.print(flameDetected ? "DETECTED" : "NO");
    Serial.print(" | MQ-9 ADC: ");
    Serial.print(gasValue);
    Serial.print(" | Gas alert: ");
    Serial.print(gasAlert ? "YES" : "NO");
    Serial.print(" | Alarm: ");
    Serial.println(alarmCondition ? "ACTIVE" : "NORMAL");
  }
}