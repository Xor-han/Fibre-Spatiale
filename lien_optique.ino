const uint8_t LED_PIN   = 9; // pin de la led
const uint8_t LDR_PIN   = A0; //pin de la photorésistance
const uint8_t MAX_TX    = 200; //max texte
const uint8_t STOP_BITS = 2; //nombre de bits pour stopper
const uint8_t LEVEL_MS  = 150; //timing entre les bits

unsigned int bitMs = 150; 
int  darkVal = 0, lightVal = 1023, thr = 512; 
bool litIsHigh = true; 

char    txBuf[MAX_TX]; 
uint8_t txLen = 0, txIdx = 0; 
int8_t  txBit = -1;            
uint8_t txByte;
unsigned long txT;

int8_t  rxBit = -1;
unsigned long rxT0;
uint8_t rxByte;
bool    prevLit = false;

char    line[MAX_TX + 8];
uint8_t lineLen = 0;
unsigned long lastLevel = 0;

bool isLit(int v) { return litIsHigh ? v > thr : v < thr; } 

int avgRead(uint8_t n) {
  long s = 0;
  for (uint8_t i = 0; i < n; i++) { s += analogRead(LDR_PIN); delay(2); }
  return s / n;
}

void calibrate() {
  digitalWrite(LED_PIN, LOW);  delay(400); darkVal  = avgRead(32);
  digitalWrite(LED_PIN, HIGH); delay(400); lightVal = avgRead(32);
  digitalWrite(LED_PIN, LOW);  delay(400);
  thr = (darkVal + lightVal) / 2;
  litIsHigh = lightVal > darkVal;
  Serial.print(F("CAL:")); Serial.print(darkVal); Serial.print(',');
  Serial.print(lightVal);  Serial.print(',');     Serial.println(thr);
  if (abs(lightVal - darkVal) < 60) Serial.println(F("E:CONTRASTE_FAIBLE"));
  prevLit = false;
  rxBit = -1;
}

void txUpdate(unsigned long now) {
  if (txBit < 0) {
    if (txIdx < txLen) {
      txByte = (uint8_t)txBuf[txIdx];
      txBit = 0;
      txT = now;
      digitalWrite(LED_PIN, HIGH);
      Serial.print(F("S:")); Serial.println(txByte);
    }
    return;
  }
  if (now - txT < bitMs) return;
  txT += bitMs;
  txBit++;
  if (txBit <= 8) {
    digitalWrite(LED_PIN, (txByte >> (txBit - 1)) & 1);
  } else if (txBit <= 8 + STOP_BITS) {
    digitalWrite(LED_PIN, LOW);
  } else {
    txBit = -1;
    txIdx++;
    if (txIdx >= txLen) { txLen = 0; txIdx = 0; Serial.println(F("D")); }
  }
}

void rxUpdate(unsigned long now) {
  if (rxBit < 0) {
    bool lit = isLit(analogRead(LDR_PIN));
    if (lit && !prevLit) { rxT0 = now; rxBit = 0; rxByte = 0; }
    prevLit = lit;
    return;
  }
  if (now - rxT0 < (unsigned long)bitMs * rxBit + bitMs / 2) return;
  bool lit = isLit(analogRead(LDR_PIN));

  if (rxBit == 0) {
    if (!lit) { rxBit = -1; prevLit = false; return; }
  } else if (rxBit <= 8) {
    if (lit) rxByte |= (1 << (rxBit - 1));
  } else {
    if (!lit) { Serial.print(F("R:")); Serial.println(rxByte); }
    else      Serial.println(F("E:TRAME"));
    rxBit = -1;
    prevLit = lit;
    return;
  }
  rxBit++;
}

void handleLine() {
  if (line[0] == 'T' && line[1] == ':') {
    for (uint8_t i = 2; line[i] && txLen < MAX_TX; i++) txBuf[txLen++] = line[i];
  } else if (line[0] == 'B' && line[1] == ':') {
    int v = atoi(line + 2);
    if (v >= 10 && v <= 1000) bitMs = v;
    Serial.print(F("B:")); Serial.println(bitMs);
  } else if (line[0] == 'C') {
    if (txBit < 0 && txLen == 0) calibrate();
    else Serial.println(F("E:OCCUPE"));
  } else if (line[0] == 'O' && line[1] == ':') {
    if (txBit < 0 && txLen == 0) {
      digitalWrite(LED_PIN, line[2] == '1' ? HIGH : LOW);
      Serial.print(F("O:")); Serial.println(line[2] == '1' ? 1 : 0);
    } else Serial.println(F("E:OCCUPE"));
  } else if (line[0] == 'K' && line[1] == ':') {
    if (line[2] == '0') {
      darkVal = avgRead(32);
      Serial.print(F("K:0,")); Serial.println(darkVal);
    } else if (line[2] == '1') {
      lightVal = avgRead(32);
      Serial.print(F("K:1,")); Serial.println(lightVal);
    } else if (line[2] == '!') {
      thr = (darkVal + lightVal) / 2;
      litIsHigh = lightVal > darkVal;
      prevLit = false;
      rxBit = -1;
      Serial.print(F("CAL:")); Serial.print(darkVal); Serial.print(',');
      Serial.print(lightVal);  Serial.print(',');     Serial.println(thr);
      if (abs(lightVal - darkVal) < 60) Serial.println(F("E:CONTRASTE_FAIBLE"));
    }
  }
}

void serialUpdate() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\r') continue;
    if (c == '\n') { line[lineLen] = 0; handleLine(); lineLen = 0; }
    else if (lineLen < sizeof(line) - 1) line[lineLen++] = c;
  }
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  pinMode(LDR_PIN, INPUT_PULLUP);
  Serial.begin(115200);
  calibrate();
  Serial.println(F("READY"));
}

void loop() {
  unsigned long now = millis();
  serialUpdate();
  txUpdate(now);
  rxUpdate(now);
  if (now - lastLevel >= LEVEL_MS) {
    lastLevel = now;
    Serial.print(F("L:")); Serial.println(analogRead(LDR_PIN));
  }
}
