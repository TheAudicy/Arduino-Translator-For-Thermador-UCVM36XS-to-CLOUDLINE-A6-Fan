/*
 * Thermador UCVM36XS to Cloudline A6 Fan Controller - Compact Version
 * Optimized for Arduino Nano memory constraints
 */

// Pin definitions
#define PWM_PIN 9
#define TACH_PIN 2
#define LED_PIN 13
#define SPEED1_PIN 3  // BLUE wire
#define SPEED2_PIN 4  // GRAY wire  
#define SPEED3_PIN 5  // ORANGE wire
#define SPEED4_PIN 6  // BLACK wire

// Fan speeds (0.0 to 1.0)
const float FAN_SPEEDS[] = {0.0, 0.25, 0.50, 0.75, 1.0};

// Global variables
float currentSpeed = 0.0;
int speedLevel = 0;
unsigned long lastChange = 0;
volatile unsigned long tachCount = 0;
float rpm = 0;
bool manualMode = false;

void setup() {
  Serial.begin(9600);
  Serial.println(F("Thermador to Cloudline Translator"));
  
  // Setup pins
  pinMode(PWM_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(SPEED1_PIN, INPUT_PULLUP);
  pinMode(SPEED2_PIN, INPUT_PULLUP);
  pinMode(SPEED3_PIN, INPUT_PULLUP);
  pinMode(SPEED4_PIN, INPUT_PULLUP);
  pinMode(TACH_PIN, INPUT_PULLUP);
  
  // Setup PWM - 25kHz
  TCCR1A = (1 << COM1A1) | (1 << WGM11);
  TCCR1B = (1 << CS10) | (1 << WGM13);
  ICR1 = 320;
  OCR1A = 0;
  
  attachInterrupt(digitalPinToInterrupt(TACH_PIN), tachISR, FALLING);
  
  setFanSpeed(0.0);
  Serial.println(F("Ready!"));
}

void loop() {
  static unsigned long lastUpdate = 0;
  static unsigned long lastStatus = 0;
  
  // Check inputs every 50ms (only if not in manual mode)
  if (millis() - lastUpdate > 50 && !manualMode) {
    lastUpdate = millis();
    
    int newLevel = readSpeedLevel();
    if (newLevel != speedLevel && millis() - lastChange > 100) {
      speedLevel = newLevel;
      setFanSpeed(FAN_SPEEDS[speedLevel]);
      lastChange = millis();
      
      Serial.print(F("Speed: "));
      Serial.println(getSpeedName());
    }
    
    updateRPM();
  }
  
  // Status every 3 seconds
  if (millis() - lastStatus > 3000) {
    lastStatus = millis();
    printStatus();
  }
  
  handleCommands();
  updateLED();
}

int readSpeedLevel() {
  if (!digitalRead(SPEED4_PIN)) return 4;  // BLACK - High
  if (!digitalRead(SPEED3_PIN)) return 3;  // ORANGE - Med-High  
  if (!digitalRead(SPEED2_PIN)) return 2;  // GRAY - Medium
  if (!digitalRead(SPEED1_PIN)) return 1;  // BLUE - Low
  return 0;  // Off
}

void setFanSpeed(float speed) {
  speed = constrain(speed, 0.0, 1.0);
  currentSpeed = speed;
  
  if (speed == 0) {
    OCR1A = 0;
  } else {
    float scaled = 0.20 + speed * 0.80;  // 20-100% range
    OCR1A = (uint16_t)(320 * scaled);
  }
}

void tachISR() {
  tachCount++;
}

void updateRPM() {
  static unsigned long lastRPMTime = 0;
  static unsigned long lastCount = 0;
  
  if (millis() - lastRPMTime >= 1000) {
    unsigned long pulses = tachCount - lastCount;
    rpm = (pulses * 60.0) / 2.0;  // 2 pulses per revolution
    lastCount = tachCount;
    lastRPMTime = millis();
  }
}

void updateLED() {
  int interval = 2000;
  switch (speedLevel) {
    case 1: interval = 1000; break;
    case 2: interval = 500; break;
    case 3: interval = 250; break;
    case 4: interval = 100; break;
  }
  digitalWrite(LED_PIN, (millis() / interval) % 2);
}

void handleCommands() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    cmd.toLowerCase();
    
    if (cmd == F("off")) {
      speedLevel = 0;
      setFanSpeed(FAN_SPEEDS[0]);
      manualMode = true;
      Serial.println(F("Manual: OFF"));
    }
    else if (cmd == F("low")) {
      speedLevel = 1;
      setFanSpeed(FAN_SPEEDS[1]);
      manualMode = true;
      Serial.println(F("Manual: LOW"));
    }
    else if (cmd == F("medium")) {
      speedLevel = 2;
      setFanSpeed(FAN_SPEEDS[2]);
      manualMode = true;
      Serial.println(F("Manual: MEDIUM"));
    }
    else if (cmd == F("high")) {
      speedLevel = 3;
      setFanSpeed(FAN_SPEEDS[3]);
      manualMode = true;
      Serial.println(F("Manual: HIGH"));
    }
    else if (cmd == F("max")) {
      speedLevel = 4;
      setFanSpeed(FAN_SPEEDS[4]);
      manualMode = true;
      Serial.println(F("Manual: MAX"));
    }
    else if (cmd == F("auto")) {
      manualMode = false;
      Serial.println(F("Auto mode ON"));
    }
    else if (cmd == F("status")) {
      printDetailedStatus();
    }
    else if (cmd == F("test")) {
      runTest();
    }
    else if (cmd == F("help")) {
      printHelp();
    }
  }
}

void printStatus() {
  Serial.print(F("Level:"));
  Serial.print(speedLevel);
  Serial.print(F(" Speed:"));
  Serial.print((int)(currentSpeed * 100));
  Serial.print(F("% RPM:"));
  Serial.println((int)rpm);
}

void printDetailedStatus() {
  Serial.println(F("=== Status ==="));
  Serial.print(F("Speed Level: "));
  Serial.println(getSpeedName());
  Serial.print(F("Fan Speed: "));
  Serial.print((int)(currentSpeed * 100));
  Serial.println(F("%"));
  Serial.print(F("RPM: "));
  Serial.println((int)rpm);
  Serial.print(F("Inputs: B"));
  Serial.print(!digitalRead(SPEED1_PIN));
  Serial.print(F(" G"));
  Serial.print(!digitalRead(SPEED2_PIN));
  Serial.print(F(" O"));
  Serial.print(!digitalRead(SPEED3_PIN));
  Serial.print(F(" K"));
  Serial.println(!digitalRead(SPEED4_PIN));
}

const char* getSpeedName() {
  switch (speedLevel) {
    case 0: return "OFF";
    case 1: return "LOW";
    case 2: return "MEDIUM"; 
    case 3: return "HIGH";
    case 4: return "MAX";
    default: return "UNKNOWN";
  }
}

void runTest() {
  Serial.println(F("Testing fan speeds..."));
  
  for (int i = 0; i <= 4; i++) {
    Serial.print(F("Testing "));
    Serial.println(getSpeedName());
    setFanSpeed(FAN_SPEEDS[i]);
    delay(2000);
  }
  
  setFanSpeed(0);
  speedLevel = 0;
  Serial.println(F("Test complete"));
}

void printHelp() {
  Serial.println(F("Commands:"));
  Serial.println(F("off/low/medium/high/max"));
  Serial.println(F("auto - enable auto mode"));
  Serial.println(F("status/test/help"));
}