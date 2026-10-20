#include <SevSegShift.h>
#include <math.h>
//button
const int button1 = A2;
const int button2 = A1;
const int button3 = A0;
//timer, resistance, light level, temp
const int buzzer = 6;

//led
const int blueLed = 2;
const int redPin = 3;
const int greenPin = 4;
const int bluePin = 5;

//other
const int photoPin = A4;
const int thermistorPin = A3;
const int potential = A5;
int photoValue = 0;
int thermistorValue = 0;
int potentiometerValue = 0;

//display
#define SHIFT_PIN_SHCP 9
#define SHIFT_PIN_STCP 8
#define SHIFT_PIN_DS 7

SevSegShift sevseg(
  SHIFT_PIN_DS,
  SHIFT_PIN_SHCP,
  SHIFT_PIN_STCP,
  1,
  true
);

int treatmentMinutes = 1;
unsigned long timeRemaining = 60;

bool timerRunning = false;
bool alarmOn = false;

unsigned long previousSecond = 0;
unsigned long previousSensor = 0;
unsigned long previousDisplay = 0;

int displayMode = 0;

int lastButton1 = HIGH;
int lastButton2 = HIGH;
int lastButton3 = HIGH;

unsigned long lastPress1 = 0;
unsigned long lastPress2 = 0;
unsigned long lastPress3 = 0;

int brightness = 0;
int brightnessPercent = 0;

const int windowSize = 5;

int lightReadings[5];
int tempReadings[5];

long lightSum = 0;
long tempSum = 0;

int lightIndex = 0;
int tempIndex = 0;

int filteredLight = 0;
int filteredTemp = 0;

//temp
const float resistorValue = 4700.0;
const float thermistorNominal = 10000.0;
const float betaValue = 3950.0;

//when RGB led shine
const float mediumTemperature = 42.0;
const float highTemperature = 45.0;
const int mediumLightIncrease = 100;
const int highLightIncrease = 250;

int startingLight = 0;
int warningLevel = 0;

const unsigned long pwmPeriod = 5000;
bool lastBlueState = false;

void setup() {

  Serial.begin(9600);
  pinMode(button1, INPUT_PULLUP);
  pinMode(button2, INPUT_PULLUP);
  pinMode(button3, INPUT_PULLUP);
  pinMode(buzzer, OUTPUT);
  pinMode(blueLed, OUTPUT);
  pinMode(redPin, OUTPUT);
  pinMode(greenPin, OUTPUT);
  pinMode(bluePin, OUTPUT);
  digitalWrite(blueLed, LOW);
  digitalWrite(redPin, LOW);
  digitalWrite(greenPin, LOW);
  digitalWrite(bluePin, LOW);
  noTone(buzzer);
  byte numDigits = 4;

  byte digitPins[] = {13, 12, 11, 10};
  byte segmentPins[] = {0, 1, 2, 3, 4, 5, 6, 7};
  bool resistorsOnSegments = false;
  byte hardwareConfig = COMMON_CATHODE;
  bool updateWithDelays = false;
  bool leadingZeros = true;
  bool disableDecPoint = false;
  sevseg.begin(
    hardwareConfig,
    numDigits,
    digitPins,
    segmentPins,
    resistorsOnSegments,
    updateWithDelays,
    leadingZeros,
    disableDecPoint
  );

  sevseg.setBrightness(50);

  photoValue = analogRead(photoPin);
  thermistorValue = analogRead(thermistorPin);

  startingLight = photoValue;

  for (int i = 0; i < windowSize; i++) {
    lightReadings[i] = photoValue;
    tempReadings[i] = thermistorValue;

    lightSum += photoValue;
    tempSum += thermistorValue;
  }

  filteredLight = photoValue;
  filteredTemp = thermistorValue;

  previousSecond = millis();
  previousSensor = millis();

  sevseg.setNumber(100);

  Serial.println("Phototherapy System Ready");
}

void loop() {
  sevseg.refreshDisplay();
  checkButtons();
  controlBrightness();
  updateTimer();
  updateBlueLED();
  updateBuzzer();

  if (millis() - previousSensor >= 1000) {
    previousSensor += 1000;
    readSensors();
    filterSensors();
    checkWarnings();
    updateWarningLED();
  }
  if (millis() - previousDisplay >= 100) {
    previousDisplay = millis();
    updateDisplay();
  }
}

void readSensors() {
  photoValue = analogRead(photoPin);
  thermistorValue = analogRead(thermistorPin);
}

void filterSensors() {
  lightSum -= lightReadings[lightIndex];

  lightReadings[lightIndex] = photoValue;
  lightSum += photoValue;
  filteredLight = lightSum / windowSize;
  lightIndex++;

  if (lightIndex >= windowSize) {
    lightIndex = 0;
  }
  tempSum -= tempReadings[tempIndex];
  tempReadings[tempIndex] = thermistorValue;
  tempSum += thermistorValue;
  filteredTemp = tempSum / windowSize;
  tempIndex++;

  if (tempIndex >= windowSize) {
    tempIndex = 0;
  }
}

float getTemperature() {
  if (filteredTemp <= 0 || filteredTemp >= 1023) {
    return NAN;
  }
  float resistance = resistorValue *(1023.0 / filteredTemp - 1.0);
  float temperature = log(resistance / thermistorNominal);
  temperature = temperature / betaValue;
  temperature += 1.0 / 298.15;
  temperature = 1.0 / temperature;
  temperature -= 273.15;
  return temperature;
}

void controlBrightness() {
  potentiometerValue = analogRead(potential);
  brightness = map(potentiometerValue, 0, 1023,0, 255);
  brightnessPercent = map(potentiometerValue,0, 1023,0, 100);
}

void updateBlueLED() {
  if (!timerRunning || warningLevel == 2) {
    digitalWrite(blueLed, LOW);
    lastBlueState = false;
    return;
  }
  unsigned long position = micros() % pwmPeriod;
  unsigned long onTime = (unsigned long)brightness * pwmPeriod / 255;
  bool ledState = (position < onTime);

  if (ledState != lastBlueState) {
    digitalWrite(blueLed, ledState ? HIGH : LOW);
    lastBlueState = ledState;
  }
}

void checkWarnings() {
  float temperature = getTemperature();
  warningLevel = 0;

  if (isnan(temperature)) {
    warningLevel = 2;
    return;
  }
  if (temperature >= mediumTemperature) {
    warningLevel = 1;
  }
  if (temperature >= highTemperature) {
    warningLevel = 2;
  }
  if (filteredLight >= startingLight + mediumLightIncrease) {
    if (warningLevel < 1) {
      warningLevel = 1;
    }
  }
  if (filteredLight >= startingLight + highLightIncrease) {
    warningLevel = 2;
  }
}

void updateWarningLED() {
  if (warningLevel == 0) {
    digitalWrite(redPin, LOW);
    digitalWrite(greenPin, LOW);
    digitalWrite(bluePin, LOW);
  }
  else if (warningLevel == 1) {
    digitalWrite(redPin, HIGH);
    digitalWrite(greenPin, HIGH);
    digitalWrite(bluePin, LOW);
  }
  else {
    digitalWrite(redPin, HIGH);
    digitalWrite(greenPin, LOW);
    digitalWrite(bluePin, LOW);
  }
}

void checkButtons() {
  int state1 = digitalRead(button1);
  int state2 = digitalRead(button2);
  int state3 = digitalRead(button3);
  unsigned long now = millis();

  if (state1 == LOW && lastButton1 == HIGH && now - lastPress1 >= 200) {
    lastPress1 = now;
    if (!timerRunning && !alarmOn) {
      treatmentMinutes++;
      if (treatmentMinutes > 60) {
        treatmentMinutes = 1;
      }
      timeRemaining =(unsigned long)treatmentMinutes * 60;
    }
  }

  if (state2 == LOW && lastButton2 == HIGH && now - lastPress2 >= 200) {
    lastPress2 = now;
    if (timerRunning || alarmOn) {
      resetTimer();
    }
    else {
      timerRunning = true;
      previousSecond = now;
      timeRemaining =(unsigned long)treatmentMinutes * 60;
    }
  }
  if (state3 == LOW && lastButton3 == HIGH && now - lastPress3 >= 200) {
    lastPress3 = now;
    displayMode++;
    if (displayMode > 3) {
      displayMode = 0;
    }
  }
  lastButton1 = state1;
  lastButton2 = state2;
  lastButton3 = state3;
}

void updateTimer() {

  if (timerRunning) {
    unsigned long now = millis();
    if (now - previousSecond >= 1000) {
      unsigned long elapsed = (now - previousSecond) / 1000;
      previousSecond += elapsed * 1000;
      if (elapsed >= timeRemaining) {
        timeRemaining = 0;
      }
      else {
        timeRemaining -= elapsed;
      }

      if (timeRemaining == 0) {
        timerRunning = false;
        alarmOn = true;
      }
    }
  }
}

void resetTimer() {
  timerRunning = false;
  alarmOn = false;
  timeRemaining =(unsigned long)treatmentMinutes * 60;
  noTone(buzzer);
}

void updateBuzzer() {
  static bool buzzerPlaying = false;
  if (alarmOn && !buzzerPlaying) {
    tone(buzzer, 1000);
    buzzerPlaying = true;
  }
  if (!alarmOn && buzzerPlaying) {
    noTone(buzzer);
    buzzerPlaying = false;
  }
}

void updateDisplay() {
  if (displayMode == 0) {
    int minutes = timeRemaining / 60;
    int seconds = timeRemaining % 60;
    int displayTime = minutes * 100 + seconds;
    sevseg.setNumber(displayTime);
  }
  else if (displayMode == 1) {
    sevseg.setNumber(brightnessPercent);
  }
  else if (displayMode == 2) {
    sevseg.setNumber(filteredLight);
  }
  else if (displayMode == 3) {
    float temperature = getTemperature();
    if (isnan(temperature)) {
      sevseg.setChars("Err ");
    }
    else {
      int displayTemp = round(temperature * 10);
      sevseg.setNumber(displayTemp, 1);
    }
  }
}
