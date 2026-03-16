#include <dht.h>
#include <LiquidCrystal.h>

// -----------------------------
// Hardware configuration
// -----------------------------
constexpr int LCD_RS = 7;
constexpr int LCD_EN = 8;
constexpr int LCD_D4 = 9;
constexpr int LCD_D5 = 10;
constexpr int LCD_D6 = 11;
constexpr int LCD_D7 = 12;

constexpr int DHT11_PIN = 5;
constexpr int SOIL_SENSOR_PIN = A0;
constexpr int RELAY_PIN = 6;

// -----------------------------
// Sensor calibration values
// Adjust these based on your own sensor readings
// -----------------------------
constexpr int DRY_SOIL_VALUE = 550;
constexpr int WET_SOIL_VALUE = 10;

// Moisture threshold below which pump turns ON
constexpr int MOISTURE_THRESHOLD = 30;

// Relay logic
constexpr int PUMP_ON_STATE = LOW;
constexpr int PUMP_OFF_STATE = HIGH;

// Display timings
constexpr unsigned long SENSOR_SCREEN_DELAY_MS = 2000;
constexpr unsigned long MOISTURE_SCREEN_DELAY_MS = 2500;

// -----------------------------
// Global objects
// -----------------------------
LiquidCrystal lcd(LCD_RS, LCD_EN, LCD_D4, LCD_D5, LCD_D6, LCD_D7);
dht DHT;

// -----------------------------
// Helper functions
// -----------------------------
int readMoisturePercent() {
  int rawValue = analogRead(SOIL_SENSOR_PIN);
  int moisturePercent = map(rawValue, DRY_SOIL_VALUE, WET_SOIL_VALUE, 0, 100);
  moisturePercent = constrain(moisturePercent, 0, 100);
  return moisturePercent;
}

bool shouldPumpBeOn(int moisturePercent) {
  return moisturePercent < MOISTURE_THRESHOLD;
}

void displayTemperatureHumidity() {
  int status = DHT.read11(DHT11_PIN);

  lcd.clear();
  lcd.setCursor(0, 0);

  if (status == 0) {
    lcd.print("Temp: ");
    lcd.print(DHT.temperature);
    lcd.print((char)223);
    lcd.print("C");

    lcd.setCursor(0, 1);
    lcd.print("Humidity: ");
    lcd.print(DHT.humidity);
    lcd.print("%");
  } else {
    lcd.print("DHT11 Read Error");
    lcd.setCursor(0, 1);
    lcd.print("Check sensor");
  }
}

void displayMoistureAndPumpStatus(int moisturePercent, bool pumpOn) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Moisture: ");
  lcd.print(moisturePercent);
  lcd.print("%");

  lcd.setCursor(0, 1);
  lcd.print("Pump: ");
  lcd.print(pumpOn ? "ON" : "OFF");
}

void setPumpState(bool pumpOn) {
  digitalWrite(RELAY_PIN, pumpOn ? PUMP_ON_STATE : PUMP_OFF_STATE);
}

void printDebugInfo(int moisturePercent, bool pumpOn) {
  Serial.print("Temperature: ");
  Serial.print(DHT.temperature);
  Serial.print(" C, Humidity: ");
  Serial.print(DHT.humidity);
  Serial.print(" %, Moisture: ");
  Serial.print(moisturePercent);
  Serial.print(" %, Pump: ");
  Serial.println(pumpOn ? "ON" : "OFF");
}

// -----------------------------
// Arduino setup
// -----------------------------
void setup() {
  lcd.begin(16, 2);
  Serial.begin(9600);

  pinMode(SOIL_SENSOR_PIN, INPUT);
  pinMode(RELAY_PIN, OUTPUT);

  setPumpState(false);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Soil Moisture");
  lcd.setCursor(0, 1);
  lcd.print("System Start");
  delay(1500);
}

// -----------------------------
// Main loop
// -----------------------------
void loop() {
  displayTemperatureHumidity();
  delay(SENSOR_SCREEN_DELAY_MS);

  int moisturePercent = readMoisturePercent();
  bool pumpOn = shouldPumpBeOn(moisturePercent);

  setPumpState(pumpOn);
  displayMoistureAndPumpStatus(moisturePercent, pumpOn);
  printDebugInfo(moisturePercent, pumpOn);

  delay(MOISTURE_SCREEN_DELAY_MS);
}
