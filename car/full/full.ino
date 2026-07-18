#include <AFMotor.h>
#include <Servo.h>

// ----- Motors -----
AF_DCMotor motor1(1); // M1
AF_DCMotor motor2(2); // M2
AF_DCMotor motor3(3); // M3
AF_DCMotor motor4(4); // M4

// ----- Servo (only SER2 = D10) -----
Servo servo;

// ----- Ultrasonic pins (single sensor) -----
const int trig = A0;
const int echo = A1;

// ----- Settings -----
int motorSpeed = 200;          // 0-255
int stopDistance = 25;         // cm threshold
bool autoMode = false;

String cmd = "";

// ----- Helper: distance -----
long readDistanceCM(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH, 30000); // 30ms timeout
  if (duration == 0) return 300; // no echo
  return duration * 0.034 / 2;
}

// ----- Motor control -----
void stopAll() {
  motor1.run(RELEASE);
  motor2.run(RELEASE);
  motor3.run(RELEASE);
  motor4.run(RELEASE);
}

void setAllSpeed(int s) {
  motor1.setSpeed(s);
  motor2.setSpeed(s);
  motor3.setSpeed(s);
  motor4.setSpeed(s);
}

void forward() {
  setAllSpeed(motorSpeed);
  motor1.run(FORWARD);
  motor2.run(FORWARD);
  motor3.run(FORWARD);
  motor4.run(FORWARD);
}

void backward() {
  setAllSpeed(motorSpeed);
  motor1.run(BACKWARD);
  motor2.run(BACKWARD);
  motor3.run(BACKWARD);
  motor4.run(BACKWARD);
}

void left() {
  setAllSpeed(motorSpeed);
  motor1.run(BACKWARD);
  motor2.run(BACKWARD);
  motor3.run(FORWARD);
  motor4.run(FORWARD);
}

void right() {
  setAllSpeed(motorSpeed);
  motor1.run(FORWARD);
  motor2.run(FORWARD);
  motor3.run(BACKWARD);
  motor4.run(BACKWARD);
}

// ----- Auto obstacle avoidance (single sensor + servo scan) -----
void autoAvoid() {
  // scan left
  servo.write(20);
  delay(300);
  long leftDist = readDistanceCM(trig, echo);

  // scan center
  servo.write(90);
  delay(200);
  long frontDist = readDistanceCM(trig, echo);

  // scan right
  servo.write(160);
  delay(300);
  long rightDist = readDistanceCM(trig, echo);

  // center
  servo.write(90);
  delay(100);

  if (frontDist > stopDistance) {
    forward();
  } else {
    stopAll();
    delay(150);
    backward();
    delay(300);
    stopAll();
    delay(150);

    if (leftDist > rightDist) {
      left();
    } else {
      right();
    }
    delay(400);
    stopAll();
  }
}

void setup() {
  Serial.begin(9600);

  pinMode(trig, OUTPUT);
  pinMode(echo, INPUT);

  servo.attach(10); // SER2 (D10)
  servo.write(90);

  stopAll();
}

void loop() {
  if (Serial.available()) {
    cmd = Serial.readStringUntil('\n');
    cmd.trim();
    cmd.toUpperCase();

    if (cmd == "A" || cmd == "AUTO") {
      autoMode = true;
    } else if (cmd == "S" || cmd == "STOP") {
      autoMode = false;
      stopAll();
    } else if (!autoMode) {
      if (cmd == "F" || cmd == "FORWARD") forward();
      else if (cmd == "B" || cmd == "BACK") backward();
      else if (cmd == "L" || cmd == "LEFT") left();
      else if (cmd == "R" || cmd == "RIGHT") right();
    }
  }

  if (autoMode) {
    autoAvoid();
  }
}