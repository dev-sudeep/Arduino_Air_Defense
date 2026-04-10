#include <Arduino.h>
#include <Servo.h>

Servo myServo;
Servo myServo2;

// Pin definitions
const int SERVO1_PIN  = 9;
const int SERVO2_PIN  = 10;
const int TRIG_PIN    = 7;
const int ECHO_PIN    = 6;
const int LED_PIN     = 13;
const int BUZZER_PIN  = 5;

// ---------------- Servo1 (radar sweep) ----------------
const int CENTER_POS  = 90;
const int SWEEP_ANGLE = 45;
const int SWEEP_MIN   = CENTER_POS - SWEEP_ANGLE; // 45
const int SWEEP_MAX   = CENTER_POS + SWEEP_ANGLE; // 135
const unsigned long SERVO1_BASE_STEP_MS = 14;

// ---------------- Servo2 (sudden launch) ----------------
const int SERVO2_REST_POS         = 50;
const int SERVO2_TRIGGER_POS      = 170;
const unsigned long SERVO2_HOLD_MS = 350; // hold at trigger before snapping back

// ---------------- Detection thresholds ----------------
const float ALERT_DISTANCE        = 30.0;
const float MIN_VALID_DISTANCE    = 2.0;
const float MAX_DISPLAY_DISTANCE  = 30.0; // match Processing

// LED flash timing
const unsigned long LED_INTERVAL_MS = 125;

// Buzzer timing
const unsigned long BEEP_DUR_MS = 200;
const unsigned long BEEP_GAP_MS = 50;

// Debounce detection
const int DETECTION_THRESHOLD = 3;
int detectionCount = 0;

// Re-arm behavior
const unsigned long ALERT_CLEAR_REARM_MS = 300;
unsigned long lastAlertMillis = 0;

// Missile launched flag (HIGH for 500ms after servo2 returns)
bool missileLaunchedFlag = false;
unsigned long missileLaunchedStartMs = 0;
const unsigned long MISSILE_FLAG_DURATION_MS = 500;

// Runtime state
unsigned long previousLedMillis  = 0;
unsigned long previousBeepMillis = 0;
bool ledState  = false;
bool beepState = false;

// Servo1 runtime
int servo1Pos = CENTER_POS;
int servo1Direction = 1;
unsigned long lastServo1Update = 0;

// Servo2 sudden state
bool servo2Triggered = false;       // currently in triggered/hold window
bool servo2HasFired = false;        // one-shot per alert cycle
unsigned long servo2TriggerStartMs = 0;

float readDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  if (duration == 0) return -1;

  float distance = (duration * 0.0343f) / 2.0f;
  if (distance < MIN_VALID_DISTANCE || distance > 400.0f) return -1;
  return distance;
}

void flashLed() {
  unsigned long now = millis();
  if (now - previousLedMillis >= LED_INTERVAL_MS) {
    previousLedMillis = now;
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState);
  }
}

void stopLed() {
  ledState = false;
  digitalWrite(LED_PIN, LOW);
}

void playBeep() {
  unsigned long now = millis();
  unsigned long elapsed = now - previousBeepMillis;

  if (beepState) {
    if (elapsed >= BEEP_DUR_MS) {
      beepState = false;
      digitalWrite(BUZZER_PIN, LOW);
      previousBeepMillis = now;
    }
  } else {
    if (elapsed >= BEEP_GAP_MS) {
      beepState = true;
      digitalWrite(BUZZER_PIN, HIGH);
      previousBeepMillis = now;
    }
  }
}

void stopBeep() {
  beepState = false;
  digitalWrite(BUZZER_PIN, LOW);
  previousBeepMillis = millis();
}

// Smooth sweep with easing near edges
void updateServo1Sweep() {
  unsigned long now = millis();

  float centerDist = abs(servo1Pos - CENTER_POS);          // 0..45
  float edgeFactor = centerDist / float(SWEEP_ANGLE);      // 0..1
  unsigned long dynamicStepMs = SERVO1_BASE_STEP_MS + (unsigned long)(edgeFactor * 10.0f);

  if (now - lastServo1Update < dynamicStepMs) return;
  lastServo1Update = now;

  int step = (edgeFactor < 0.55f) ? 2 : 1;
  servo1Pos += servo1Direction * step;

  if (servo1Pos >= SWEEP_MAX) {
    servo1Pos = SWEEP_MAX;
    servo1Direction = -1;
  } else if (servo1Pos <= SWEEP_MIN) {
    servo1Pos = SWEEP_MIN;
    servo1Direction = 1;
  }

  myServo.write(servo1Pos);
}

// Instant jump to trigger position
void startServo2SuddenIfNeeded() {
  if (!servo2HasFired && !servo2Triggered) {

    delay(250);
    servo2HasFired = true;
    servo2Triggered = true;
    servo2TriggerStartMs = millis();

    // sudden movement
    myServo2.write(SERVO2_TRIGGER_POS);
  }
}

// Hold, then instant return
void updateServo2Sudden() {
  if (servo2Triggered) {
    if (millis() - servo2TriggerStartMs >= SERVO2_HOLD_MS) {
      servo2Triggered = false;

      // sudden return
      myServo2.write(SERVO2_REST_POS);

      // pulse missile flag
      missileLaunchedFlag = true;
      missileLaunchedStartMs = millis();
    }
  }
}

void updateMissileFlag() {
  if (missileLaunchedFlag && (millis() - missileLaunchedStartMs >= MISSILE_FLAG_DURATION_MS)) {
    missileLaunchedFlag = false;
  }
}

void rearmServo2IfAlertCleared() {
  if (servo2HasFired && (millis() - lastAlertMillis >= ALERT_CLEAR_REARM_MS) && !servo2Triggered) {
    servo2HasFired = false;
  }
}

void setup() {
  Serial.begin(9600);

  myServo.attach(SERVO1_PIN);
  myServo2.attach(SERVO2_PIN);

  myServo.write(CENTER_POS);
  myServo2.write(SERVO2_REST_POS);

  servo1Pos = CENTER_POS;

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  delay(500);
}

void loop() {
  float distance = readDistance();

  if (distance > 0 && distance <= ALERT_DISTANCE) {
    detectionCount++;
  } else {
    detectionCount = 0;
  }

  bool alertActive = (detectionCount >= DETECTION_THRESHOLD);

  if (alertActive) {
    lastAlertMillis = millis();

    // Hold Servo1 while alerting
    myServo.write(servo1Pos);

    flashLed();
    playBeep();

    // sudden Servo2 launch
    startServo2SuddenIfNeeded();
  } else {
    stopLed();
    stopBeep();

    updateServo1Sweep();
    rearmServo2IfAlertCleared();
  }

  updateServo2Sudden();
  updateMissileFlag();

  // Telemetry: angle,distance,alert,missileLaunched
  int radarAngle = map(servo1Pos, SWEEP_MIN, SWEEP_MAX, 0, 180);

  float outDistance = distance;
  if (outDistance < 0) outDistance = MAX_DISPLAY_DISTANCE;

  Serial.print(radarAngle);
  Serial.print(",");
  Serial.print(outDistance, 1);
  Serial.print(",");
  Serial.print(alertActive ? 1 : 0);
  Serial.print(",");
  Serial.println(missileLaunchedFlag ? 1 : 0);

  delay(20);
}