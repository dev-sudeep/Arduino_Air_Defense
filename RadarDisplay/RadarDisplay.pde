// =============================================================================
//  Air Defense Radar Display  —  Processing 4
// =============================================================================
//
//  Reads structured serial data emitted by Arduino_Air_Defense.ino and draws
//  a real-time radar screen showing the sweeping servo position and any
//  detected objects.
//
//  SETUP
//  -----
//  1. Upload Arduino_Air_Defense.ino to your Arduino (baud rate 9600).
//  2. Open this sketch in Processing 4.
//  3. Run the sketch once — the console will list available serial ports.
//  4. Set PORT_INDEX below to the index that matches your Arduino port.
//     On Windows it is usually "COM3" / "COM4", on Linux "/dev/ttyACM0",
//     on macOS "/dev/cu.usbmodem…".
//  5. Run the sketch again.
//
//  CONTROLS
//  --------
//  C / c  — clear all detection blips
//  +  /  -  — increase / decrease max display range (step 50 cm)
//
// =============================================================================

import processing.serial.*;

// ── Serial configuration ─────────────────────────────────────────────────────
// Change PORT_INDEX to match your Arduino (see console output on first run).
static final int   PORT_INDEX = 0;
static final int   BAUD_RATE  = 9600;

// ── Radar geometry ────────────────────────────────────────────────────────────
// Half-circle radar drawn from bottom-centre of the window.
int   cx, cy;           // centre of the radar arc (bottom-centre of window)
int   r;                // radius in pixels

// ── Radar parameters ─────────────────────────────────────────────────────────
float maxDisplayDist = 200.0;   // cm shown at the rim  (adjustable with +/-)
final float ALERT_DIST   = 30.0;   // cm — alert threshold (amber ring)
// Servo sweeps from 45° to 135° (±45° around 90° centre)
final int   SWEEP_MIN    = 45;
final int   SWEEP_MAX    = 135;

// ── Sweep trail ───────────────────────────────────────────────────────────────
final int   TRAIL_LEN    = 40;   // number of historical positions in trail
float[]     trailAngles  = new float[TRAIL_LEN];

// ── Detection memory ──────────────────────────────────────────────────────────
final float FADE_MS      = 6000; // ms before a blip fully fades out
final int   MAX_BLIPS    = 300;  // hard cap on stored blips
final float RANGE_STEP   = 50;   // cm added/removed per +/- key press
ArrayList<Blip> blips    = new ArrayList<Blip>();

// ── Live state ────────────────────────────────────────────────────────────────
float   currentAngle     = 90;
float   currentDist      = -1;
boolean alertActive      = false;

// ── Serial ────────────────────────────────────────────────────────────────────
Serial  port;
boolean portOpen         = false;

// =============================================================================
//  Setup
// =============================================================================
void setup() {
  size(900, 600);
  colorMode(RGB, 255);

  // Radar centre at bottom-centre with a small margin
  cx = width / 2;
  cy = height - 28;
  // Radius: fill screen width or height, whichever is tighter
  r  = min(width / 2 - 20, height - 80);

  // Initialise trail at centre angle
  for (int i = 0; i < TRAIL_LEN; i++) trailAngles[i] = 90;

  // List serial ports and attempt to open selected one
  String[] ports = Serial.list();
  println("=== Available serial ports ===");
  for (int i = 0; i < ports.length; i++) println(i + "  " + ports[i]);
  println("================================");
  println("Set PORT_INDEX in the sketch to select your Arduino port.");

  if (ports.length == 0) {
    println("No serial ports found. Running without hardware.");
  } else if (PORT_INDEX < ports.length) {
    try {
      port     = new Serial(this, ports[PORT_INDEX], BAUD_RATE);
      port.bufferUntil('\n');
      portOpen = true;
      println("Opened port: " + ports[PORT_INDEX]);
    } catch (Exception e) {
      println("Could not open port[" + PORT_INDEX + "]: " + e.getMessage());
    }
  } else {
    println("PORT_INDEX " + PORT_INDEX + " is out of range. "
          + "Change PORT_INDEX to a valid value and re-run.");
  }
}

// =============================================================================
//  Main draw loop
// =============================================================================
void draw() {
  background(0, 4, 0);   // near-black green tint

  // Expire old blips
  for (int i = blips.size() - 1; i >= 0; i--) {
    if (blips.get(i).expired()) blips.remove(i);
  }

  drawGrid();
  drawTrail();
  drawSweepLine();
  drawBlips();
  drawHUD();
}

// =============================================================================
//  Radar grid
// =============================================================================
void drawGrid() {
  noFill();

  // Concentric distance rings
  float[] rings = {50, 100, 150, 200, 250, 300, 350, 400};
  for (float d : rings) {
    if (d > maxDisplayDist) break;
    float px = distToPx(d);
    stroke(0, 70, 0);
    strokeWeight(1);
    arc(cx, cy, px * 2, px * 2, PI, TWO_PI);

    // Range label just inside the arc on the right side
    fill(0, 130, 0);
    noStroke();
    textSize(10);
    textAlign(LEFT, CENTER);
    text((int)d + "cm", cx + px + 3, cy - 6);
    noFill();
    stroke(0, 70, 0);
  }

  // Alert-distance ring (amber)
  float alertPx = distToPx(ALERT_DIST);
  stroke(200, 100, 0);
  strokeWeight(1.5);
  arc(cx, cy, alertPx * 2, alertPx * 2, PI, TWO_PI);
  fill(200, 100, 0);
  noStroke();
  textSize(10);
  textAlign(LEFT, CENTER);
  text((int)ALERT_DIST + "cm!", cx + alertPx + 3, cy - 6);

  // Angle spokes every 15°  (0° = far left, 90° = straight up, 180° = far right)
  for (int deg = 0; deg <= 180; deg += 15) {
    float sa = servoToScreen(deg);
    float ex = cx + cos(sa) * r;
    float ey = cy - sin(sa) * r;

    noFill();
    stroke(0, 55, 0);
    strokeWeight(0.8);
    line(cx, cy, ex, ey);

    // Angle label at rim
    fill(0, 110, 0);
    noStroke();
    textSize(10);
    textAlign(CENTER, CENTER);
    float lx = cx + cos(sa) * (r + 16);
    float ly = cy - sin(sa) * (r + 16);
    text(deg + "°", lx, ly);
  }

  // Sweep-range boundary lines (servo 45° and 135°)
  noFill();
  stroke(0, 120, 0);
  strokeWeight(1.2);
  for (int bound : new int[]{SWEEP_MIN, SWEEP_MAX}) {
    float sa = servoToScreen(bound);
    line(cx, cy, cx + cos(sa) * r, cy - sin(sa) * r);
  }

  // Baseline
  stroke(0, 140, 0);
  strokeWeight(1);
  line(cx - r, cy, cx + r, cy);

  // Centre dot
  fill(0, 255, 0);
  noStroke();
  ellipse(cx, cy, 6, 6);
}

// =============================================================================
//  Fading sweep trail
// =============================================================================
void drawTrail() {
  for (int i = TRAIL_LEN - 1; i >= 0; i--) {
    float alpha = map(i, TRAIL_LEN - 1, 0, 4, 90);
    float sa    = servoToScreen(trailAngles[i]);
    float ex    = cx + cos(sa) * r;
    float ey    = cy - sin(sa) * r;
    stroke(0, (int)alpha, 0, (int)alpha);
    strokeWeight(1.5);
    line(cx, cy, ex, ey);
  }
}

// =============================================================================
//  Active sweep line
// =============================================================================
void drawSweepLine() {
  float sa = servoToScreen(currentAngle);
  float ex = cx + cos(sa) * r;
  float ey = cy - sin(sa) * r;

  // Outer glow
  strokeWeight(4);
  stroke(0, 255, 0, 50);
  line(cx, cy, ex, ey);

  // Core line
  strokeWeight(1.5);
  stroke(0, 255, 0, 210);
  line(cx, cy, ex, ey);

  // Tip dot
  noStroke();
  fill(0, 255, 0);
  ellipse(ex, ey, 5, 5);
}

// =============================================================================
//  Detection blips
// =============================================================================
void drawBlips() {
  for (Blip b : blips) {
    float a = b.alpha();
    if (a <= 0) continue;

    float sa = servoToScreen(b.angle);
    float px = distToPx(b.dist);
    float x  = cx + cos(sa) * px;
    float y  = cy - sin(sa) * px;

    // Outer glow
    noStroke();
    fill(0, 255, 0, a * 0.25);
    ellipse(x, y, 18, 18);

    // Blip dot
    fill(0, 255, 0, a);
    ellipse(x, y, 8, 8);

    // Distance label (visible while fresh)
    if (a > 160) {
      fill(0, 255, 0, a);
      textSize(10);
      textAlign(LEFT, CENTER);
      text(nf(b.dist, 0, 1) + "cm", x + 8, y);
    }
  }
}

// =============================================================================
//  HUD / status panel
// =============================================================================
void drawHUD() {
  // Semi-transparent panel background
  noStroke();
  fill(0, 18, 0, 210);
  rect(0, 0, 210, 130, 4);

  // Panel text
  fill(0, 200, 0);
  textSize(12);
  textAlign(LEFT, TOP);
  text("Angle :  " + nf(currentAngle, 3, 0) + " \u00b0",             10, 12);
  text("Dist  :  " + (currentDist < 0 ? "--- " : nf(currentDist, 3, 1)) + " cm", 10, 30);
  text("Blips :  " + blips.size(),                                    10, 48);
  text("Range :  " + (int)maxDisplayDist + " cm  (+/- to adjust)",    10, 66);

  // Alert indicator
  if (alertActive) {
    float flash = (sin(millis() * 0.012) > 0) ? 255 : 160;
    fill(255, 60, 0, flash);
    textSize(14);
    text("!!! OBJECT DETECTED !!!",  10, 90);
  }

  // Port status (bottom-left)
  fill(0, 80, 0);
  textSize(9);
  textAlign(LEFT, BOTTOM);
  text(portOpen ? "Serial OK  9600 baud" : "No serial connection", 6, height - 4);

  // Title (top-centre)
  fill(0, 220, 0);
  textSize(17);
  textAlign(CENTER, TOP);
  text("AIR DEFENSE RADAR", width / 2, 8);
}

// =============================================================================
//  Serial event — called automatically when a '\n'-terminated line arrives
// =============================================================================
void serialEvent(Serial p) {
  String line = p.readStringUntil('\n');
  if (line == null) return;
  line = trim(line);

  // Only process lines that start with the radar tag
  if (!line.startsWith("RADAR:")) return;

  String payload = line.substring(6);       // strip "RADAR:"
  String[] parts = split(payload, ',');
  if (parts.length != 2) return;

  try {
    float angle = float(parts[0]);
    float dist  = float(parts[1]);

    currentAngle = angle;
    currentDist  = dist;
    alertActive  = (dist > 0 && dist <= ALERT_DIST);

    // Shift trail
    for (int i = TRAIL_LEN - 1; i > 0; i--) trailAngles[i] = trailAngles[i - 1];
    trailAngles[0] = angle;

    // Record blip if a valid distance was returned
    if (dist > 0 && dist <= maxDisplayDist) {
      blips.add(new Blip(angle, dist));
      if (blips.size() > MAX_BLIPS) blips.remove(0);
    }
  } catch (Exception e) {
    // Silently skip malformed lines
  }
}

// =============================================================================
//  Keyboard controls
// =============================================================================
void keyPressed() {
  if (key == 'c' || key == 'C') {
    blips.clear();
    println("Blips cleared.");
  } else if (key == '+' || key == '=') {
    maxDisplayDist = min(maxDisplayDist + RANGE_STEP, 400);
    println("Max display range: " + maxDisplayDist + " cm");
  } else if (key == '-' || key == '_') {
    maxDisplayDist = max(maxDisplayDist - RANGE_STEP, 50);
    println("Max display range: " + maxDisplayDist + " cm");
  }
}

// =============================================================================
//  Coordinate helpers
// =============================================================================

// Map a servo angle (0-180) to a Processing screen angle (radians).
// Servo 0  → left  side (PI from +x axis)
// Servo 90 → straight up (PI/2)
// Servo 180 → right side (0)
float servoToScreen(float servoAngle) {
  return PI - radians(servoAngle);
}

// Map a distance in cm to pixels along the radar radius.
float distToPx(float distCm) {
  return map(distCm, 0, maxDisplayDist, 0, r);
}

// =============================================================================
//  Blip class
// =============================================================================
class Blip {
  float angle, dist, ts;

  Blip(float a, float d) {
    angle = a;
    dist  = d;
    ts    = millis();
  }

  boolean expired() { return millis() - ts > FADE_MS; }

  float alpha() {
    return constrain(map(millis() - ts, 0, FADE_MS, 255, 0), 0, 255);
  }
}
