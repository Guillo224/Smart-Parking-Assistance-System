/*

Smart Parking Assistance System

The system measures the distance between an object and the sensor and provides real-time 
feedback through visual auditory by integrating multiple components such as an LCD, speaker,
and LEDs, the system simulates a real parking aid mechanism.

*/

#include <LiquidCrystal.h> // Library for LCD

// Initialize LCD: RS, E, D4, D5, D6, D7
LiquidCrystal lcd(12, 11, 5, 4, 3, A0);

// Initialize HC-SR04 Ultrasonic Sensor
const int TRIG_PIN = 9;
const int ECHO_PIN = 10;

// LEDs
const int GREEN_LED = 6;
const int YELLOW_LED = 7;
const int RED_LED = A1;

// Speaker
const int SPEAKER_PIN = 8;

// Button
const int BUTTON_PIN = 2;

// Zones
const int SAFE_DISTANCE = 50;
const int DANGER_DISTANCE = 20;

// Timing speaker and LED blink — shared flag keeps them in sync
unsigned long previousMillis = 0;
bool speakerState = false; // Shared state: drives both speaker and blinking LEDs

// Noise filter — rolling array of 5 distance samples
const int NUM_SAMPLES = 5;
int samples[NUM_SAMPLES] = {0, 0, 0, 0, 0}; // Circular buffer to store readings
int sampleIndex = 0; // Tracks which slot to write next

// State Machine 
enum Zone { SAFE, CAUTION, DANGER }; // 0 = SAFE, 1 = CAUTION, 2 = DANGER

// Current zone
Zone currentZone = SAFE;

volatile bool systemActive = false; // System starts OFF
volatile bool justStarted = false;  // Flag to trigger startup sequence

// ISR: toggles system ON/OFF
void toggleSystem() {

  // If system is active, invert status and reset flags to mark that it just started
  if (!systemActive) {
    systemActive = true;
    justStarted = true;

  // Inactive
  } else {
    systemActive = false;
  }
}

void setup() {

  // Configure LCD
  lcd.begin(16, 2);

  // Configure components
  pinMode(GREEN_LED, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(SPEAKER_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // Configure button with interrupt
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), toggleSystem, FALLING);
  
  // Ensure all LEDs and speaker start OFF
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(YELLOW_LED, LOW);
  digitalWrite(RED_LED, LOW);
  noTone(SPEAKER_PIN);

  // Establish serial communication
  Serial.begin(9600);

}

void loop() {

  // If system is OFF, show standby message and wait
  if (!systemActive) {

    // Turn off all LEDs when system is OFF
    digitalWrite(GREEN_LED, LOW);
    digitalWrite(YELLOW_LED, LOW);
    digitalWrite(RED_LED, LOW);
    noTone(SPEAKER_PIN);

    // Display initial message 
    lcd.setCursor(0, 0);
    lcd.print("Press button to ");
    lcd.setCursor(0, 1);
    lcd.print("start system    ");
    delay(100);
    return;
  }

  // Runs once when system first starts
  if (justStarted) {
    justStarted = false;
    
    // Initializing animation
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Initializing.");
    delay(700);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Initializing..");
    delay(700);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Initializing...");
    delay(700);
    lcd.clear();

    // Splash screen
    lcd.setCursor(0, 0);
    lcd.print("Smart Parking");
    lcd.setCursor(0, 1);
    lcd.print("System");
    delay(2000);
    lcd.clear();
  }

  // Clean TRIG pin before starting to avoid noise in the signal
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2); // Wait 2 µs

  // Send a sound pulse exactly for 10 µs
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10); 
  digitalWrite(TRIG_PIN, LOW); // Turn the sound pulse OFF

  // Measure how much time the ECHO takes to come back
  long duration = pulseIn(ECHO_PIN, HIGH); // Wait until ECHO turns HIGH and measures how many microseconds lasts

  // Convert the time into centimeters (cm)
  int rawDistance = duration * 0.034 / 2;

  // Store raw reading into the circular sample buffer
  samples[sampleIndex] = rawDistance;
  sampleIndex = (sampleIndex + 1) % NUM_SAMPLES; // Advance index and wrap around after slot 4

  // Compute the average of all 5 samples to filter out noise
  int total = 0;
  for (int i = 0; i < NUM_SAMPLES; i++) {
    total += samples[i];
  }
  int distance = total / NUM_SAMPLES; // Averaged, noise-filtered distance

  // Process only if the distance readings are valid
  if (distance > 0) {

    // Call working functions and pass the distance value as an argument
    updateState(distance);
    updateSpeaker();  // Called before updateLEDs so speakerState is updated first
    updateLEDs();
    updateLCD(distance);

    // Display readings and zone indicators in Serial Monitor
    Serial.print("Distance: ");
    Serial.print(distance);
    Serial.print(" cm | Zone: ");
    switch(currentZone) {
      case SAFE:
        Serial.print("SAFE");
        break;
      
      case CAUTION:
        Serial.print("CAUTION");
        break;

      case DANGER:
        Serial.print("DANGER");
        break;
    }
    Serial.println();

  }

  delay(100);

}

// 1. Update state machine
void updateState(int distance) {

  // Update only if distance is a valid value
  if (distance > 0) {

    switch (currentZone) {
    
    // If state is SAFE and an object is close to 50 cm or less, switch to CAUTION
    case SAFE:
      if (distance <= SAFE_DISTANCE)  currentZone = CAUTION;
      break;

    // If state is CAUTION and an object moves more than 50 cm, switch to SAFE
    // or if the object is close to 20 cm or less, switch to DANGER
    case CAUTION:
      if (distance > SAFE_DISTANCE) currentZone = SAFE;
      if (distance <= DANGER_DISTANCE) currentZone = DANGER;
      break;

    // If state is DANGER and an object moves more than 20 cm away, switch to CAUTION
    case DANGER:
      if (distance > DANGER_DISTANCE) currentZone = CAUTION;
      break;
    }

  }
}

// 2. Control LEDs
// Blinking LEDs share speakerState so their timing stays perfectly in sync with the beeps
void updateLEDs() {

  switch (currentZone) {

    // If state is SAFE, only Green LED is ON solid
    case SAFE:
      digitalWrite(GREEN_LED, HIGH);
      digitalWrite(YELLOW_LED, LOW);
      digitalWrite(RED_LED, LOW);
      break;

    // If state is CAUTION, Yellow LED blinks at the same 500 ms rate as the speaker
    case CAUTION:
      digitalWrite(GREEN_LED, LOW);
      digitalWrite(YELLOW_LED, speakerState ? HIGH : LOW); // Follows speaker toggle flag
      digitalWrite(RED_LED, LOW);
      break;

    // If state is DANGER, Red LED blinks at the same 100 ms rate as the speaker
    case DANGER:
      digitalWrite(GREEN_LED, LOW);
      digitalWrite(YELLOW_LED, LOW);
      digitalWrite(RED_LED, speakerState ? HIGH : LOW); // Follows speaker toggle flag
      break;
  }

}

// 3. Update LCD
void updateLCD(int distance) {

  // Overwrite row 1 without lcd.clear() to avoid flickering
  lcd.setCursor(0, 0);

  // Display current state in the LCD — padded to 16 chars to clear leftover characters
  switch(currentZone) {
    case SAFE:
      lcd.print("SAFE            ");
      break;

    case CAUTION:
      lcd.print("CAUTION         ");
      break;
    
    case DANGER:
      lcd.print("DANGER!         ");
      break;
  }

  // Overwrite row 2 with distance — padded with spaces to clear leftover characters
  lcd.setCursor(0, 1);
  lcd.print("Dist: ");
  lcd.print(distance);
  lcd.print(" cm      ");
  
}

// 4. Control speaker
void updateSpeaker() {

  // Capture the current time since the Arduino started
  unsigned long currentMillis = millis();

  // Speaker sounds depending on the active state
  switch (currentZone) {

    case SAFE:
      // Speaker is OFF when SAFE zone is active
      noTone(SPEAKER_PIN);

      speakerState = false; // Reset shared flag
      previousMillis = currentMillis; // Store the previous time as the current
      break;

    case CAUTION:
      // Perform the following if enough time has passed (500ms)
      if (currentMillis - previousMillis >= 500) {
        previousMillis = currentMillis; // Store the actual time as a new reference point
        speakerState = !speakerState;  // Invert the shared state flag

        // Create intermittent beep effect when CAUTION is active
        if (speakerState) {
          tone(SPEAKER_PIN, 800); // Speaker makes a beeping sound (800 Hz) if true
        } else {
          noTone(SPEAKER_PIN); // Speaker makes no sound if false
        }
      }
      break;

    case DANGER:
      // Perform the following if enough time has passed (100ms)
      if (currentMillis - previousMillis >= 100) {
        previousMillis = currentMillis; // Store the actual time as a new reference point
        speakerState = !speakerState;  // Invert the shared state flag

        // Create intermittent beep effect when DANGER is active
        if (speakerState) {
          tone(SPEAKER_PIN, 2000); // Speaker makes a beeping sound (2000 Hz) if true
        } else {
          noTone(SPEAKER_PIN); // Speaker makes no sound if false
        }
      }
      break;
      
  }
}

