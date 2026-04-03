#include <Arduino.h>
#include <Servo.h>
#include <LedControl.h>

Servo myServo;

// MAX7219 Pin definitions
const int DIN_PIN = 11;   // Data In
const int CLK_PIN = 12;   // Clock
const int CS_PIN  = 10;   // Chip Select

// Create LedControl object: (DIN, CLK, CS, number of devices)
LedControl lc = LedControl(DIN_PIN, CLK_PIN, CS_PIN, 1);

// Pin definitions
const int SERVO_PIN  = 9;
const int TRIG_PIN   = 7;
const int ECHO_PIN   = 6;
const int LED_PIN    = 13;
const int BUZZER_PIN = 5;

// Servo settings
const int CENTER_POS  = 90;
const int SWEEP_ANGLE = 45;
const int SWEEP_STEP  = 1;
const int SWEEP_DELAY = 15;

// Distance threshold
const float ALERT_DISTANCE = 30.0;

// LED flash timing (4 flashes per second = 125ms on, 125ms off)
const int LED_INTERVAL = 125;

// Buzzer beep timing
const int BEEP_DUR = 200;
const int BEEP_GAP = 50;

int currentPos = CENTER_POS;
int sweepDirection = 1;

unsigned long previousLedMillis  = 0;
unsigned long previousBeepMillis = 0;
bool ledState  = false;
bool beepState = false;

// Servo sweep uses a simple blocking delay since
// it only runs when no object is detected
unsigned long lastServoStep = 0;

void setup() {
  Serial.begin(9600);
  myServo.attach(SERVO_PIN);
  myServo.write(CENTER_POS);
  delay(500);

  pinMode(TRIG_PIN,   OUTPUT);
  pinMode(ECHO_PIN,   INPUT);
  pinMode(LED_PIN,    OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  // Initialize MAX7219 LED Matrix
  lc.shutdown(0, false);      // Wake up MAX7219
  lc.setIntensity(0, 8);      // Set brightness (0-15)
  lc.clearDisplay(0);         // Clear display

  Serial.println("System ready.");
}

float readDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000);

  if (duration == 0) return -1;

  return (duration * 0.0343) / 2.0;
}

void flashLed() {
  unsigned long currentMillis = millis();
  if (currentMillis - previousLedMillis >= LED_INTERVAL) {
    previousLedMillis = currentMillis;
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState);
  }
}

void stopLed() {
  ledState = false;
  digitalWrite(LED_PIN, LOW);
}

void playBeep() {
  unsigned long currentMillis = millis();
  unsigned long elapsed = currentMillis - previousBeepMillis;
  if (beepState) {
    if (elapsed >= BEEP_DUR) {
      beepState = false;
      digitalWrite(BUZZER_PIN, LOW);
      previousBeepMillis = currentMillis;
    }
  } else {
    if (elapsed >= BEEP_GAP) {
      beepState = true;
      digitalWrite(BUZZER_PIN, HIGH);
      previousBeepMillis = currentMillis;
    }
  }
}

void stopBeep() {
  beepState = false;
  digitalWrite(BUZZER_PIN, LOW);
  previousBeepMillis = millis();
}

// Display a single digit on the 8x8 matrix (minimal, clear patterns)
void displayNumber(int num) {
  lc.clearDisplay(0);
  
  // Very simple 5x5 digit patterns to avoid lighting up entire matrix
  // Each represents rows 1-7 (row 0 and 7 are blank borders)
  switch(num) {
    case 0:
      lc.setRow(0, 1, 0b00111100);
      lc.setRow(0, 2, 0b01000010);
      lc.setRow(0, 3, 0b01000010);
      lc.setRow(0, 4, 0b01000010);
      lc.setRow(0, 5, 0b01000010);
      lc.setRow(0, 6, 0b00111100);
      break;
    case 1:
      lc.setRow(0, 1, 0b00010000);
      lc.setRow(0, 2, 0b00110000);
      lc.setRow(0, 3, 0b00010000);
      lc.setRow(0, 4, 0b00010000);
      lc.setRow(0, 5, 0b00010000);
      lc.setRow(0, 6, 0b00111000);
      break;
    case 2:
      lc.setRow(0, 1, 0b00111100);
      lc.setRow(0, 2, 0b01000010);
      lc.setRow(0, 3, 0b00000010);
      lc.setRow(0, 4, 0b00001100);
      lc.setRow(0, 5, 0b01000000);
      lc.setRow(0, 6, 0b01111110);
      break;
    case 3:
      lc.setRow(0, 1, 0b00111100);
      lc.setRow(0, 2, 0b01000010);
      lc.setRow(0, 3, 0b00000010);
      lc.setRow(0, 4, 0b00011100);
      lc.setRow(0, 5, 0b01000010);
      lc.setRow(0, 6, 0b00111100);
      break;
    case 4:
      lc.setRow(0, 1, 0b00001000);
      lc.setRow(0, 2, 0b00011000);
      lc.setRow(0, 3, 0b00101000);
      lc.setRow(0, 4, 0b01001000);
      lc.setRow(0, 5, 0b01111110);
      lc.setRow(0, 6, 0b00001000);
      break;
    case 5:
      lc.setRow(0, 1, 0b01111110);
      lc.setRow(0, 2, 0b01000000);
      lc.setRow(0, 3, 0b01111100);
      lc.setRow(0, 4, 0b00000010);
      lc.setRow(0, 5, 0b01000010);
      lc.setRow(0, 6, 0b00111100);
      break;
    case 6:
      lc.setRow(0, 1, 0b00111100);
      lc.setRow(0, 2, 0b01000010);
      lc.setRow(0, 3, 0b01000000);
      lc.setRow(0, 4, 0b01111100);
      lc.setRow(0, 5, 0b01000010);
      lc.setRow(0, 6, 0b00111100);
      break;
    case 7:
      lc.setRow(0, 1, 0b01111110);
      lc.setRow(0, 2, 0b00000010);
      lc.setRow(0, 3, 0b00000100);
      lc.setRow(0, 4, 0b00001000);
      lc.setRow(0, 5, 0b00010000);
      lc.setRow(0, 6, 0b00010000);
      break;
    case 8:
      lc.setRow(0, 1, 0b00111100);
      lc.setRow(0, 2, 0b01000010);
      lc.setRow(0, 3, 0b00111100);
      lc.setRow(0, 4, 0b01000010);
      lc.setRow(0, 5, 0b01000010);
      lc.setRow(0, 6, 0b00111100);
      break;
    case 9:
      lc.setRow(0, 1, 0b00111100);
      lc.setRow(0, 2, 0b01000010);
      lc.setRow(0, 3, 0b01000010);
      lc.setRow(0, 4, 0b00111110);
      lc.setRow(0, 5, 0b00000010);
      lc.setRow(0, 6, 0b00111100);
      break;
  }
}

// Display "NiR" (Not in Range) text
void displayNiR() {
  lc.clearDisplay(0);
  
  // N pattern
  const byte N[8] = {0b01100110, 0b01100110, 0b01111110, 0b01111110, 0b01101110, 0b01100110, 0b01100110, 0b00000000};
  // i pattern
  const byte i[8] = {0b00011000, 0b00000000, 0b00011000, 0b00011000, 0b00011000, 0b00011000, 0b00111100, 0b00000000};
  // R pattern
  const byte R[8] = {0b01111100, 0b01100110, 0b01100110, 0b01111100, 0b01101100, 0b01100110, 0b01100110, 0b00000000};
  
  // Scroll through or display across rows
  // For simplicity, display "NiR" character by character vertically
  for (int row = 0; row < 8; row++) {
    lc.setRow(0, row, N[row]);
  }
}

void loop() {
  float distance = readDistance();

  Serial.print("Distance: ");
  Serial.println(distance);

  if (distance > 0 && distance <= ALERT_DISTANCE) {
    // Object detected — halt servo, flash LED, beep, display distance
    myServo.write(currentPos); // Keep writing current position to hold it firm
    flashLed();
    playBeep();
    
    int distanceInt = (int)distance;
    if (distanceInt >= 0 && distanceInt <= 9) {
      displayNumber(distanceInt);
    } else {
      displayNumber(9); // Show 9 if distance > 9
    }
    
    Serial.print("ALERT! Object at: ");
    Serial.print(distance);
    Serial.println(" cm");
  } else {
    // No object — stop LED, stop beep, sweep servo, display NiR
    stopLed();
    stopBeep();
    displayNiR();

    unsigned long now = millis();
    if (now - lastServoStep >= SWEEP_DELAY) {
      lastServoStep = now;

      currentPos += sweepDirection * SWEEP_STEP;

      if (currentPos >= CENTER_POS + SWEEP_ANGLE) {
        currentPos = CENTER_POS + SWEEP_ANGLE;
        sweepDirection = -1;
      } else if (currentPos <= CENTER_POS - SWEEP_ANGLE) {
        currentPos = CENTER_POS - SWEEP_ANGLE;
        sweepDirection = 1;
      }

      myServo.write(currentPos);
    }
  }
}