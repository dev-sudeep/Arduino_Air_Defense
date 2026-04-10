import processing.serial.*;
import java.util.ArrayList;

Serial port;

float angleDeg = 0;
float distanceCm = 30;
int alertState = 0;
int missileLaunched = 0;

// Radar range now 30 cm
float maxDistanceCm = 30.0;

float cx, cy, radarR;

class Blip {
  float a, d, life;
  Blip(float a, float d) {
    this.a = a;
    this.d = d;
    this.life = 255;
  }
}
ArrayList<Blip> blips = new ArrayList<Blip>();

void setup() {
  size(960, 600);
  smooth(8);
  textFont(createFont("Consolas", 24));

  cx = width * 0.5;
  cy = height * 0.82;
  radarR = min(width * 0.44, height * 0.75);

  // Updated hardcoded serial port
  port = new Serial(this, "/dev/cu.usbserial-120", 9600);
  port.clear();
  port.bufferUntil('\n');
}

void draw() {
  background(0, 35, 0);

  drawGrid();
  drawBeam((int)angleDeg);
  drawBlips();
  drawDetectedObject();
  drawSweepLine((int)angleDeg);
  drawText();
}

void serialEvent(Serial p) {
  String line = trim(p.readStringUntil('\n'));
  if (line == null || line.length() == 0) return;

  // expected: angle,distance,alert,missileLaunched
  String[] parts = split(line, ',');
  if (parts.length < 4) return;

  try {
    angleDeg = constrain(float(parts[0]), 0, 180);
    distanceCm = constrain(float(parts[1]), 0, maxDistanceCm);
    alertState = int(parts[2]);
    missileLaunched = int(parts[3]);

    if (distanceCm > 0 && distanceCm < maxDistanceCm) {
      if (alertState == 1 || random(1) < 0.25) {
        blips.add(new Blip(angleDeg, distanceCm));
      }
    }
  } catch (Exception e) {
    // ignore malformed lines
  }
}

void drawGrid() {
  stroke(0, 220, 0);
  strokeWeight(3);
  noFill();

  // Outer 30cm ring
  arc(cx, cy, radarR * 2, radarR * 2, PI, TWO_PI);
  // 15cm ring
  arc(cx, cy, radarR, radarR, PI, TWO_PI);
  // Near ring (style)
  arc(cx, cy, radarR * 0.5, radarR * 0.5, PI, TWO_PI);

  line(cx - radarR, cy, cx + radarR, cy);

  drawGuide(45);
  drawGuide(90);
  drawGuide(135);
}

void drawGuide(float aDeg) {
  float a = radians(180 - aDeg);
  float x = cx + cos(a) * radarR;
  float y = cy - sin(a) * radarR;
  stroke(0, 200, 0);
  strokeWeight(3);
  line(cx, cy, x, y);
}

void drawBeam(int aDeg) {
  for (int i = 0; i < 18; i++) {
    float aa = radians(180 - (aDeg - i));
    float alpha = map(i, 0, 17, 220, 10);
    stroke(0, 255, 0, alpha);
    strokeWeight(3);
    line(cx, cy, cx + cos(aa) * radarR, cy - sin(aa) * radarR);
  }
}

void drawSweepLine(int aDeg) {
  float a = radians(180 - aDeg);
  stroke(0, 255, 0);
  strokeWeight(4);
  line(cx, cy, cx + cos(a) * radarR, cy - sin(a) * radarR);
}

void drawBlips() {
  noStroke();
  for (int i = blips.size() - 1; i >= 0; i--) {
    Blip b = blips.get(i);

    float rr = map(b.d, 0, maxDistanceCm, 0, radarR);
    float a = radians(180 - b.a);
    float x = cx + cos(a) * rr;
    float y = cy - sin(a) * rr;

    if (alertState == 1) fill(255, 0, 0, b.life);
    else fill(255, 80, 80, b.life * 0.6);

    ellipse(x, y, 5, 5);

    b.life -= (alertState == 1) ? 6.0 : 2.2;
    if (b.life <= 0) blips.remove(i);
  }
}

void drawDetectedObject() {
  if (alertState != 1) return;
  if (distanceCm <= 0 || distanceCm > maxDistanceCm) return;

  float rr = map(distanceCm, 0, maxDistanceCm, 0, radarR);
  float a = radians(180 - angleDeg);
  float x = cx + cos(a) * rr;
  float y = cy - sin(a) * rr;

  stroke(255, 0, 0);
  strokeWeight(2);
  fill(255, 0, 0, 220);
  ellipse(x, y, 14, 14);
  noFill();
  ellipse(x, y, 22, 22);

  stroke(255, 0, 0, 170);
  line(cx, cy, x, y);
}

void drawText() {
  fill(0, 255, 0);
  textAlign(CENTER);
  textSize(42);
  text("Radar Display", width * 0.5, 70);

  textAlign(LEFT);
  textSize(40);
  text("Angle: " + nf(angleDeg, 0, 0) + " deg", 22, 92);
  text("Distance: " + nf(distanceCm, 0, 1) + " cm", 22, 140);

  textSize(34);
  text("15 cm", cx + radarR * 0.50, cy - 8);
  text("30 cm", cx + radarR + 8, cy - 8);

  // OBJECT DETECTED in white
  if (alertState == 1) {
    fill(255, 255, 255);
    textSize(34);
    text("OBJECT DETECTED", 22, 188);
  }

  // MISSILE LAUNCHED in red across width
  if (missileLaunched == 1) {
    fill(255, 0, 0);
    textAlign(CENTER);
    textSize(54);
    text("MISSILE LAUNCHED", width * 0.5, 235);
  }
}
