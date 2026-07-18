// ===== SENSOR 1 =====
int trig1 = 11;
int echo1 = 12;

// ===== SENSOR 2 =====
int trig2 = A0;
int echo2 = A2;

// ===== SENSOR 3 =====
int trig3 = A3;
int echo3 = 13;

long distance1, distance2, distance3;

void setup() {
  
  pinMode(trig1, OUTPUT);
  pinMode(echo1, INPUT);
  pinMode(trig2, OUTPUT);
  pinMode(echo2, INPUT);
  pinMode(trig3, OUTPUT);
  pinMode(echo3, INPUT);

  Serial.begin(9600);
  Serial.println("Testing 3 Ultrasonic Sensors...");
}

long getDistance(int trig, int echo) {
  digitalWrite(trig, LOW);
  delayMicroseconds(2);
  digitalWrite(trig, HIGH);
  delayMicroseconds(10);
  digitalWrite(trig, LOW);

  long duration = pulseIn(echo, HIGH, 15000);
  if (duration == 0) return 999;
  return duration * 0.034 / 2;
}

void loop() {
  distance1 = getDistance(trig1, echo1);
  distance2 = getDistance(trig2, echo2);
  distance3 = getDistance(trig3, echo3);

  Serial.print("Sensor 1 (11/12): ");
  Serial.print(distance1);
  Serial.println(" cm");

  Serial.print("Sensor 2 (A0/A1): ");
  Serial.print(distance2);
  Serial.println(" cm");

  Serial.print("Sensor 3 (A2/A3): ");
  Serial.print(distance3);
  Serial.println(" cm");

  Serial.println("------------------");
  delay(500);
}