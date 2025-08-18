/*
 * Thermador UCVM36XS to Cloudline A6 Fan Speed Translator
 * 
 * This Arduino Nano acts as a translator between:
 * INPUT: Thermador UCVM36XS fan speed control signals
 * OUTPUT: PWM control for AC Infinity Cloudline A6 6-inch fan
 * 
 * The Arduino reads the fan speed signals from the Thermador control
 * and converts them to appropriate PWM signals for the Cloudline A6
 */

#include <Arduino.h>

// Pin definitions
#define CLOUDLINE_PWM_PIN 9      // PWM output to Cloudline A6
#define CLOUDLINE_TACH_PIN 2     // Tachometer from Cloudline A6 (optional)
#define STATUS_LED_PIN 13        // Status LED

// Thermador input pins - adjust based on actual wiring
#define THERMADOR_SPEED1_PIN 3   // Low speed signal from Thermador
#define THERMADOR_SPEED2_PIN 4   // Medium speed signal from Thermador  
#define THERMADOR_SPEED3_PIN 5   // High speed signal from Thermador
#define THERMADOR_COMMON_PIN 6   // Common/ground from Thermador (if needed)

// Alternative: Single analog input if Thermador uses variable voltage
#define THERMADOR_ANALOG_PIN A0  // For variable voltage speed control

// Fan speed settings for Cloudline A6
#define FAN_OFF 0.0f
#define FAN_LOW 0.3f      // 30% speed
#define FAN_MEDIUM 0.6f   // 60% speed  
#define FAN_HIGH 1.0f     // 100% speed

// Control mode selection
#define CONTROL_MODE_DIGITAL 1   // Thermador sends discrete on/off signals
#define CONTROL_MODE_ANALOG 2    // Thermador sends variable voltage
#define CONTROL_MODE CONTROL_MODE_DIGITAL  // Change this based on your setup

// Global variables
float currentFanSpeed = 0.0f;
int currentSpeedLevel = 0;  // 0=off, 1=low, 2=medium, 3=high
unsigned long lastSpeedChange = 0;
volatile unsigned long tachPulseCount = 0;
float currentRPM = 0;

// Debouncing variables
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;  // 50ms debounce
int lastSpeedReading = 0;

void setup() {
  Serial.begin(9600);
  Serial.println("Thermador UCVM36XS to Cloudline A6 Translator");
  Serial.println("==============================================");
  
  // Initialize output pins
  pinMode(CLOUDLINE_PWM_PIN, OUTPUT);
  pinMode(STATUS_LED_PIN, OUTPUT);
  
  // Initialize input pins based on control mode
  if (CONTROL_MODE == CONTROL_MODE_DIGITAL) {
    pinMode(THERMADOR_SPEED1_PIN, INPUT_PULLUP);
    pinMode(THERMADOR_SPEED2_PIN, INPUT_PULLUP);
    pinMode(THERMADOR_SPEED3_PIN, INPUT_PULLUP);
    Serial.println("Digital control mode - monitoring discrete speed inputs");
  } else {
    pinMode(THERMADOR_ANALOG_PIN, INPUT);
    Serial.println("Analog control mode - monitoring variable voltage input");
  }
  
  // Setup tachometer (optional)
  pinMode(CLOUDLINE_TACH_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(CLOUDLINE_TACH_PIN), tachISR, FALLING);
  
  // Setup PWM for Cloudline A6 control
  setupCloudlinePWM();
  
  // Start with fan off
  setCloudlineSpeed(FAN_OFF);
  
  Serial.println("Translator ready!");
  Serial.println("Commands: 'off', 'low', 'medium', 'high', 'status', 'test'");
  printConnectionDiagram();
}

void loop() {
  static unsigned long lastUpdate = 0;
  static unsigned long lastStatusPrint = 0;
  
  // Check Thermador inputs every 50ms
  if (millis() - lastUpdate > 50) {
    lastUpdate = millis();
    
    int newSpeedLevel = readThermadorSpeed();
    
    // Only change speed if different and debounced
    if (newSpeedLevel != currentSpeedLevel) {
      if (millis() - lastSpeedChange > debounceDelay) {
        currentSpeedLevel = newSpeedLevel;
        updateCloudlineSpeed();
        lastSpeedChange = millis();
        
        Serial.print("Speed changed to: ");
        Serial.println(getSpeedName(currentSpeedLevel));
      }
    }
    
    // Update RPM calculation
    updateRPM();
  }
  
  // Print status every 5 seconds
  if (millis() - lastStatusPrint > 5000) {
    lastStatusPrint = millis();
    printStatus();
  }
  
  // Handle manual serial commands for testing
  handleSerialCommands();
  
  // Update status LED (blink rate indicates speed level)
  updateStatusLED();
}

void setupCloudlinePWM() {
  // Configure Timer 1 for 25kHz PWM - optimal for Cloudline A6
  TCCR1A = (1 << COM1A1) | (1 << WGM11);
  TCCR1B = (1 << CS10) | (1 << WGM13);
  ICR1 = 320;  // 25kHz PWM frequency
  OCR1A = 0;   // Start at 0% duty cycle
}

int readThermadorSpeed() {
  if (CONTROL_MODE == CONTROL_MODE_DIGITAL) {
    // Read discrete speed inputs
    bool speed1 = !digitalRead(THERMADOR_SPEED1_PIN);  // Assuming active low
    bool speed2 = !digitalRead(THERMADOR_SPEED2_PIN);
    bool speed3 = !digitalRead(THERMADOR_SPEED3_PIN);
    
    // Determine speed level based on which inputs are active
    if (speed3) return 3;       // High speed
    else if (speed2) return 2;  // Medium speed  
    else if (speed1) return 1;  // Low speed
    else return 0;              // Off
    
  } else {
    // Read analog voltage and map to speed levels
    int analogValue = analogRead(THERMADOR_ANALOG_PIN);
    
    // Map analog reading to speed levels
    // Adjust these thresholds based on your Thermador's output voltages
    if (analogValue < 100) return 0;        // Off (0-0.5V)
    else if (analogValue < 300) return 1;   // Low (0.5-1.5V)
    else if (analogValue < 600) return 2;   // Medium (1.5-3V)
    else return 3;                          // High (3V+)
  }
}

void updateCloudlineSpeed() {
  float targetSpeed;
  
  switch (currentSpeedLevel) {
    case 0: targetSpeed = FAN_OFF; break;
    case 1: targetSpeed = FAN_LOW; break;
    case 2: targetSpeed = FAN_MEDIUM; break;
    case 3: targetSpeed = FAN_HIGH; break;
    default: targetSpeed = FAN_OFF; break;
  }
  
  setCloudlineSpeed(targetSpeed);
}

void setCloudlineSpeed(float speed) {
  // Clamp speed to valid range
  speed = constrain(speed, 0.0f, 1.0f);
  currentFanSpeed = speed;
  
  if (speed == 0) {
    OCR1A = 0;  // Fan off
  } else {
    // Cloudline A6 works well with 20-100% duty cycle
    float minDuty = 0.20f;  // 20% minimum for reliable operation
    float scaledSpeed = minDuty + speed * (1.0f - minDuty);
    OCR1A = (uint16_t)(320 * scaledSpeed);
  }
}

void tachISR() {
  tachPulseCount++;
}

void updateRPM() {
  static unsigned long lastRPMUpdate = 0;
  static unsigned long lastPulseCount = 0;
  
  if (millis() - lastRPMUpdate >= 1000) {  // Calculate every second
    unsigned long pulsesSinceLastUpdate = tachPulseCount - lastPulseCount;
    currentRPM = (pulsesSinceLastUpdate * 60.0) / 2.0;  // 2 pulses per revolution
    
    lastPulseCount = tachPulseCount;
    lastRPMUpdate = millis();
  }
}

void updateStatusLED() {
  // Blink pattern indicates current speed level
  int blinkInterval;
  switch (currentSpeedLevel) {
    case 0: blinkInterval = 2000; break;  // Slow blink = off
    case 1: blinkInterval = 1000; break;  // Medium blink = low
    case 2: blinkInterval = 500; break;   // Fast blink = medium  
    case 3: blinkInterval = 200; break;   // Very fast blink = high
    default: blinkInterval = 2000; break;
  }
  
  digitalWrite(STATUS_LED_PIN, (millis() / blinkInterval) % 2);
}

void handleSerialCommands() {
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    command.toLowerCase();
    
    if (command == "off") {
      currentSpeedLevel = 0;
      updateCloudlineSpeed();
      Serial.println("Manual: Fan OFF");
    }
    else if (command == "low") {
      currentSpeedLevel = 1;
      updateCloudlineSpeed();
      Serial.println("Manual: Fan LOW");
    }
    else if (command == "medium") {
      currentSpeedLevel = 2;
      updateCloudlineSpeed();
      Serial.println("Manual: Fan MEDIUM");
    }
    else if (command == "high") {
      currentSpeedLevel = 3;
      updateCloudlineSpeed();
      Serial.println("Manual: Fan HIGH");
    }
    else if (command == "status") {
      printDetailedStatus();
    }
    else if (command == "test") {
      runSpeedTest();
    }
    else if (command == "help") {
      printHelp();
    }
    else {
      Serial.println("Unknown command. Available: off, low, medium, high, status, test, help");
    }
  }
}

void printStatus() {
  Serial.print("Thermador: " + getSpeedName(currentSpeedLevel));
  Serial.print(" | Cloudline: " + String(currentFanSpeed * 100, 0) + "%");
  Serial.print(" | RPM: " + String(currentRPM, 0));
  Serial.println();
}

void printDetailedStatus() {
  Serial.println("=== Fan Translator Status ===");
  Serial.println("Thermador Speed: " + getSpeedName(currentSpeedLevel));
  Serial.println("Cloudline Speed: " + String(currentFanSpeed * 100, 1) + "%");
  Serial.println("Cloudline RPM: " + String(currentRPM, 0));
  Serial.println("Uptime: " + String(millis() / 1000) + " seconds");
  
  if (CONTROL_MODE == CONTROL_MODE_DIGITAL) {
    Serial.println("Input Pins - S1:" + String(!digitalRead(THERMADOR_SPEED1_PIN)) + 
                   " S2:" + String(!digitalRead(THERMADOR_SPEED2_PIN)) + 
                   " S3:" + String(!digitalRead(THERMADOR_SPEED3_PIN)));
  } else {
    Serial.println("Analog Input: " + String(analogRead(THERMADOR_ANALOG_PIN)) + " (0-1023)");
  }
  Serial.println("");
}

String getSpeedName(int level) {
  switch (level) {
    case 0: return "OFF";
    case 1: return "LOW";
    case 2: return "MEDIUM";
    case 3: return "HIGH";
    default: return "UNKNOWN";
  }
}

void runSpeedTest() {
  Serial.println("Running Cloudline A6 speed test...");
  
  Serial.println("Testing OFF...");
  setCloudlineSpeed(FAN_OFF);
  delay(2000);
  
  Serial.println("Testing LOW (30%)...");
  setCloudlineSpeed(FAN_LOW);
  delay(3000);
  
  Serial.println("Testing MEDIUM (60%)...");
  setCloudlineSpeed(FAN_MEDIUM);
  delay(3000);
  
  Serial.println("Testing HIGH (100%)...");
  setCloudlineSpeed(FAN_HIGH);
  delay(3000);
  
  Serial.println("Returning to OFF...");
  setCloudlineSpeed(FAN_OFF);
  currentSpeedLevel = 0;
  
  Serial.println("Speed test complete!");
}

void printHelp() {
  Serial.println("=== Available Commands ===");
  Serial.println("off     - Turn fan off");
  Serial.println("low     - Set low speed");
  Serial.println("medium  - Set medium speed");
  Serial.println("high    - Set high speed");
  Serial.println("status  - Show detailed status");
  Serial.println("test    - Run speed test sequence");
  Serial.println("help    - Show this help");
  Serial.println("");
}

void printConnectionDiagram() {
  Serial.println("\n=== Connection Diagram ===");
  Serial.println("THERMADOR UCVM36XS → ARDUINO NANO → CLOUDLINE A6");
  Serial.println("");
  Serial.println("Thermador Connections:");
  if (CONTROL_MODE == CONTROL_MODE_DIGITAL) {
    Serial.println("  Speed 1 signal → Pin 3");
    Serial.println("  Speed 2 signal → Pin 4"); 
    Serial.println("  Speed 3 signal → Pin 5");
    Serial.println("  Common/Ground → Arduino GND");
  } else {
    Serial.println("  Variable speed signal → Pin A0");
    Serial.println("  Ground → Arduino GND");
  }
  Serial.println("");
  Serial.println("Cloudline A6 Connections:");
  Serial.println("  PWM Control → Pin 9");
  Serial.println("  Tachometer → Pin 2 (optional)");
  Serial.println("  Power: 12V+ → Arduino VIN & Fan +12V");
  Serial.println("  Ground → Arduino GND & Fan GND");
  Serial.println("===============================\n");
}