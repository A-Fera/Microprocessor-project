#include <Servo.h>

// ---- Servos ----
Servo frontServo;
Servo rearServo;

// ---- Ultrasonic Pins ----
const int frontTrig = 7;
const int frontEcho = 8;
const int rearTrig  = 12;
const int rearEcho  = 13;

// ---- Motor Pins ----
const int IN1 = 2;
const int IN2 = 3;
const int IN3 = 4;
const int IN4 = 5;

// ---- State ----
char command;
bool goingForward  = false;
bool goingBackward = false;
bool gentleTurning = false;
int  motorSpeed    = 180;
int  gentleFactor  = 70;

// ---- Tuned Parameters ----
const int safeDistance  = 45;  // cm — stop if closer than this
const int clearDistance = 65;  // cm — path is clear if farther than this
const int turnTime      = 420; // ms per turn step
const int scanDelay     = 350; // ms — wait after servo.write() before pinging

// ---- Distance check throttle ----
unsigned long lastDistCheck   = 0;
const int     distCheckInterval = 150; // ms between readings in main loop

// ============================================================
//  ULTRASONIC — single ping
//  Waits 65 ms after each ping so the echo fully decays before
//  the next trigger. This is the main fix for missed detections.
// ============================================================
int singlePing(int trigPin, int echoPin) {
  // Ensure trigger is low before starting
  digitalWrite(trigPin, LOW);
  delayMicroseconds(4);

  // Send 10 µs pulse
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // Wait for echo — 30 ms timeout ≈ ~510 cm (well beyond safe range)
  long duration = pulseIn(echoPin, HIGH, 30000);

  // Must wait before next ping so echo dies completely
  // At 4 m max range, sound travels 8 m total → 23 ms round-trip.
  // 65 ms gives comfortable margin for indoor reflections.
  delay(65);

  if (duration == 0) return 999;          // timeout → treat as clear
  int dist = (int)(duration * 0.034 / 2);
  if (dist < 2 || dist > 400) return 999; // out of valid range
  return dist;
}

// ============================================================
//  ULTRASONIC — stable median of 5 pings
//  Because each ping already waits 65 ms, 5 pings = ~325 ms.
//  Only call this when stopped or during avoidance scans.
// ============================================================
int getDistance(int trigPin, int echoPin) {
  int readings[5];
  int valid = 0;

  for (int i = 0; i < 5; i++) {
    int d = singlePing(trigPin, echoPin);
    if (d < 999) {
      readings[valid++] = d;
    }
  }

  if (valid == 0) return 999;

  // Bubble sort then return median
  for (int i = 0; i < valid - 1; i++) {
    for (int j = 0; j < valid - i - 1; j++) {
      if (readings[j] > readings[j + 1]) {
        int tmp       = readings[j];
        readings[j]   = readings[j + 1];
        readings[j+1] = tmp;
      }
    }
  }
  return readings[valid / 2];
}

// ============================================================
//  ULTRASONIC — fast single reading for main loop checks
//  (not 5-sample median — that would block motion too long)
// ============================================================
int quickDistance(int trigPin, int echoPin) {
  return singlePing(trigPin, echoPin);
}

// ============================================================
//  MOTOR FUNCTIONS
// ============================================================
void forward() {
  analogWrite(IN1, motorSpeed);
  digitalWrite(IN2, LOW);
  analogWrite(IN3, motorSpeed);
  digitalWrite(IN4, LOW);
}

void backward() {
  digitalWrite(IN1, LOW);
  analogWrite(IN2, motorSpeed);
  digitalWrite(IN3, LOW);
  analogWrite(IN4, motorSpeed);
}

void turnRight() {
  analogWrite(IN1, motorSpeed);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  analogWrite(IN4, motorSpeed);
}

void turnLeft() {
  digitalWrite(IN1, LOW);
  analogWrite(IN2, motorSpeed);
  analogWrite(IN3, motorSpeed);
  digitalWrite(IN4, LOW);
}

// Gentle arc: inner wheel runs at gentleFactor, outer at motorSpeed
void gentleRight() {
  analogWrite(IN1, gentleFactor);  // right wheels slow (inner)
  digitalWrite(IN2, LOW);
  analogWrite(IN3, motorSpeed);    // left wheels fast (outer)
  digitalWrite(IN4, LOW);
}

void gentleLeft() {
  analogWrite(IN1, motorSpeed);    // right wheels fast (outer)
  digitalWrite(IN2, LOW);
  analogWrite(IN3, gentleFactor);  // left wheels slow (inner)
  digitalWrite(IN4, LOW);
}

void stopMotors() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}

// ============================================================
//  SERVO — move and wait for it to physically reach the angle.
//
//  FIX: servo was being passed BY VALUE to scanAllDirections(),
//  which created a copy. The copy lost the pin attachment after
//  the function returned, causing jitter and missed steps.
//  Now every servo call goes through this helper using a
//  global reference, so the same Servo object is always used.
// ============================================================
void servoGoto(Servo &s, int angle) {
  s.write(angle);
  // Standard servo moves ~60°/sec. 120° swing = 2000 ms worst case.
  // scanDelay (350 ms) covers 90° comfortably. Add extra for heavy servos.
  delay(scanDelay);
}

// ============================================================
//  SCAN — sweep 5 angles, return the angle with max clearance.
//  Uses Servo& (reference) — no copy, no detach/reattach.
// ============================================================
int scanAllDirections(Servo &s, int trigPin, int echoPin) {
  const int angles[5]    = {30, 60, 90, 120, 150};
  int       distances[5] = {0,  0,  0,  0,   0};

  Serial.println("Scanning...");

  for (int i = 0; i < 5; i++) {
    servoGoto(s, angles[i]);
    // Take a stable 5-sample median at each angle position
    distances[i] = getDistance(trigPin, echoPin);
    Serial.print("  "); Serial.print(angles[i]);
    Serial.print(" deg -> "); Serial.print(distances[i]); Serial.println(" cm");
  }

  // Return to center
  servoGoto(s, 90);

  // Find angle with greatest clearance
  int bestIndex    = 2; // default: straight ahead
  int bestDistance = 0;
  for (int i = 0; i < 5; i++) {
    if (distances[i] > bestDistance) {
      bestDistance = distances[i];
      bestIndex    = i;
    }
  }

  Serial.print("Best: "); Serial.print(angles[bestIndex]);
  Serial.print(" deg ("); Serial.print(bestDistance); Serial.println(" cm)");
  return angles[bestIndex];
}

// ============================================================
//  EXECUTE TURN based on best angle from scan
// ============================================================
void executeTurn(int bestAngle) {
  if (bestAngle < 75) {
    Serial.println("Turn RIGHT");
    turnRight();
    delay(turnTime);
    stopMotors();
    delay(200);
  } else if (bestAngle > 105) {
    Serial.println("Turn LEFT");
    turnLeft();
    delay(turnTime);
    stopMotors();
    delay(200);
  } else {
    Serial.println("Ahead clear — no turn");
  }
}

// ============================================================
//  AVOID OBSTACLE — FRONT
//
//  FIX: after avoidance, the function returns WITHOUT calling
//  forward() or setting goingForward. The calling loop handles
//  resumption. This prevents re-entering avoidance immediately.
// ============================================================
void avoidObstacleForward() {
  Serial.println("=== OBSTACLE FRONT ===");
  goingForward = false; // pause forward-checking while we maneuver
  stopMotors();
  delay(300);

  // ---- Quick look left/right before full 5-point scan ----
  servoGoto(frontServo, 150);
  int leftDist = getDistance(frontTrig, frontEcho);
  Serial.print("Quick left: "); Serial.println(leftDist);

  servoGoto(frontServo, 30);
  int rightDist = getDistance(frontTrig, frontEcho);
  Serial.print("Quick right: "); Serial.println(rightDist);

  servoGoto(frontServo, 90);

  if (leftDist > clearDistance && leftDist > rightDist + 20) {
    Serial.println("Left clear — turning LEFT");
    turnLeft(); delay(turnTime); stopMotors(); delay(200);
    Serial.println("Resuming FORWARD");
    goingForward = true;
    forward();
    return;
  }
  if (rightDist > clearDistance && rightDist > leftDist + 20) {
    Serial.println("Right clear — turning RIGHT");
    turnRight(); delay(turnTime); stopMotors(); delay(200);
    Serial.println("Resuming FORWARD");
    goingForward = true;
    forward();
    return;
  }

  // ---- Full scan loop ----
  for (int attempt = 1; attempt <= 6; attempt++) {
    int bestAngle = scanAllDirections(frontServo, frontTrig, frontEcho);
    executeTurn(bestAngle);
    delay(300);

    // Verify path is now clear
    servoGoto(frontServo, 90);
    int checkDist = getDistance(frontTrig, frontEcho);
    Serial.print("Path check: "); Serial.println(checkDist);

    if (checkDist > clearDistance) {
      Serial.println("Path CLEAR — resuming FORWARD");
      goingForward = true;
      forward();
      return;
    }

    Serial.print("Attempt "); Serial.print(attempt); Serial.println(" — still blocked");

    // Every 2 failed attempts, back up to reposition
    if (attempt % 2 == 0) {
      Serial.println("Backing up to reposition");
      backward();
      delay(550);
      stopMotors();
      delay(300);
    }
  }

  // Last resort
  Serial.println("Stuck — longer reverse");
  backward(); delay(1000); stopMotors(); delay(400);
  int finalAngle = scanAllDirections(frontServo, frontTrig, frontEcho);
  executeTurn(finalAngle);
  delay(300);
  goingForward = true;
  forward();
}

// ============================================================
//  AVOID OBSTACLE — REAR
// ============================================================
void avoidObstacleBackward() {
  Serial.println("=== OBSTACLE REAR ===");
  goingBackward = false;
  stopMotors();
  delay(300);

  servoGoto(rearServo, 150);
  int leftDist = getDistance(rearTrig, rearEcho);
  Serial.print("Quick left: "); Serial.println(leftDist);

  servoGoto(rearServo, 30);
  int rightDist = getDistance(rearTrig, rearEcho);
  Serial.print("Quick right: "); Serial.println(rightDist);

  servoGoto(rearServo, 90);

  if (leftDist > clearDistance && leftDist > rightDist + 20) {
    Serial.println("Left clear — turning LEFT");
    turnLeft(); delay(turnTime); stopMotors(); delay(200);
    goingBackward = true;
    backward();
    return;
  }
  if (rightDist > clearDistance && rightDist > leftDist + 20) {
    Serial.println("Right clear — turning RIGHT");
    turnRight(); delay(turnTime); stopMotors(); delay(200);
    goingBackward = true;
    backward();
    return;
  }

  for (int attempt = 1; attempt <= 6; attempt++) {
    int bestAngle = scanAllDirections(rearServo, rearTrig, rearEcho);
    executeTurn(bestAngle);
    delay(300);

    servoGoto(rearServo, 90);
    int checkDist = getDistance(rearTrig, rearEcho);
    Serial.print("Rear path check: "); Serial.println(checkDist);

    if (checkDist > clearDistance) {
      Serial.println("Rear CLEAR — resuming BACKWARD");
      goingBackward = true;
      backward();
      return;
    }

    Serial.print("Attempt "); Serial.print(attempt); Serial.println(" — still blocked");

    if (attempt % 2 == 0) {
      Serial.println("Moving forward to reposition");
      forward();
      delay(550);
      stopMotors();
      delay(300);
    }
  }

  Serial.println("Stuck — longer forward push");
  forward(); delay(1000); stopMotors(); delay(400);
  int finalAngle = scanAllDirections(rearServo, rearTrig, rearEcho);
  executeTurn(finalAngle);
  delay(300);
  goingBackward = true;
  backward();
}

// ============================================================
//  SETUP
// ============================================================
void setup() {
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  pinMode(frontTrig, OUTPUT);
  pinMode(frontEcho, INPUT);
  pinMode(rearTrig,  OUTPUT);
  pinMode(rearEcho,  INPUT);

  // Attach servos and centre them before anything else
  frontServo.attach(6);
  rearServo.attach(9);

  // Let servos reach centre before any scan happens
  frontServo.write(90);
  rearServo.write(90);
  delay(800); // give servos time to physically reach centre

  stopMotors();
  Serial.begin(9600);

  Serial.println("=== System Ready ===");
  Serial.println("F/B=Forward/Backward  L/R=Turn  G/H=Gentle turn  S=Stop");
  Serial.println("1-5=Speed");
}

// ============================================================
//  MAIN LOOP
// ============================================================
void loop() {

  // ---- Read Bluetooth command ----
  if (Serial.available()) {
    command = Serial.read();
    Serial.print("CMD: "); Serial.println(command);

    // Reset all movement flags before applying new command
    goingForward  = false;
    goingBackward = false;
    gentleTurning = false;

    if      (command == 'F' || command == 'f') {
      goingForward = true;
      forward();
      Serial.println("FORWARD");
    }
    else if (command == 'B' || command == 'b') {
      goingBackward = true;
      backward();
      Serial.println("BACKWARD");
    }
    else if (command == 'L' || command == 'l') {
      turnLeft();
      delay(500);
      stopMotors();
      Serial.println("TURN LEFT");
    }
    else if (command == 'R' || command == 'r') {
      turnRight();
      delay(500);
      stopMotors();
      Serial.println("TURN RIGHT");
    }
    // G = gentle right arc (curves while moving forward)
    else if (command == 'G' || command == 'g') {
      goingForward  = true;  // still approaching objects — check front sensor
      gentleTurning = true;
      gentleRight();
      Serial.println("GENTLE RIGHT");
    }
    // H = gentle left arc
    else if (command == 'H' || command == 'h') {
      goingForward  = true;
      gentleTurning = true;
      gentleLeft();
      Serial.println("GENTLE LEFT");
    }
    else if (command == 'S' || command == 's') {
      stopMotors();
      Serial.println("STOP");
    }

    // Speed control — gentleFactor scales proportionally so arc shape stays consistent
    else if (command == '1') { motorSpeed = 80;  gentleFactor = 30;  Serial.println("Speed 1 — very slow"); }
    else if (command == '2') { motorSpeed = 120; gentleFactor = 45;  Serial.println("Speed 2 — slow"); }
    else if (command == '3') { motorSpeed = 180; gentleFactor = 70;  Serial.println("Speed 3 — medium"); }
    else if (command == '4') { motorSpeed = 220; gentleFactor = 90;  Serial.println("Speed 4 — fast"); }
    else if (command == '5') { motorSpeed = 255; gentleFactor = 110; Serial.println("Speed 5 — full"); }
  }

  // ---- Throttled obstacle checks (single ping per interval) ----
  // Using quickDistance() here — one ping every 150 ms is safe for
  // motion control without blocking the command reader.
  if (millis() - lastDistCheck >= distCheckInterval) {
    lastDistCheck = millis();

    if (goingForward) {
      int frontDist = quickDistance(frontTrig, frontEcho);
      if (frontDist != 999) { // only log real readings
        Serial.print("Front: "); Serial.println(frontDist);
      }
      if (frontDist > 0 && frontDist < safeDistance) {
        avoidObstacleForward();
        gentleTurning = false; // avoidance always ends in straight forward
      }
    }

    if (goingBackward) {
      int rearDist = quickDistance(rearTrig, rearEcho);
      if (rearDist != 999) {
        Serial.print("Rear: "); Serial.println(rearDist);
      }
      if (rearDist > 0 && rearDist < safeDistance) {
        avoidObstacleBackward();
      }
    }
  }
}
