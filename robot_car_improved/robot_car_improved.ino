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
bool goingForward   = false;
bool goingBackward  = false;
bool gentleTurning  = false;   // NEW: tracks gentle turn mode
int  motorSpeed     = 180;

// ---- Tuned Parameters ----
int safeDistance  = 45;  // Stop if object closer than this (cm)
int clearDistance = 65;  // Path is clear if farther than this (cm)
int turnTime      = 400; // ms per turn step
int scanDelay     = 300; // ms for servo to settle

// ---- Distance check throttle ----
unsigned long lastDistCheck = 0;
int distCheckInterval = 150; // ms between distance readings in main loop

// ---- Bubble-sort helper for median ----
void sortArray(int arr[], int n) {
  for (int i = 0; i < n - 1; i++) {
    for (int j = 0; j < n - i - 1; j++) {
      if (arr[j] > arr[j + 1]) {
        int tmp  = arr[j];
        arr[j]   = arr[j + 1];
        arr[j+1] = tmp;
      }
    }
  }
}

// ---- Get Stable Distance — median of 5 readings (more noise-resistant) ----
int getDistance(int trigPin, int echoPin) {
  int readings[5];
  int valid = 0;

  for (int i = 0; i < 5; i++) {
    digitalWrite(trigPin, LOW);
    delayMicroseconds(2);
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(trigPin, LOW);

    long duration = pulseIn(echoPin, HIGH, 25000); // 25ms timeout ≈ 425cm max
    int dist = (int)(duration * 0.034 / 2);

    if (dist > 2 && dist < 400) {
      readings[valid] = dist;
      valid++;
    }
    delay(10);
  }

  if (valid == 0) return 999; // Nothing detected → treat as clear

  sortArray(readings, valid);
  return readings[valid / 2]; // Return median
}

// ---- Motor Functions ----

// Forward — uses motorSpeed so speed control applies
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

// ---- NEW: Gentle Turns ----
// One side runs at full motorSpeed, other side runs slower → curved arc
// gentleFactor controls how much slower the inner wheel is (0–255)
// Lower gentleFactor = tighter curve; higher = wider arc
int gentleFactor = 80; // inner wheel PWM

void gentleRight() {
  // Left wheels fast, right wheels slow → curves right
  analogWrite(IN1, gentleFactor);  // right wheel slow
  digitalWrite(IN2, LOW);
  analogWrite(IN3, motorSpeed);    // left wheel fast
  digitalWrite(IN4, LOW);
}

void gentleLeft() {
  // Right wheels fast, left wheels slow → curves left
  analogWrite(IN1, motorSpeed);    // right wheel fast
  digitalWrite(IN2, LOW);
  analogWrite(IN3, gentleFactor);  // left wheel slow
  digitalWrite(IN4, LOW);
}

void stopMotors() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}

// ---- Scan All Directions and Return Best Angle ----
int scanAllDirections(Servo &s, int trig, int echo) {
  int angles[5]    = {30, 60, 90, 120, 150};
  int distances[5] = {0,  0,  0,  0,   0};

  Serial.println("Scanning all directions...");

  for (int i = 0; i < 5; i++) {
    s.write(angles[i]);
    delay(scanDelay);
    distances[i] = getDistance(trig, echo);
    Serial.print("  Angle ");
    Serial.print(angles[i]);
    Serial.print(" → ");
    Serial.print(distances[i]);
    Serial.println(" cm");
  }

  // Return servo to center
  s.write(90);
  delay(300);

  // Find angle with maximum distance
  int bestIndex    = 2; // default center
  int bestDistance = 0;

  for (int i = 0; i < 5; i++) {
    if (distances[i] > bestDistance) {
      bestDistance = distances[i];
      bestIndex    = i;
    }
  }

  Serial.print("Best direction: ");
  Serial.print(angles[bestIndex]);
  Serial.print(" deg | Distance: ");
  Serial.print(bestDistance);
  Serial.println(" cm");

  return angles[bestIndex];
}

// ---- Execute Turn Based on Best Angle ----
void executeTurn(int bestAngle) {
  if (bestAngle < 75) {
    Serial.println("Executing: Turn RIGHT");
    turnRight();
    delay(turnTime);
    stopMotors();
    delay(200);
  } else if (bestAngle > 105) {
    Serial.println("Executing: Turn LEFT");
    turnLeft();
    delay(turnTime);
    stopMotors();
    delay(200);
  } else {
    Serial.println("Best path is straight ahead");
  }
}

// ---- Avoid Obstacle Forward — improved ----
void avoidObstacleForward() {
  Serial.println("=== OBSTACLE FRONT ===");
  stopMotors();
  delay(300);

  // Step 1: Quick left/right look before full scan
  int leftDist  = 0;
  int rightDist = 0;

  frontServo.write(150); delay(scanDelay);
  leftDist = getDistance(frontTrig, frontEcho);
  Serial.print("Quick left: "); Serial.println(leftDist);

  frontServo.write(30);  delay(scanDelay);
  rightDist = getDistance(frontTrig, frontEcho);
  Serial.print("Quick right: "); Serial.println(rightDist);

  frontServo.write(90);  delay(300);

  // If one side is clearly open, turn that way immediately
  if (leftDist > clearDistance && leftDist > rightDist + 20) {
    Serial.println("Quick left clear — turning LEFT");
    turnLeft();
    delay(turnTime);
    stopMotors();
    delay(200);
    forward();
    return;
  }
  if (rightDist > clearDistance && rightDist > leftDist + 20) {
    Serial.println("Quick right clear — turning RIGHT");
    turnRight();
    delay(turnTime);
    stopMotors();
    delay(200);
    forward();
    return;
  }

  // Step 2: Full scan loop
  int attempts = 0;
  while (attempts < 6) {
    int bestAngle = scanAllDirections(frontServo, frontTrig, frontEcho);
    executeTurn(bestAngle);
    delay(300);

    frontServo.write(90);
    delay(300);
    int checkDist = getDistance(frontTrig, frontEcho);
    Serial.print("Path check: "); Serial.println(checkDist);

    if (checkDist > clearDistance) {
      Serial.println("Path CLEAR — moving forward");
      forward();
      return;
    }

    attempts++;
    Serial.print("Attempt "); Serial.print(attempts); Serial.println(" — still blocked");

    // Every 2 failed attempts, back up a bit to reposition
    if (attempts % 2 == 0) {
      Serial.println("Backing up to reposition...");
      backward();
      delay(500);
      stopMotors();
      delay(300);
    }
  }

  // Last resort: reverse more and try again
  Serial.println("Stuck — longer reverse");
  backward();
  delay(900);
  stopMotors();
  delay(300);

  // One final turn attempt
  int finalAngle = scanAllDirections(frontServo, frontTrig, frontEcho);
  executeTurn(finalAngle);
  delay(300);
  forward();
}

// ---- Avoid Obstacle Backward — improved ----
void avoidObstacleBackward() {
  Serial.println("=== OBSTACLE REAR ===");
  stopMotors();
  delay(300);

  // Quick left/right look
  int leftDist  = 0;
  int rightDist = 0;

  rearServo.write(150); delay(scanDelay);
  leftDist = getDistance(rearTrig, rearEcho);
  Serial.print("Quick left: "); Serial.println(leftDist);

  rearServo.write(30);  delay(scanDelay);
  rightDist = getDistance(rearTrig, rearEcho);
  Serial.print("Quick right: "); Serial.println(rightDist);

  rearServo.write(90);  delay(300);

  if (leftDist > clearDistance && leftDist > rightDist + 20) {
    Serial.println("Quick left clear — turning LEFT");
    turnLeft();
    delay(turnTime);
    stopMotors();
    delay(200);
    backward();
    return;
  }
  if (rightDist > clearDistance && rightDist > leftDist + 20) {
    Serial.println("Quick right clear — turning RIGHT");
    turnRight();
    delay(turnTime);
    stopMotors();
    delay(200);
    backward();
    return;
  }

  int attempts = 0;
  while (attempts < 6) {
    int bestAngle = scanAllDirections(rearServo, rearTrig, rearEcho);
    executeTurn(bestAngle);
    delay(300);

    rearServo.write(90);
    delay(300);
    int checkDist = getDistance(rearTrig, rearEcho);
    Serial.print("Rear path check: "); Serial.println(checkDist);

    if (checkDist > clearDistance) {
      Serial.println("Rear CLEAR — moving backward");
      backward();
      return;
    }

    attempts++;

    if (attempts % 2 == 0) {
      Serial.println("Moving forward to reposition...");
      forward();
      delay(500);
      stopMotors();
      delay(300);
    }
  }

  Serial.println("Stuck — longer forward push");
  forward();
  delay(900);
  stopMotors();
  delay(300);

  int finalAngle = scanAllDirections(rearServo, rearTrig, rearEcho);
  executeTurn(finalAngle);
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

  frontServo.write(90);
  rearServo.write(90);

  Serial.begin(9600);
  stopMotors();

  Serial.println("=== System Ready ===");
  Serial.println("Commands: F=Forward B=Backward L=Left R=Right");
  Serial.println("          G=Gentle Right H=Gentle Left S=Stop");
  Serial.println("          1-5=Speed");
  delay(1000);
}

// ---- Main Loop ----
void loop() {

  // Read Bluetooth command
  if (Serial.available()) {
    command = Serial.read();
    Serial.print("CMD: ");
    Serial.println(command);

    goingForward  = false;
    goingBackward = false;
    gentleTurning = false;

    if (command == 'F' || command == 'f') {
      goingForward = true;
      forward();
      Serial.println("Moving FORWARD");
    }
    else if (command == 'B' || command == 'b') {
      goingBackward = true;
      backward();
      Serial.println("Moving BACKWARD");
    }
    else if (command == 'L' || command == 'l') {
      turnLeft();
      delay(500);
      stopMotors();
      Serial.println("Turned LEFT");
    }
    else if (command == 'R' || command == 'r') {
      turnRight();
      delay(500);
      stopMotors();
      Serial.println("Turned RIGHT");
    }
    // ---- NEW: Gentle turns — car curves while moving ----
    // Send 'G'/'g' for gentle right arc, 'H'/'h' for gentle left arc
    // While in gentle turn, obstacle detection still works
    else if (command == 'G' || command == 'g') {
      goingForward  = true; // still moving forward, check front obstacles
      gentleTurning = true;
      gentleRight();
      Serial.println("Gentle RIGHT arc");
    }
    else if (command == 'H' || command == 'h') {
      goingForward  = true;
      gentleTurning = true;
      gentleLeft();
      Serial.println("Gentle LEFT arc");
    }
    else if (command == 'S' || command == 's') {
      stopMotors();
      Serial.println("STOPPED");
    }

    // Speed control — also updates gentleFactor proportionally
    else if (command == '1') { motorSpeed = 80;  gentleFactor = 30;  Serial.println("Speed: Very Slow"); }
    else if (command == '2') { motorSpeed = 120; gentleFactor = 45;  Serial.println("Speed: Slow"); }
    else if (command == '3') { motorSpeed = 180; gentleFactor = 70;  Serial.println("Speed: Medium"); }
    else if (command == '4') { motorSpeed = 220; gentleFactor = 90;  Serial.println("Speed: Fast"); }
    else if (command == '5') { motorSpeed = 255; gentleFactor = 110; Serial.println("Speed: Full"); }
  }

  // Throttled distance checks — avoids flooding Serial and blocking motion
  if (millis() - lastDistCheck > distCheckInterval) {
    lastDistCheck = millis();

    if (goingForward) {
      int frontDist = getDistance(frontTrig, frontEcho);
      Serial.print("Front: ");
      Serial.println(frontDist);

      if (frontDist > 0 && frontDist < safeDistance) {
        avoidObstacleForward();
        // After avoidance, restore gentle turn if that was the mode
        // (avoidance always ends in forward() so this is fine)
        gentleTurning = false;
      }
    }

    if (goingBackward) {
      int rearDist = getDistance(rearTrig, rearEcho);
      Serial.print("Rear: ");
      Serial.println(rearDist);

      if (rearDist > 0 && rearDist < safeDistance) {
        avoidObstacleBackward();
      }
    }
  }
}
