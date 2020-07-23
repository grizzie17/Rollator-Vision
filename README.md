# Rollator-Sensors #

Sonic sensors for a Rollator (Walker)

## ADXL345 Accelerometer ##

The following table identifies the connections for the
ADXL345 to an Arduino Nano

| ADXL345 | Arduino        | Notes                |
| ------- | -------------- | -------------------- |
| GND     | GND            |                      |
| VCC     | 3.3V           |                      |
| CS      | 3.3V           |                      |
| INT1    | D2 (INT0)      | use Interrupts       |
| INT2    | -              |                      |
| SD0     | -              |                      |
| SDA     | A4 (SDA) blue  | 10k resistor to 3.3V |
| SCL     | A5 (SCL) green | 10k resistor to 3.3v |

![Wiring Diagram](adxl345.png)  
Arduino Nano

## HC-SR04 Ultrasonic Sensor ##

There are four HC-SR04 sensors.  Two facing forward,
one facing left, one facing right.

| HC-SR04 | Arduino | Notes          |
| ------- | ------- | -------------- |
| Vcc     | 5v      |                |
| Trigger | D7-D10  | Echo connected |
| Echo    | Trigger |                |
| GND     | GND     |                |


## Power ##

|   mA | Notes             |
| ---: | ----------------- |
| 19.2 | Batteries to Buck |
|  3.6 | Buck to Main      |
| 15.6 | Buck load         |
