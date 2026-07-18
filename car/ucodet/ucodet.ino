#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27,16,2);

// ===== LED PINS =====
int R1=2, Y1=3, G1=4;
int R2=5, Y2=6, G2=7;
int R3=8, Y3=9, G3=10;

// ===== ULTRASONIC =====
int trig1=11, echo1=12;
int trig2=A0, echo2=A2;  // fixed pins
int trig3=A3, echo3=13;  // fixed pins

long d1,d2,d3;

// ===== SETTINGS =====
int GREEN_CAR    = 12;  // car detected
int GREEN_NO_CAR = 5;   // no car
int YELLOW_TIME  = 2;   // always 2 sec
int CAR_DISTANCE = 100; // cm — if closer than this = car detected

int currentLane  = 1;

// ===== GET DISTANCE =====
long getDistance(int trig, int echo){
  digitalWrite(trig, LOW);
  delayMicroseconds(2);
  digitalWrite(trig, HIGH);
  delayMicroseconds(10);
  digitalWrite(trig, LOW);

  long duration = pulseIn(echo, HIGH, 15000);
  if(duration == 0) return 999;
  return duration * 0.034 / 2;
}

// ===== UPDATE SENSORS =====
void updateSensors(){
  d1 = getDistance(trig1, echo1);
  d2 = getDistance(trig2, echo2);
  d3 = getDistance(trig3, echo3);
}

// ===== CAR DETECTED? =====
bool carDetected(long distance){
  if(distance < CAR_DISTANCE && distance != 999){
    return true;   // car detected
  }
  return false;    // no car
}

// ===== GET GREEN TIME =====
int getGreenTime(long distance){
  if(carDetected(distance)){
    return GREEN_CAR;     // 12 sec
  }
  return GREEN_NO_CAR;    // 5 sec
}

// ===== RESET ALL =====
void resetAll(){
  digitalWrite(R1,HIGH); digitalWrite(R2,HIGH); digitalWrite(R3,HIGH);
  digitalWrite(G1,LOW);  digitalWrite(G2,LOW);  digitalWrite(G3,LOW);
  digitalWrite(Y1,LOW);  digitalWrite(Y2,LOW);  digitalWrite(Y3,LOW);
}

// ===== RUN GREEN =====
void runGreen(int lane, int sec){
  resetAll();

  if(lane==1){ digitalWrite(R1,LOW); digitalWrite(G1,HIGH); }
  if(lane==2){ digitalWrite(R2,LOW); digitalWrite(G2,HIGH); }
  if(lane==3){ digitalWrite(R3,LOW); digitalWrite(G3,HIGH); }

  Serial.print("Road "); Serial.print(lane);
  if(sec == GREEN_CAR){
    Serial.println(" GREEN — Car Detected! 12s");
  } else {
    Serial.println(" GREEN — No Car. 5s");
  }

  for(int i=sec; i>0; i--){
    updateSensors();

    // show on lcd
    lcd.setCursor(0,0);
    lcd.print("Road ");
    lcd.print(lane);
    lcd.print(" GREEN  ");

    lcd.setCursor(0,1);
    lcd.print("Time: ");
    lcd.print(i);
    lcd.print("s   ");

    // show on serial
    Serial.print("S1: "); Serial.print(d1);
    Serial.print("cm | S2: "); Serial.print(d2);
    Serial.print("cm | S3: "); Serial.print(d3);
    Serial.print("cm | Time: "); Serial.print(i);
    Serial.println("s");

    delay(1000);
  }
}

// ===== RUN YELLOW =====
void runYellow(int lane){
  digitalWrite(R1,HIGH); digitalWrite(R2,HIGH); digitalWrite(R3,HIGH);
  digitalWrite(G1,LOW);  digitalWrite(G2,LOW);  digitalWrite(G3,LOW);

  if(lane==1) digitalWrite(Y1,HIGH);
  if(lane==2) digitalWrite(Y2,HIGH);
  if(lane==3) digitalWrite(Y3,HIGH);

  Serial.print("Road "); Serial.print(lane);
  Serial.println(" YELLOW — 2s");

  for(int i=YELLOW_TIME; i>0; i--){
    lcd.setCursor(0,0);
    lcd.print("Road ");
    lcd.print(lane);
    lcd.print(" YELLOW ");

    lcd.setCursor(0,1);
    lcd.print("Wait: ");
    lcd.print(i);
    lcd.print("s   ");

    Serial.print("Wait: "); Serial.print(i); Serial.println("s");
    delay(1000);
  }

  digitalWrite(Y1,LOW);
  digitalWrite(Y2,LOW);
  digitalWrite(Y3,LOW);
}

// ===== SETUP =====
void setup(){
  lcd.init();
  lcd.backlight();

  pinMode(R1,OUTPUT); pinMode(Y1,OUTPUT); pinMode(G1,OUTPUT);
  pinMode(R2,OUTPUT); pinMode(Y2,OUTPUT); pinMode(G2,OUTPUT);
  pinMode(R3,OUTPUT); pinMode(Y3,OUTPUT); pinMode(G3,OUTPUT);

  pinMode(trig1,OUTPUT); pinMode(echo1,INPUT);
  pinMode(trig2,OUTPUT); pinMode(echo2,INPUT);
  pinMode(trig3,OUTPUT); pinMode(echo3,INPUT);

  Serial.begin(9600);
  Serial.println("Traffic System Ready!");

  // all red at start
  digitalWrite(R1,HIGH);
  digitalWrite(R2,HIGH);
  digitalWrite(R3,HIGH);
}

// ===== LOOP =====
void loop(){
  updateSensors();

  // get green time based on car detection
  int t1 = getGreenTime(d1);
  int t2 = getGreenTime(d2);
  int t3 = getGreenTime(d3);

  if(currentLane == 1){
    runGreen(1, t1);
    runYellow(1);
    currentLane = 2;
  }
  else if(currentLane == 2){
    runGreen(2, t2);
    runYellow(2);
    currentLane = 3;
  }
  else if(currentLane == 3){
    runGreen(3, t3);
    runYellow(3);
    currentLane = 1;
  }
}