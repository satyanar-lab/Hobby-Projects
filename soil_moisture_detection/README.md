# Soil Moisture Detection Using IoT

An Arduino-based smart irrigation prototype that monitors soil moisture, temperature, and humidity, and automatically controls a water pump using a relay.

## Overview

This project reads data from:
- a soil moisture sensor
- a DHT11 temperature and humidity sensor

The measured values are displayed on a 16x2 LCD. When the soil moisture falls below a defined threshold, the system automatically turns the pump ON through a relay. When the moisture level is sufficient, the pump turns OFF.

## Features

- Soil moisture monitoring
- Temperature and humidity sensing using DHT11
- 16x2 LCD display output
- Automatic pump control using relay
- Threshold-based irrigation logic
- Serial monitor output for debugging

## Components Used

- Arduino Uno
- Soil Moisture Sensor
- DHT11 Sensor
- 16x2 LCD Display
- Relay Module
- Water Pump
- Jumper wires
- Breadboard / power supply

## Working Principle

1. The DHT11 sensor measures temperature and humidity.
2. The soil moisture sensor reads the moisture level of the soil.
3. The moisture value is converted into a percentage.
4. If the moisture percentage is below the threshold, the relay activates the pump.
5. If the moisture percentage is above the threshold, the pump is turned off.
6. The values are displayed on the LCD.

## Pin Connections

| Component | Arduino Pin |
|----------|-------------|
| DHT11 Data Pin | D5 |
| Soil Moisture Sensor Output | A0 |
| Relay Input | D6 |
| LCD RS | D7 |
| LCD EN | D8 |
| LCD D4 | D9 |
| LCD D5 | D10 |
| LCD D6 | D11 |
| LCD D7 | D12 |

## Libraries Required

Install these libraries before uploading the code:
- LiquidCrystal
- dht library compatible with `#include <dht.h>`

## File Structure

soil-moisture-detection-iot/
├── README.md
└── soil_moisture_detection.ino

## How to Run

1. Connect all components to the Arduino.
2. Open the `.ino` file in Arduino IDE.
3. Install the required libraries.
4. Select the correct board and COM port.
5. Upload the code.
6. Use Serial Monitor for debugging if needed.

## Calibration Notes

The current calibration values are:
- `DRY_SOIL_VALUE = 550`
- `WET_SOIL_VALUE = 10`

You may need to adjust them depending on your sensor and soil conditions.

## Future Improvements
- Add Wi-Fi support using ESP8266 or ESP32
- Send data to a cloud dashboard
- Add notifications
- Log readings over time
- Improve pump control using hysteresis


## Author

Pavankumar Satyanarayan
