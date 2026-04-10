#include <Arduino.h>
#include <Servo.h>
#include <LedControl.h>

Servo myServo;
Servo myServo2;

// Pin definitions
const int SERVO1_PIN  = 9;
const int SERVO2_PIN  = 3;   // PWM pin 3 (pin 10 reserved for MAX7219 CS)
const int TRIG_PIN   = 7;
const int ECHO_PIN   = 6;
const int LED_PIN    = 13;
const int BUZZER_PIN = 5;

// MAX7219 LED Matrix pins
const int DIN_PIN = 11;
const int CLK_PIN = 12;
const int CS_PIN  = 10;

// LedControl(dataPin, clkPin, csPin, numDevices)
LedControl lc = LedControl(DIN_PIN, CLK_PIN, CS_PIN, 1);

// 3×5 pixel font for digits 0-9 (each entry is 5 rows, 3 bits wide)
// Bit pattern per row: bit2=left col, bit1=mid col, bit0=right col
const byte DIGIT_FONT[10][5] = {
  {7, 5, 5, 5, 7},  // 0
  {2, 6, 2, 2, 7},  // 1
  {7, 1, 7, 4, 7},  // 2
  {7, 1, 7, 1, 7},  // 3
  {5, 5, 7, 1, 1},  // 4
  {7, 4, 7, 1, 7},  // 5
  {7, 4, 7, 5, 7},  // 6
  {7, 1, 1, 2, 2},  // 7
  {7, 5, 7, 5, 7},  // 8
  {7, 5, 7, 1, 7}   // 9
};

// Servo settings
const int CENTER_POS  = 90;
const int SWEEP_ANGLE = 45;
const int SWEEP_STEP  = 1;
const int SWEEP_DELAY = 15;

// Servo2 settings (trigger mechanism)
const int SERVO2_REST_POS = 50;
const int SERVO2_TRIGGER_POS = 140;  // 90 degrees clockwise (reversed direction)
const int SERVO2_TRIGGER_DURATION = 500; // Half second in milliseconds

// Distance threshold
const float ALERT_DISTANCE = 30.0;
const float MIN_VALID_DISTANCE = 2.0;  // Ignore readings below 2cm (likely noise)

// LED flash timing (4 flashes per second = 125ms on, 125ms off)
const int LED_INTERVAL = 125;

// Buzzer beep timing
const int BEEP_DUR = 200;
const int BEEP_GAP = 50;

// Debounce settings for distance readings
const int DETECTION_THRESHOLD = 3;  // Need 3 consecutive detections to trigger
int detectionCount = 0;

int currentPos = CENTER_POS;
int sweepDirection = 1;

unsigned long previousLedMillis  = 0;
unsigned long previousBeepMillis = 0;
bool ledState  = false;
bool beepState = false;

// Servo2 trigger state
bool servo2Triggered = false;
unsigned long servo2TriggerStartTime = 0;
bool servo2HasFired = false;  // Tracks if servo2 already fired this detection cycle

// Servo sweep uses a simple blocking delay since
// it only runs when no object is detected
unsigned long lastServoStep = 0;

// --- LED Matrix helpers ---

// Display a two-digit value (0-99) on the 8x8 matrix.
// Layout: 1-px margin | left digit (3 cols) | 1-px gap | right digit (3 cols)
// Digits occupy rows 1-5 (centred vertically with 1-row padding top/bottom).
void matrixDisplayNumber(int value) {
  if (value < 0)  value = 0;
  if (value > 99) value = 99;

  int tens  = value / 10;
  int units = value % 10;

  lc.clearDisplay(0);
  for (int row = 0; row < 5; row++) {
    // Left digit → bits 6,5,4 (shift left by 4)
    // Right digit → bits 2,1,0 (as-is)
    byte rowByte = ((DIGIT_FONT[tens][row] & 0x07) << 4)
                 |  (DIGIT_FONT[units][row] & 0x07);
    lc.setRow(0, row + 1, rowByte);  // rows 1-5
  }
}

// Show "--" dashes when no object is in range.
void matrixDisplayNoRange() {
  lc.clearDisplay(0);
  // Dash = all three columns lit in a single row.
  // byte layout: left dash in bits 6,5,4 | right dash in bits 2,1,0 → 0x77
  lc.setRow(0, 3, 0x77);  // single centred row (row 3 of 0-7)
}

void setupMatrix() {
  lc.shutdown(0, false);   // Wake up MAX7219
  lc.setIntensity(0, 8);   // Brightness 0-15 (8 = mid)
  lc.clearDisplay(0);
}

// --- end LED Matrix helpers ---

void setup() {
  Serial.begin(9600);
  myServo.attach(SERVO1_PIN);
  myServo.write(CENTER_POS);
  myServo2.attach(SERVO2_PIN);
  myServo2.write(SERVO2_REST_POS);
  delay(500);

  pinMode(TRIG_PIN,   OUTPUT);
  pinMode(ECHO_PIN,   INPUT);
  pinMode(LED_PIN,    OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  setupMatrix();

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

  float distance = (duration * 0.0343) / 2.0;
  
  // Filter out obviously invalid readings
  if (distance < MIN_VALID_DISTANCE || distance > 400) {
    return -1;  // Invalid reading
  }
  
  return distance;
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

void triggerServo2() {
  // Only trigger if it hasn't already fired in this detection cycle
  if (!servo2Triggered && !servo2HasFired) {
    // Start the trigger - move to trigger position
    servo2Triggered = true;
    servo2HasFired = true;  // Mark that it has fired
    servo2TriggerStartTime = millis();
    myServo2.write(SERVO2_TRIGGER_POS);
  }
}

void updateServo2() {
  if (servo2Triggered) {
    unsigned long elapsed = millis() - servo2TriggerStartTime;
    if (elapsed >= SERVO2_TRIGGER_DURATION) {
      // Return to rest position
      servo2Triggered = false;
      myServo2.write(SERVO2_REST_POS);
    }
  }
}

void resetServo2() {
  servo2Triggered = false;
  servo2HasFired = false;  // Reset for next detection cycle
  myServo2.write(SERVO2_REST_POS);
}

void loop() {
  float distance = readDistance();

  Serial.print("Distance: ");
  Serial.print(distance);
  Serial.print(" cm");

  // Debounce detection - need multiple consecutive valid readings
  if (distance > 0 && distance <= ALERT_DISTANCE) {
    detectionCount++;
    Serial.print(" | Detection count: ");
    Serial.println(detectionCount);
  } else {
    detectionCount = 0;
    Serial.println(" | No object");
  }

  // Emit a structured line consumed by the Processing radar display.
  // Format: RADAR:angle,distance  (distance = -1 when no object)
  Serial.print("RADAR:");
  Serial.print(currentPos);
  Serial.print(",");
  Serial.println(distance);

  // Only trigger alert if we have enough consecutive detections
  if (detectionCount >= DETECTION_THRESHOLD) {
    // Object detected — halt servo, flash LED, beep, trigger servo2
    myServo.write(currentPos); // Keep writing current position to hold it firm
    flashLed();
    playBeep();
    triggerServo2();
    matrixDisplayNumber((int)distance);  // Show distance in cm on LED matrix

    Serial.print("*** ALERT! Object at: ");
    Serial.print(distance);
    Serial.println(" cm ***");
  } else {
    // No object — stop LED, stop beep, sweep servo, reset servo2
    stopLed();
    stopBeep();
    resetServo2();
    matrixDisplayNoRange();  // Show "--" on LED matrix

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
  
  // Always update servo2 state
  updateServo2();
  
  // Small delay to stabilize sensor readings
  delay(50);
}