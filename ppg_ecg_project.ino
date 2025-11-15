#include <Wire.h>
#include "max86150.h"
#include "LiquidCrystal_I2C.h"

// Pin definitions for AD8232
#define ECG_PIN A0
#define MAX86150_ADDRESS 0x5E
#define LCD_ADDRESS 0x27

// Flags for signal processing
bool ecgflag = false; 
bool ppgflag = false; 

// MAX86150 object
MAX86150 max86150;

// LCD object
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Buzzer pin
const int buzzer = 9;

// Variables for ECG and PPG signal processing
volatile unsigned long ecgTime = 0;  // Time of R-wave detection
volatile unsigned long irTime = 0;   // Time of highest IR peak detection
volatile float ptt = 0;              // Pulse Transit Time in milliseconds

// Constants for blood pressure calculation (calibrated values)
const float A_sys = 90;    // Adjusted for Systolic BP
const float B_sys = 7000;  // Adjusted for Systolic BP
const float A_dia = 60;    // Adjusted for Diastolic BP
const float B_dia = 5000;  // Adjusted for Diastolic BP

// Constants for ECG and PPG thresholds
#define ECG_THRESHOLD 650     // Threshold to detect R-wave
#define IR_THRESHOLD 20000    // Minimum threshold for valid IR peaks

void setup() {
  Serial.begin(115200);

  // Initialize ECG (AD8232)
  pinMode(ECG_PIN, INPUT);
  pinMode(buzzer, OUTPUT);

  // Initialize LCD
  Wire.begin();
  lcd.init();
  lcd.backlight();
  lcd.clear();

  // Initialize MAX86150
  if (!max86150.begin()) {
    Serial.println(F("MAX86150 initialization failed!"));
    while (1);
  }
  max86150.setup();  // Setup default configuration
  Serial.println(F("System initialized. Waiting for signals..."));
}

void processBloodPressure(float ptt) {
  if (ptt <= 0) {  // Prevent division by zero or invalid PTT
    Serial.println(F("Invalid PTT value. Skipping BP calculation."));
    return;
  }
  
  // Calculate Blood Pressure
  int systolicBP = A_sys + (B_sys / ptt);
  int diastolicBP = A_dia + (B_dia / ptt);

  // Debug: Print to Serial Monitor
  Serial.print(F("Systolic BP: "));
  Serial.println(systolicBP);
  Serial.print(F("Diastolic BP: "));
  Serial.println(diastolicBP);


  // Display Blood Pressure on LCD
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Sy: ");
  lcd.print(systolicBP);  // Display with one decimal point
  lcd.setCursor(0, 1);
  lcd.print("Di: ");
  lcd.print(diastolicBP);  // Display with one decimal point
 
  delay(2000);  // Wait 2 seconds before updating with BP detection status
  lcd.clear();
  // BP detection (High/Low)
  if (systolicBP > 130 || diastolicBP > 80) {  // High BP
    tone(buzzer, 1000, 1000);  // Buzzer sound
    delay(500);                // Delay for tone
    noTone(buzzer);            // Stop tone
    lcd.setCursor(0, 0);
    lcd.print("High BP Detected  ");
  } else if (systolicBP < 90 || diastolicBP < 60) {  // Low BP
    tone(buzzer, 500, 500);   // Buzzer sound
    delay(500);               // Delay for tone
    noTone(buzzer);           // Stop tone
    lcd.setCursor(0, 0);
    lcd.print("Low BP Detected   ");
  } else {  // Normal BP
    noTone(buzzer);
    lcd.setCursor(0, 0);
    lcd.print("Normal BP         ");
  }
  delay(2000);  // Display BP for 2 seconds before updating again
}

void loop() {
  // Step 1: ECG Signal Processing
  int ecgValue = analogRead(ECG_PIN);  // Read ECG value
  Serial.print(F("ECG : "));
  Serial.println(ecgValue);

  // Detect R-wave (value exceeds threshold)
  if (ecgValue > ECG_THRESHOLD && !ecgflag) {
    ecgTime = millis();  // Store timestamp for R-wave detection
    Serial.print(F("R-Peak Value: "));
    Serial.println(ecgValue);
    Serial.print(F("ECG R-wave Detected at: "));
    Serial.println(ecgTime);
    ecgflag = true;  // Mark R-wave detected
    ppgflag = false; // Reset IR peak flag for this window
    irTime = 0;      // Reset IR time for new detection window
  }

  // Step 2: PPG Signal Processing (IR peak detection within 1000 ms)
  if (ecgflag && !ppgflag) {
    unsigned long startTime = millis(); 
    unsigned long highestIRTime = 0;  
    long highestIRValue = 0;          

    while (millis() - startTime <= 1000) { 
      if (max86150.check() > 0) {  // Ensure new PPG data is available
        long currentIRValue = max86150.getIR();  // Read IR value
        //Serial.print(F("IR : "));
        Serial.println(currentIRValue);

        if (currentIRValue > IR_THRESHOLD && currentIRValue > highestIRValue) {
          highestIRValue = currentIRValue; // Update highest IR value
          highestIRTime = millis();       // Store timestamp of highest IR value
        }
      }
    }

    if (highestIRValue > 0) {  // If a valid IR peak is detected
      irTime = highestIRTime;
      ppgflag = true;  // Mark IR peak detected
      Serial.print(F("IR-Peak Value: "));
      Serial.println(highestIRValue);
      Serial.print(F("IR Peak Detected at: "));
      Serial.println(irTime);
    }
  }

  // Step 3: Calculate PTT after IR peak is detected
  if (ecgflag && ppgflag) {
    ptt = irTime - ecgTime;  // Calculate PTT in milliseconds
    Serial.print(F("PTT: "));
    Serial.println(ptt);

    // Display ECG, IR, and PTT values on LCD
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("ECG R: ");
    lcd.print(ecgValue);
    lcd.setCursor(0, 1);
    lcd.print("PTT: ");
    lcd.print(ptt);

    delay(1000);  // Display for 1 second


    // Reset flags for next cycle
    ecgflag = false;
    ppgflag = false;

    // Process Blood Pressure
    processBloodPressure(ptt);
  }

  delay(100);  // Short delay for smoother updates
}
