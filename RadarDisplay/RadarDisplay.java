// =============================================================================
//  Air Defense Radar Display — Processing 4 (Java Mode)
// =============================================================================
//
//  Reads serial data from Arduino_Air_Defense.ino and renders a real-time
//  radar screen showing the sweeping servo position and detected objects.
//
//  The Arduino sketch prints one line per loop iteration (~50 ms):
//
//    Distance: 24.70 cm | No object
//    Distance: 24.70 cm | Detection count: 3
//    *** ALERT! Object at: 24.70 cm ***
//
//  Because the Arduino does not print the servo angle, this app mirrors the
//  sweep logic from the firmware (CENTER=90°, ±45°, 1°/step) and advances
//  the simulated angle each time a "No object" line is received, holding it
//  whenever a detection is reported.
//
//  SETUP
//  -----
//  1. Open Processing 4, switch to "Java" mode (top-right mode selector).
//  2. Open this file as a new sketch  (File → New, paste, or open the folder).
//  3. Run once — the console lists available serial ports.
//  4. Set PORT_INDEX to the index that matches your Arduino port and re-run.
//     Windows: usually 0 ("COM3"/"COM4")
//     Linux  : usually 0 ("/dev/ttyACM0")
//     macOS  : usually 0 ("/dev/cu.usbmodem…")
//
//  CONTROLS
//  --------
//  C / c   — clear stored detection blips
//  +  / -  — increase / decrease the displayed range (step: 50 cm)
//
// =============================================================================

import processing.core.PApplet;
import processing.serial.Serial;
import java.util.ArrayList;
import java.util.Iterator;

public class RadarDisplay extends PApplet {

  // ── Serial configuration ───────────────────────────────────────────────────
  /** Change to the index of your Arduino port shown in the console. */
  static final int PORT_INDEX = 0;
  static final int BAUD_RATE  = 9600;

  // ── Servo sweep mirror (must match Arduino firmware constants) ─────────────
  static final int CENTER_POS  = 90;
  static final int SWEEP_ANGLE = 45;   // ±degrees from centre
  static final int SWEEP_MIN   = CENTER_POS - SWEEP_ANGLE;  // 45°
  static final int SWEEP_MAX   = CENTER_POS + SWEEP_ANGLE;  // 135°

  // ── Radar geometry ─────────────────────────────────────────────────────────
  int cx, cy, r;

  // ── Display range (adjustable at runtime) ─────────────────────────────────
  float maxDisplayDist = 200.0f;  // cm shown at the rim
  static final float RANGE_STEP  = 50.0f;
  static final float ALERT_DIST  = 30.0f;  // amber ring threshold (cm)

  // ── Sweep trail ────────────────────────────────────────────────────────────
  static final int TRAIL_LEN = 40;
  float[] trailAngles = new float[TRAIL_LEN];

  // ── Detection blip storage ─────────────────────────────────────────────────
  static final float FADE_MS  = 6000.0f;
  static final int   MAX_BLIPS = 300;
  ArrayList<Blip> blips = new ArrayList<Blip>();

  // ── Live state ─────────────────────────────────────────────────────────────
  float   simAngle     = CENTER_POS;  // simulated servo angle
  int     simDirection = 1;           // +1 = increasing angle
  float   currentDist  = -1;
  boolean alertActive  = false;

  // ── Serial ─────────────────────────────────────────────────────────────────
  Serial  port;
  boolean portOpen = false;

  // ==========================================================================
  //  Entry point
  // ==========================================================================
  public static void main(String[] args) {
    PApplet.main("RadarDisplay");
  }

  // ==========================================================================
  //  Processing lifecycle
  // ==========================================================================
  @Override
  public void settings() {
    size(900, 600);
  }

  @Override
  public void setup() {
    colorMode(RGB, 255);

    cx = width  / 2;
    cy = height - 28;
    r  = min(width / 2 - 20, height - 80);

    for (int i = 0; i < TRAIL_LEN; i++) trailAngles[i] = CENTER_POS;

    // List available ports so the user can identify the correct one
    String[] ports = Serial.list();
    System.out.println("=== Available serial ports ===");
    for (int i = 0; i < ports.length; i++) System.out.println(i + "  " + ports[i]);
    System.out.println("================================");
    System.out.println("Set PORT_INDEX in RadarDisplay.java to select your Arduino port.");

    if (ports.length == 0) {
      System.out.println("No serial ports found. Running without hardware.");
    } else if (PORT_INDEX < ports.length) {
      try {
        port     = new Serial(this, ports[PORT_INDEX], BAUD_RATE);
        port.bufferUntil('\n');
        portOpen = true;
        System.out.println("Opened port: " + ports[PORT_INDEX]);
      } catch (Exception e) {
        System.out.println("Could not open port[" + PORT_INDEX + "]: " + e.getMessage());
      }
    } else {
      System.out.println("PORT_INDEX " + PORT_INDEX + " is out of range.");
    }
  }

  @Override
  public void draw() {
    background(0, 4, 0);

    // Expire old blips
    Iterator<Blip> it = blips.iterator();
    while (it.hasNext()) { if (it.next().expired()) it.remove(); }

    drawGrid();
    drawTrail();
    drawSweepLine();
    drawBlips();
    drawHUD();
  }

  // ==========================================================================
  //  Serial callback — called when a '\n'-terminated line is received
  // ==========================================================================
  public void serialEvent(Serial p) {
    String line = p.readStringUntil('\n');
    if (line == null) return;
    line = line.trim();

    if (line.startsWith("Distance:")) {
      // Format: "Distance: X.XX cm | No object"
      //      or "Distance: X.XX cm | Detection count: N"
      boolean detected = !line.contains("No object");

      // Parse distance value
      float dist = -1;
      try {
        // Text between "Distance: " and " cm"
        int start = "Distance: ".length();
        int end   = line.indexOf(" cm");
        if (end > start) dist = Float.parseFloat(line.substring(start, end).trim());
      } catch (NumberFormatException ignored) { }

      currentDist = dist;
      alertActive = detected && dist > 0 && dist <= ALERT_DIST;

      if (!detected || dist <= 0) {
        // No object — advance simulated sweep angle
        advanceSweep();
      }
      // When object detected the sweep holds (simAngle stays unchanged)

      // Record a blip if distance is valid and within display range
      if (detected && dist > 0 && dist <= maxDisplayDist) {
        blips.add(new Blip(simAngle, dist));
        if (blips.size() > MAX_BLIPS) blips.remove(0);
      }

      // Always update trail
      for (int i = TRAIL_LEN - 1; i > 0; i--) trailAngles[i] = trailAngles[i - 1];
      trailAngles[0] = simAngle;
    }
  }

  // ==========================================================================
  //  Keyboard controls
  // ==========================================================================
  @Override
  public void keyPressed() {
    if (key == 'c' || key == 'C') {
      blips.clear();
      System.out.println("Blips cleared.");
    } else if (key == '+' || key == '=') {
      maxDisplayDist = min(maxDisplayDist + RANGE_STEP, 400);
      System.out.println("Max display range: " + maxDisplayDist + " cm");
    } else if (key == '-' || key == '_') {
      maxDisplayDist = max(maxDisplayDist - RANGE_STEP, 50);
      System.out.println("Max display range: " + maxDisplayDist + " cm");
    }
  }

  // ==========================================================================
  //  Sweep simulation (mirrors Arduino firmware logic)
  // ==========================================================================
  private void advanceSweep() {
    simAngle += simDirection;
    if (simAngle >= SWEEP_MAX) {
      simAngle     = SWEEP_MAX;
      simDirection = -1;
    } else if (simAngle <= SWEEP_MIN) {
      simAngle     = SWEEP_MIN;
      simDirection = 1;
    }
  }

  // ==========================================================================
  //  Radar grid
  // ==========================================================================
  private void drawGrid() {
    noFill();

    // Concentric distance rings
    float[] rings = {50, 100, 150, 200, 250, 300, 350, 400};
    for (float d : rings) {
      if (d > maxDisplayDist) break;
      float px = distToPx(d);
      stroke(0, 70, 0);
      strokeWeight(1);
      arc(cx, cy, px * 2, px * 2, PI, TWO_PI);

      fill(0, 130, 0);
      noStroke();
      textSize(10);
      textAlign(LEFT, CENTER);
      text((int) d + "cm", cx + px + 3, cy - 6);
      noFill();
      stroke(0, 70, 0);
    }

    // Alert ring (amber)
    float alertPx = distToPx(ALERT_DIST);
    stroke(200, 100, 0);
    strokeWeight(1.5f);
    arc(cx, cy, alertPx * 2, alertPx * 2, PI, TWO_PI);
    fill(200, 100, 0);
    noStroke();
    textSize(10);
    textAlign(LEFT, CENTER);
    text((int) ALERT_DIST + "cm!", cx + alertPx + 3, cy - 6);

    // Angle spokes every 15°
    for (int deg = 0; deg <= 180; deg += 15) {
      float sa = servoToScreen(deg);
      float ex = cx + cos(sa) * r;
      float ey = cy - sin(sa) * r;

      noFill();
      stroke(0, 55, 0);
      strokeWeight(0.8f);
      line(cx, cy, ex, ey);

      fill(0, 110, 0);
      noStroke();
      textSize(10);
      textAlign(CENTER, CENTER);
      float lx = cx + cos(sa) * (r + 16);
      float ly = cy - sin(sa) * (r + 16);
      text(deg + "\u00b0", lx, ly);
    }

    // Sweep boundary lines at 45° and 135°
    noFill();
    stroke(0, 120, 0);
    strokeWeight(1.2f);
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

  // ==========================================================================
  //  Fading sweep trail
  // ==========================================================================
  private void drawTrail() {
    for (int i = TRAIL_LEN - 1; i >= 0; i--) {
      float alpha = map(i, TRAIL_LEN - 1, 0, 4, 90);
      float sa    = servoToScreen(trailAngles[i]);
      float ex    = cx + cos(sa) * r;
      float ey    = cy - sin(sa) * r;
      stroke(0, (int) alpha, 0, (int) alpha);
      strokeWeight(1.5f);
      line(cx, cy, ex, ey);
    }
  }

  // ==========================================================================
  //  Active sweep line
  // ==========================================================================
  private void drawSweepLine() {
    float sa = servoToScreen(simAngle);
    float ex = cx + cos(sa) * r;
    float ey = cy - sin(sa) * r;

    strokeWeight(4);
    stroke(0, 255, 0, 50);
    line(cx, cy, ex, ey);

    strokeWeight(1.5f);
    stroke(0, 255, 0, 210);
    line(cx, cy, ex, ey);

    noStroke();
    fill(0, 255, 0);
    ellipse(ex, ey, 5, 5);
  }

  // ==========================================================================
  //  Detection blips
  // ==========================================================================
  private void drawBlips() {
    for (Blip b : blips) {
      float a = b.alpha();
      if (a <= 0) continue;

      float sa = servoToScreen(b.angle);
      float px = distToPx(b.dist);
      float x  = cx + cos(sa) * px;
      float y  = cy - sin(sa) * px;

      noStroke();
      fill(0, 255, 0, a * 0.25f);
      ellipse(x, y, 18, 18);

      fill(0, 255, 0, a);
      ellipse(x, y, 8, 8);

      if (a > 160) {
        fill(0, 255, 0, a);
        textSize(10);
        textAlign(LEFT, CENTER);
        text(nf(b.dist, 0, 1) + "cm", x + 8, y);
      }
    }
  }

  // ==========================================================================
  //  HUD panel
  // ==========================================================================
  private void drawHUD() {
    noStroke();
    fill(0, 18, 0, 210);
    rect(0, 0, 220, 140, 4);

    fill(0, 200, 0);
    textSize(12);
    textAlign(LEFT, TOP);
    text("Angle :  " + nf(simAngle, 3, 0) + " \u00b0",                          10, 12);
    text("Dist  :  " + (currentDist < 0 ? "---" : nf(currentDist, 3, 1)) + " cm", 10, 30);
    text("Blips :  " + blips.size(),                                              10, 48);
    text("Range :  " + (int) maxDisplayDist + " cm  (+/- to adjust)",             10, 66);

    if (alertActive) {
      float flash = (sin(millis() * 0.012f) > 0) ? 255 : 160;
      fill(255, 60, 0, flash);
      textSize(14);
      text("!!! OBJECT DETECTED !!!", 10, 96);
    }

    fill(0, 80, 0);
    textSize(9);
    textAlign(LEFT, BOTTOM);
    text(portOpen ? "Serial OK  9600 baud" : "No serial connection", 6, height - 4);

    fill(0, 220, 0);
    textSize(17);
    textAlign(CENTER, TOP);
    text("AIR DEFENSE RADAR", width / 2, 8);
  }

  // ==========================================================================
  //  Coordinate helpers
  // ==========================================================================

  /** Map a servo angle (0–180) to a Processing screen angle in radians.
   *  Servo  0° → far left  (screen angle = PI)
   *  Servo 90° → straight up (screen angle = PI/2)
   *  Servo 180° → far right (screen angle = 0)
   */
  private float servoToScreen(float servoAngle) {
    return PI - radians(servoAngle);
  }

  /** Map distance in cm to pixels along the radar radius. */
  private float distToPx(float distCm) {
    return map(distCm, 0, maxDisplayDist, 0, r);
  }

  // ==========================================================================
  //  Blip inner class
  // ==========================================================================
  class Blip {
    final float angle;
    final float dist;
    final float ts;

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
}
