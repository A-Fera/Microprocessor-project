#include <Servo.h>

// ---- Servos ----
Servo frontServo;
Servo rearServo;

// ---- Ultrasonic Pins ----
int frontTrig = 7;
int frontEcho = 8;
int rearTrig  = 12;
int rearEcho  = 13;

// ---- Motor Pins ----
int IN1 = 2;
int IN2 = 3;
int IN3 = 4;
int IN4 = 5;

// ---- Variables ----
char command;
bool goingForward  = false;
bool goingBackward = false;
int motorSpeed     = 180;

// ---- Tuned Parameters ----
int safeDistance    = 45; // Stop if object closer than this
int clearDistance   = 60; // Path is clear if distance more than this
int turnTime        = 400; // ms for each turn step
int scanDelay       = 350; // ms for servo to settle

// ---- Get Stable Distance (average of 5 readings) ----
int getDistance(int trigPin, int echoPin) {
  int readings[5];
  int total = 0;
  int valid = 0;

  for (int i = 0; i < 5; i++) {
    digitalWrite(trigPin, LOW);
    delayMicroseconds(2);
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(trigPin, LOW);

    long duration = pulseIn(echoPin, HIGH, 25000);
    int dist = duration * 0.034 / 2;

    // Only count valid readings
    if (dist > 2 && dist < 400) {
      readings[valid] = dist;
      total += dist;
      valid++;
    }
    delay(10);
  }

  // If no valid readings return 999 (clear)
  if (valid == 0) return 999;

  // Return average
  return total / valid;
}

// ---- Motor Functions ----
void forward() {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

void backward() {
  digitalWrite(IN1, LOW);
  analogWrite(IN2, motorSpeed);
  digitalWrite(IN3, LOW);
  analogWrite(IN4, motorSpeed);
}

void turnRight() {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  analogWrite(IN4, motorSpeed);
}

void turnLeft() {
  digitalWrite(IN1, LOW);
  analogWrite(IN2, motorSpeed);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

void stopMotors() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}

// ---- Scan All Directions and Find Best Path ----
int scanAllDirections(Servo s, int trig, int echo) {
  // Scan 5 positions and store distances
  int angles[5]    = {30, 60, 90, 120, 150};
  int distances[5] = {0,  0,  0,  0,   0};

  Serial.println("Scanning all directions...");

  for (int i = 0; i < 5; i++) {
    s.write(angles[i]);
    delay(scanDelay);
    distances[i] = getDistance(trig, echo);
    Serial.print("Angle ");
    Serial.print(angles[i]);
    Serial.print(" → ");
    Serial.print(distances[i]);
    Serial.println(" cm");
  }

  // Find angle with maximum distance
  int bestIndex    = 0;
  int bestDistance = 0;

  for (int i = 0; i < 5; i++) {
    if (distances[i] > bestDistance) {
      bestDistance = distances[i];
      bestIndex    = i;
    }
  }

  // Return servo to center
  s.write(90);
  delay(300);

  Serial.print("Best direction: ");
  Serial.print(angles[bestIndex]);
  Serial.print(" degrees | Distance: ");
  Serial.println(bestDistance);

  return angles[bestIndex];
}

// ---- Execute Turn Based on Best Angle ----
void executeTurn(int bestAngle) {
  if (bestAngle < 75) {
    // Turn right
    Serial.println("Executing: Turn RIGHT");
    turnRight();
    delay(turnTime);
    stopMotors();
    delay(200);
  }
  else if (bestAngle > 105) {
    // Turn left
    Serial.println("Executing: Turn LEFT");
    turnLeft();
    delay(turnTime);
    stopMotors();
    delay(200);
  }
  else {
    // Path is clear ahead — no turn needed
    Serial.println("Path clear ahead — continuing");
  }
}

// ---- Avoid Obstacle Forward ----
void avoidObstacleForward() {
  Serial.println("=== OBSTACLE DETECTED FRONT ===");
  stopMotors();
  delay(400);

  // Keep scanning until clear path found
  int attempts = 0;
  while (attempts < 5) {
    int bestAngle = scanAllDirections(frontServo, frontTrig, frontEcho);
    executeTurn(bestAngle);
    delay(300);

    // Check if path ahead is now clear
    frontServo.write(90);
    delay(300);
    int checkDist = getDistance(frontTrig, frontEcho);
    Serial.print("Path check: ");
    Serial.println(checkDist);

    if (checkDist > clearDistance) {
      Serial.println("Path is CLEAR! Moving forward.");
      forward();
      return;
    }

    attempts++;
    Serial.print("Attempt ");
    Serial.print(attempts);
    Serial.println(" — path still blocked, scanning again");
    delay(300);
  }

  // If still stuck after 5 attempts go backward a little
  Serial.println("Still stuck — backing up");
  backward();
  delay(600);
  stopMotors();
  delay(300);
  forward();
}

// ---- Avoid Obstacle Backward ----
void avoidObstacleBackward() {
  Serial.println("=== OBSTACLE DETECTED REAR ===");
  stopMotors();
  delay(400);

  int attempts = 0;
  while (attempts < 5) {
    int bestAngle = scanAllDirections(rearServo, rearTrig, rearEcho);
    executeTurn(bestAngle);
    delay(300);

    rearServo.write(90);
    delay(300);
    int checkDist = getDistance(rearTrig, rearEcho);
    Serial.print("Rear path check: ");
    Serial.println(checkDist);

    if (checkDist > clearDistance) {
      Serial.println("Rear path CLEAR! Moving backward.");
      backward();
      return;
    }

    attempts++;
    delay(300);
  }

  // If still stuck go forward a little
  Serial.println("Still stuck — moving forward");
  forward();
  delay(600);
  stopMotors();
  delay(300);
  backward();
}

// ---- Setup ----
void setup() {
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  pinMode(frontTrig, OUTPUT);
  pinMode(frontEcho, INPUT);
  pinMode(rearTrig,  OUTPUT);
  pinMode(rearEcho,  INPUT);

  frontServo.attach(6);
  rearServo.attach(9);

  // Center both servos
  frontServo.write(90);
  rearServo.write(90);

  Serial.begin(9600);
  stopMotors();

  Serial.println("=== System Ready ===");
  delay(1000);
}

// ---- Main Loop ----
void loop() {

  // Read Bluetooth command
  if (Serial.available()) {
    command = Serial.read();
    Serial.print("Command received: ");
    Serial.println(command);

    // Movement
    if (command == 'F' || command == 'f') {
      goingForward  = true;
      goingBackward = false;
      forward();
      Serial.println("Moving FORWARD");
    }
    else if (command == 'B' || command == 'b') {
      goingBackward = true;
      goingForward  = false;
      backward();
      Serial.println("Moving BACKWARD");
    }
    else if (command == 'L' || command == 'l') {
      goingForward  = false;
      goingBackward = false;
      turnLeft();
      delay(500);
      stopMotors();
      Serial.println("Turned LEFT");
    }
    else if (command == 'R' || command == 'r') {
      goingForward  = false;
      goingBackward = false;
      turnRight();
      delay(500);
      stopMotors();
      Serial.println("Turned RIGHT");
    }
    else if (command == 'S' || command == 's') {
      goingForward  = false;
      goingBackward = false;
      stopMotors();
      Serial.println("STOPPED");
    }

    // Speed control
    else if (command == '1') { motorSpeed = 80;  Serial.println("Speed: Very Slow"); }
    else if (command == '2') { motorSpeed = 120; Serial.println("Speed: Slow"); }
    else if (command == '3') { motorSpeed = 180; Serial.println("Speed: Medium"); }
    else if (command == '4') { motorSpeed = 220; Serial.println("Speed: Fast"); }
    else if (command == '5') { motorSpeed = 255; Serial.println("Speed: Full"); }
  }

  // Front obstacle check
  if (goingForward) {
    int frontDist = getDistance(frontTrig, frontEcho);
    Serial.print("Front: ");
    Serial.println(frontDist);

    if (frontDist < safeDistance && frontDist > 0) {
      avoidObstacleForward();
    }
  }

  // Rear obstacle check
  if (goingBackward) {
    int rearDist = getDistance(rearTrig, rearEcho);
    Serial.print("Rear: ");
    Serial.println(rearDist);

    if (rearDist < safeDistance && rearDist > 0) {
      avoidObstacleBackward();
    }
  }
}