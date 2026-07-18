int trigPin = 7;
int echoPin = 8;

long duration;
int distance;

void setup() {
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  Serial.begin(9600);
  Serial.println("Ultrasonic Sensor Test Ready!");
}

void loop() {
  // Make sure trig is clean before pulse
  digitalWrite(trigPin, LOW);
  delayMicroseconds(4);  // increased from 2

  // Send pulse
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // Read echo with timeout
  duration = pulseIn(echoPin, HIGH, 30000); // 30ms timeout

  // Check if reading is valid
  if (duration == 0) {
    Serial.println("No echo received — check wiring!");
  } else {
    distance = duration * 0.034 / 2;
    Serial.print("Duration: ");
    Serial.print(duration);
    Serial.print(" | Distance: ");
    Serial.print(distance);
    Serial.println(" cm");
  }

  delay(500);
}