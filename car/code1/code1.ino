#include <Servo.h>

// ---- Servo Setup ----
Servo frontServo;
Servo rearServo;

// ---- Ultrasonic Pins ----
int frontTrig = 7;
int frontEcho = 8;
int rearTrig  = 12;
int rearEcho  = 13;

// ---- Motor Pins ----
int IN1 = 2; // No PWM
int IN2 = 3; // PWM
int IN3 = 4; // No PWM
int IN4 = 5; // PWM

// ---- Variables ----
char command;
bool goingForward  = false;
bool goingBackward = false;
int motorSpeed     = 180;
int safeDistance   = 30;

// ---- Distance Function ----
int getDistance(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long duration = pulseIn(echoPin, HIGH, 20000);
  int distance = duration * 0.034 / 2;
  if (distance == 0) distance = 999;
  return distance;
}

// ---- Motor Functions ----
void forward() {
  digitalWrite(IN1, HIGH);   // No PWM pin
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH);   // No PWM pin
  digitalWrite(IN4, LOW);
}

void backward() {
  digitalWrite(IN1, LOW);
  analogWrite(IN2, motorSpeed);  // PWM pin
  digitalWrite(IN3, LOW);
  analogWrite(IN4, motorSpeed);  // PWM pin
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

// ---- Scan Functions ----
int scanLeft(Servo s, int trig, int echo) {
  s.write(150);
  delay(400);
  int d = getDistance(trig, echo);
  s.write(90);
  return d;
}

int scanRight(Servo s, int trig, int echo) {
  s.write(30);
  delay(400);
  int d = getDistance(trig, echo);
  s.write(90);
  return d;
}

// ---- Avoid Obstacle Forward ----
void avoidObstacleForward() {
  stopMotors();
  delay(300);

  int leftDist  = scanLeft(frontServo, frontTrig, frontEcho);
  int rightDist = scanRight(frontServo, frontTrig, frontEcho);

  Serial.print("Left: ");   Serial.print(leftDist);
  Serial.print(" Right: "); Serial.println(rightDist);

  if (leftDist > rightDist) {
    Serial.println("Turning LEFT to avoid");
    turnLeft();
    delay(600);
  } else {
    Serial.println("Turning RIGHT to avoid");
    turnRight();
    delay(600);
  }

  forward();
}

// ---- Avoid Obstacle Backward ----
void avoidObstacleBackward() {
  stopMotors();
  delay(300);

  int leftDist  = scanLeft(rearServo, rearTrig, rearEcho);
  int rightDist = scanRight(rearServo, rearTrig, rearEcho);

  Serial.print("Rear Left: ");  Serial.print(leftDist);
  Serial.print("Rear Right: "); Serial.println(rightDist);

  if (leftDist > rightDist) {
    Serial.println("Turning LEFT to avoid");
    turnLeft();
    delay(600);
  } else {
    Serial.println("Turning RIGHT to avoid");
    turnRight();
    delay(600);
  }

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

  Serial.println("System Ready!");
  delay(1000);
}

// ---- Main Loop ----
void loop() {

  // Read Bluetooth command
  if (Serial.available()) {
    command = Serial.read();
    Serial.print("Command: ");
    Serial.println(command);

    // Movement commands
    if (command == 'F' || command == 'f') {
      goingForward  = true;
      goingBackward = false;
      forward();
    }
    else if (command == 'B' || command == 'b') {
      goingBackward = true;
      goingForward  = false;
      backward();
    }
    else if (command == 'L' || command == 'l') {
      goingForward  = false;
      goingBackward = false;
      turnLeft();
      delay(500);
      stopMotors();
    }
    else if (command == 'R' || command == 'r') {
      goingForward  = false;
      goingBackward = false;
      turnRight();
      delay(500);
      stopMotors();
    }
    else if (command == 'S' || command == 's') {
      goingForward  = false;
      goingBackward = false;
      stopMotors();
    }

    // Speed commands
    else if (command == '1') {
      motorSpeed = 80;
      Serial.println("Speed: Very Slow");
    }
    else if (command == '2') {
      motorSpeed = 120;
      Serial.println("Speed: Slow");
    }
    else if (command == '3') {
      motorSpeed = 180;
      Serial.println("Speed: Medium");
    }
    else if (command == '4') {
      motorSpeed = 220;
      Serial.println("Speed: Fast");
    }
    else if (command == '5') {
      motorSpeed = 255;
      Serial.println("Speed: Full");
    }
  }

  // Check front obstacle when moving forward
  if (goingForward) {
    int frontDist = getDistance(frontTrig, frontEcho);
    Serial.print("Front: ");
    Serial.println(frontDist);

    if (frontDist < safeDistance && frontDist > 0) {
      Serial.println("FRONT OBSTACLE!");
      avoidObstacleForward();
    }
  }

  // Check rear obstacle when moving backward
  if (goingBackward) {
    int rearDist = getDistance(rearTrig, rearEcho);
    Serial.print("Rear: ");
    Serial.println(rearDist);

    if (rearDist < safeDistance && rearDist > 0) {
      Serial.println("REAR OBSTACLE!");
      avoidObstacleBackward();
    }
  }
}