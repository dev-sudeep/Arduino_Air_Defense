# Arduino Air Defense System - Wiring Guide with MAX7219 8x8 LED Matrix

## Overview
This guide explains how to wire the MAX7219 8x8 LED unicolor matrix display to your Arduino along with all other components for the complete Air Defense system.

---

## Components Required

### Main Components
- Arduino (Uno/Nano/Mega)
- Ultrasonic Sensor (HC-SR04)
- MAX7219 LED Driver IC
- 8x8 LED Matrix Display (unicolor)
- Servo Motor (SG90 or similar)
- LED (any color, 5mm)
- Buzzer (piezo)
- Resistors: 1x 10kΩ, 1x 470Ω
- Capacitors: 1x 10µF, 1x 0.1µF (ceramic)
- Breadboard & Jumper Wires

---

## Pin Configuration

### Arduino Pins Used:
- **Pin 5**: Buzzer
- **Pin 6**: Ultrasonic Sensor (ECHO)
- **Pin 7**: Ultrasonic Sensor (TRIG)
- **Pin 9**: Servo Motor
- **Pin 10**: MAX7219 Chip Select (CS)
- **Pin 11**: MAX7219 Data In (DIN)
- **Pin 12**: MAX7219 Clock (CLK)
- **Pin 13**: LED (Alert indicator)

---

## Wiring Diagrams

### 1. MAX7219 8x8 LED Matrix Connection

```
Arduino              MAX7219              8x8 LED Matrix
----------------------------------------------------
Pin 11 (MOSI)  ----> DIN (Pin 1)
Pin 12 (SCK)   ----> CLK (Pin 13)
Pin 10         ----> CS  (Pin 12)
GND            ----> GND (Pins 4, 9, 20)
5V             ----> VCC (Pins 11, 19)

MAX7219 (IC pins):
- Pin 1: DIN (Data In) → Arduino Pin 11
- Pin 4: GND
- Pin 9: GND
- Pin 11: VCC (5V)
- Pin 12: CS (Chip Select) → Arduino Pin 10
- Pin 13: CLK (Clock) → Arduino Pin 12
- Pin 19: VCC (5V)
- Pin 20: GND

8x8 Matrix:
- Row pins (1-8): Connect to MAX7219 row outputs
- Column pins (1-8): Connect to MAX7219 column outputs
(Usually pre-soldered on the backside)
```

### 2. MAX7219 Capacitor Configuration

Add bypass capacitors near the MAX7219 for stable operation:
```
5V ---+--- [0.1µF Ceramic] ---+--- GND
      |                         |
      +--- [10µF Electrolytic] -+--- GND
```

### 3. Ultrasonic Sensor (HC-SR04)

```
Arduino              HC-SR04 Sensor
-----------------------------------
Pin 7        ----> TRIG
Pin 6        ----> ECHO
5V           ----> VCC
GND          ----> GND

Note: If using 5V on ECHO damages 3.3V pins, add voltage divider:
      ECHO (5V) --[10kΩ]--+--[10kΩ]-- GND
                          |
                          +-- Pin 6 (Arduino)
```

### 4. Servo Motor

```
Arduino              Servo Motor
-------------------------------
Pin 9        ----> Signal (Orange/Yellow wire)
5V           ----> Power (Red wire)
GND          ----> Ground (Brown/Black wire)
```

### 5. LED Alert Indicator

```
Arduino              LED
------------------------
Pin 13       ----> Anode (longer leg)
             
LED Cathode (shorter leg) --[470Ω Resistor]-- GND
```

### 6. Buzzer (Piezo)

```
Arduino              Buzzer
------------------------
Pin 5        ----> Positive (+)
GND          ----> Negative (-)
```

---

## Complete Wiring Summary Table

| Component | Arduino Pin | Connection | Voltage |
|-----------|------------|------------|---------|
| MAX7219 DIN | 11 | Data signal | 5V |
| MAX7219 CLK | 12 | Clock signal | 5V |
| MAX7219 CS | 10 | Chip Select | 5V |
| MAX7219 VCC | - | Power | 5V |
| MAX7219 GND | - | Ground | GND |
| HC-SR04 TRIG | 7 | Trigger pulse | 5V |
| HC-SR04 ECHO | 6 | Echo pulse | 5V |
| HC-SR04 VCC | - | Power | 5V |
| HC-SR04 GND | - | Ground | GND |
| Servo Signal | 9 | PWM control | 5V |
| Servo Power | - | Power | 5V* |
| Servo GND | - | Ground | GND |
| LED Anode | 13 | Through 470Ω | 5V |
| LED GND | - | Resistor to GND | GND |
| Buzzer (+) | 5 | Signal | 5V |
| Buzzer (-) | - | Ground | GND |

*Note: Servo can draw significant current. Consider using external power supply.

---

## Power Considerations

### Recommended Power Supply:
- **Arduino**: USB (5V, 500mA) or external 5V adapter
- **Servo Motor**: Separate 5-6V power supply (draws 500mA-1A at full load)
- **Total System**: 2A @ 5V recommended

### Power Wiring:
```
5V PSU (2A) ---|
              |--- Arduino 5V
              |--- MAX7219 VCC
              |--- HC-SR04 VCC
              |--- Servo Power
              
All GND points connected together
```

---

## Hardware Assembly Steps

1. **Mount MAX7219 on breadboard** with supporting capacitors
2. **Connect data lines** (DIN, CLK, CS) from Arduino to MAX7219
3. **Connect power/ground** to MAX7219 (5V and GND)
4. **Mount 8x8 LED matrix** on the backside or solder to MAX7219
5. **Wire HC-SR04** sensor (TRIG, ECHO, VCC, GND)
6. **Connect Servo motor** signal to Pin 9
7. **Add LED and 470Ω resistor** for alert indication
8. **Connect Buzzer** to Pin 5 and GND
9. **Connect all grounds together** (star grounding recommended)
10. **Apply power** and upload code

---

## Library Installation

Install the LedControl library for MAX7219:

1. Open Arduino IDE
2. Go to **Sketch → Include Library → Manage Libraries**
3. Search for "LedControl"
4. Install by **Elek Mikrokontroller** (or latest version)
5. Click Install

---

## Troubleshooting

### LED Matrix Not Displaying:
- Check MAX7219 power connections
- Verify DIN, CLK, CS pin connections
- Check capacitors near MAX7219
- Test with LedControl library examples

### Ultrasonic Sensor Not Reading:
- Verify TRIG and ECHO pins
- Check HC-SR04 power supply
- Add voltage divider if ECHO is 5V logic

### Servo Not Moving:
- Ensure Pin 9 is capable of PWM
- Check servo power supply (separate PSU recommended)
- Verify servo cable connections

### Display Flickering:
- Reduce brightness in code: `lc.setIntensity(0, lower_value);`
- Check power supply stability
- Verify capacitors are properly connected

---

## Code Configuration

In `Defense.ino`, you can adjust:

```cpp
const int DIN_PIN = 11;   // Data In pin
const int CLK_PIN = 12;   // Clock pin
const int CS_PIN  = 10;   // Chip Select pin

// In setup():
lc.setIntensity(0, 8);    // Brightness 0-15 (8 is mid-range)
```

---

## Display Functionality

- **When object detected (≤30cm)**: Shows distance as single digit (0-9) on LED matrix
- **When no object detected**: Shows "NiR" (Not in Range) pattern on LED matrix
- **LED**: Flashes when object is detected
- **Buzzer**: Beeps when object is detected
- **Servo**: Sweeps ±45° when no object; holds position when object detected

---

## Next Steps

1. Assemble hardware according to wiring diagram
2. Upload the code to Arduino
3. Test each component individually
4. Verify distance readings on Serial Monitor
5. Calibrate ultrasonic sensor if needed
6. Adjust brightness and display patterns as desired

