// Signal 1 : GP2 (R), GP3 (Y), GP4 (G)
// Signal 2 : GP5 (R), GP6 (Y), GP7 (G)
// Signal 3 : GP8 (R), GP9 (Y), GP10 (G)
// Signal 4 : GP11 (R), GP12 (Y), GP13 (G)
// IR sensor : GP26 (ADC0) used as digital input
// ------------------------------------------------------------------------

int signal1[] = {2, 3, 4};    
int signal2[] = {5, 6, 7};
int signal3[] = {8, 9, 10};
int signal4[] = {11, 12, 13};

// delays
int redDelay = 5000;
int yellowDelay = 5000;
int greenDelay = 5000;
int green2Delay = 5000;

// IR sensor
const int IR_PIN = 26;   // GP26 (ADC0)
const int IR_LANE = 0;   // IR belongs to signal1

// ambulance timing
const unsigned long ambulanceGreenDuration = 8000;
const unsigned long POLL_INTERVAL_MS = 25;

void setup() {
  // LED pins output
  for (int i = 0; i < 3; i++) {
    pinMode(signal1[i], OUTPUT);
    pinMode(signal2[i], OUTPUT);
    pinMode(signal3[i], OUTPUT);
    pinMode(signal4[i], OUTPUT);
  }

  // IR sensor input
  pinMode(IR_PIN, INPUT);

  // all lights red initially
  setAllRed();
}

void loop() {

  // signal 1 green
  setSignalState(0, 2);
  setSignalState(1, 0);
  setSignalState(2, 0);
  setSignalState(3, 0);
  if (waitWithIRCheck(greenDelay, 0)) return;

  // signal 1 yellow
  setSignalState(0, 1);
  if (waitWithIRCheck(yellowDelay, 0)) return;

  // signal2 green
  setSignalState(0, 0);
  setSignalState(1, 2);
  if (waitWithIRCheck(green2Delay, 1)) return;

  // signal2 yellow
  setSignalState(1, 1);
  if (waitWithIRCheck(yellowDelay, 1)) return;

  // signal3 green
  setSignalState(1, 0);
  setSignalState(2, 2);
  if (waitWithIRCheck(green2Delay, 2)) return;

  // signal3 yellow
  setSignalState(2, 1);
  if (waitWithIRCheck(yellowDelay, 2)) return;

  // signal4 green
  setSignalState(2, 0);
  setSignalState(3, 2);
  if (waitWithIRCheck(greenDelay, 3)) return;

  // signal4 yellow
  setSignalState(3, 1);
  if (waitWithIRCheck(yellowDelay, 3)) return;

  // back to signal1
  setSignalState(3, 0);
  setSignalState(0, 2);
  if (waitWithIRCheck(greenDelay, 0)) return;
}

void setSignalState(int signalIndex, int state) {
  int *sig;
  switch (signalIndex) {
    case 0: sig = signal1; break;
    case 1: sig = signal2; break;
    case 2: sig = signal3; break;
    case 3: sig = signal4; break;
    default: return;
  }

  digitalWrite(sig[0], LOW);
  digitalWrite(sig[1], LOW);
  digitalWrite(sig[2], LOW);

  if (state == 0) digitalWrite(sig[0], HIGH);
  else if (state == 1) digitalWrite(sig[1], HIGH);
  else if (state == 2) digitalWrite(sig[2], HIGH);
}

void setAllRed() {
  setSignalState(0, 0);
  setSignalState(1, 0);
  setSignalState(2, 0);
  setSignalState(3, 0);
}

bool waitWithIRCheck(unsigned long ms, int currentLane) {
  unsigned long start = millis();
  while (millis() - start < ms) {
    if (currentLane == IR_LANE && digitalRead(IR_PIN) == HIGH) {
      handleAmbulance(IR_LANE);
      return true;
    }
    delay(POLL_INTERVAL_MS);
  }
  return false;
}

void handleAmbulance(int lane) {

  for (int i = 0; i < 4; i++) {
    if (i == lane) setSignalState(i, 2);
    else setSignalState(i, 0);
  }

  unsigned long start = millis();
  while (millis() - start < ambulanceGreenDuration) {
    if (digitalRead(IR_PIN) == HIGH) {
      start = millis();
    }
    delay(POLL_INTERVAL_MS);
  }

  setSignalState(lane, 1);

  unsigned long ystart = millis();
  while (millis() - ystart < yellowDelay) {
    if (digitalRead(IR_PIN) == HIGH) {
      handleAmbulance(lane);
      return;
    }
    delay(POLL_INTERVAL_MS);
  }

  setSignalState(lane, 0);
}
