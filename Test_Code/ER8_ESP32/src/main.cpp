#include <AlfredoCRSF.h> //CRSF staat voor Crossfire, een protocol voor communicatie tussen zenders en ontvangers in de RC-wereld.
#include <HardwareSerial.h>
#include <Arduino.h>
#include <ESP32Servo.h>

#define ER8_RX_PIN 16 
#define ER8_TX_PIN 17

#define ESC1_PIN 32    // ESC 1 PWM pin
#define ESC2_PIN 33    // ESC 2 PWM pin
#define ESC3_PIN 26   // ESC 3 PWM pin
#define ESC4_PIN 25  // ESC 4 PWM pin


// --- LINKER STICK ---
// LL = Linker stick naar Links, UL = Linker stick naar Boven, etc.
#define LL_LED_PIN 2   // (Links)
#define LR_LED_PIN 15  // (Yaw Rechts)
#define UL_LED_PIN 22  // (Gas Omhoog)
#define DL_LED_PIN 19  // (Gas Omlaag)

// --- RECHTER STICK (---
// RL = Rechter stick naar Links, UR = Rechter stick naar Boven, etc.
#define RL_LED_PIN 5   // (Roll Links)
#define RR_LED_PIN 18  // (Roll Rechts)
#define UR_LED_PIN 23  // (Pitch Omhoog)
#define DR_LED_PIN 21  // (Pitch Omlaag)

// Initialiseer HardwareSerial op UART2
HardwareSerial crsfSerial(2); 
AlfredoCRSF crsf;

// ESC servo objects
Servo esc1;
Servo esc2;
Servo esc3;
Servo esc4;

void setup() {
 
  Serial.begin(115200);
  
  // Start verbinding met ontvanger
  crsfSerial.begin(420000, SERIAL_8N1, ER8_RX_PIN, ER8_TX_PIN);
  crsf.begin(crsfSerial);
  
  // Pinnen instellen als OUTPUT
  pinMode(LL_LED_PIN, OUTPUT);
  pinMode(LR_LED_PIN, OUTPUT);
  pinMode(UL_LED_PIN, OUTPUT);
  pinMode(DL_LED_PIN, OUTPUT);
  
  pinMode(RL_LED_PIN, OUTPUT);
  pinMode(RR_LED_PIN, OUTPUT);
  pinMode(UR_LED_PIN, OUTPUT);
  pinMode(DR_LED_PIN, OUTPUT);
  
  // Initialize ESCs with servo library
  esc1.setPeriodHertz(50);      // Standard servo/ESC frequency (50Hz)
  esc1.attach(ESC1_PIN, 1000, 2000);  // Attach to pin with 1000-2000µs range
  
  esc2.setPeriodHertz(50);      // Standard servo/ESC frequency (50Hz)
  esc2.attach(ESC2_PIN, 1000, 2000);  // Attach to pin with 1000-2000µs range

  esc3.setPeriodHertz(50);      // Standard servo/ESC frequency (50Hz)
  esc3.attach(ESC3_PIN, 1000, 2000);  //

  esc4.setPeriodHertz(50);      // Standard servo/ESC frequency (50Hz)
  esc4.attach(ESC4_PIN, 1000, 2000);  //

  
  // Initialize ESCs to low throttle
  esc1.writeMicroseconds(1000);
  esc2.writeMicroseconds(1000);
  esc3.writeMicroseconds(1000);
  esc4.writeMicroseconds(1000);

  Serial.println("ESP32 gestart. Wacht op input van Radiomaster...");
}

void loop() {
  // Update de CRSF data elke keer
  crsf.update();

  static unsigned long lastPrint = 0;
  

  if (millis() - lastPrint > 50) {
    
   
    int ch1 = crsf.getChannel(1); // Roll  (Rechter Stick L/R)
    int ch2 = crsf.getChannel(2); // Pitch (Rechter Stick Op/Neer)
    int ch3 = crsf.getChannel(3); // Gas   (Linker Stick Op/Neer)
    int ch4 = crsf.getChannel(4); // Yaw   (Linker Stick L/R)
    int aux1 = crsf.getChannel(5); // Eventuele schakelaar
    int aux2 = crsf.getChannel(6); // Eventuele schakelaar

    // --- LED FEEDBACK & ESC CONTROL ---

    // Links / Rechts
    if (ch1 < 1400) {
      digitalWrite(RL_LED_PIN, HIGH);
      digitalWrite(RR_LED_PIN, LOW);

      esc1.writeMicroseconds(1200);  // ESC1 value for Roll Left
    } else if (ch1 > 1600) {
      digitalWrite(RL_LED_PIN, LOW);
      digitalWrite(RR_LED_PIN, HIGH);

      esc1.writeMicroseconds(1800);  // ESC1 value for Roll Right
    } else {
      digitalWrite(RL_LED_PIN, LOW);
      digitalWrite(RR_LED_PIN, LOW);
      esc1.writeMicroseconds(1500);  // ESC1 neutral
    }

    // Omhoog / Omlaag
    if (ch2 < 1400) {
      digitalWrite(UR_LED_PIN, HIGH);
      digitalWrite(DR_LED_PIN, LOW);

      esc2.writeMicroseconds(1200);  // ESC2 value for Pitch Up
    } else if (ch2 > 1600) {
      digitalWrite(UR_LED_PIN, LOW);
      digitalWrite(DR_LED_PIN, HIGH);

      esc2.writeMicroseconds(1800);  // ESC2 value for Pitch Down
    } else {
      digitalWrite(UR_LED_PIN, LOW);
      digitalWrite(DR_LED_PIN, LOW);

      esc2.writeMicroseconds(1500);  // ESC2 neutral
    }

    // --- LOGICA LINKER STICK (Gas & Yaw) ---

    // Omhoog / Omlaag (Gas)
    if (ch3 < 1400) {
      digitalWrite(DL_LED_PIN, HIGH);
      digitalWrite(UL_LED_PIN, LOW);

      esc3.writeMicroseconds(1200);  // ESC3 value for Gas Up
    } else if (ch3 > 1600) {
      digitalWrite(DL_LED_PIN, LOW);
      digitalWrite(UL_LED_PIN, HIGH);

      esc3.writeMicroseconds(1800);  // ESC3 value for Gas Down
    } else {
      digitalWrite(DL_LED_PIN, LOW);
      digitalWrite(UL_LED_PIN, LOW);

      esc3.writeMicroseconds(1500);  // ESC3 neutral
    }

    // Links / Rechts (Yaw)
    if (ch4 < 1400) {
      digitalWrite(LL_LED_PIN, HIGH);
      digitalWrite(LR_LED_PIN, LOW);

      esc4.writeMicroseconds(1200);  // ESC4 value for Yaw Left
    } else if (ch4 > 1600) {
      digitalWrite(LL_LED_PIN, LOW);
      digitalWrite(LR_LED_PIN, HIGH);

      esc4.writeMicroseconds(1800);  // ESC4 value for Yaw Right
    } else {
      digitalWrite(LL_LED_PIN, LOW);
      digitalWrite(LR_LED_PIN, LOW);

      esc4.writeMicroseconds(1500);  // ESC4 neutral
    }

    // --- SERIAL OUTPUT ---
    // Dit verschijnt in je scherm op de computer
    Serial.print("R Stick (Roll): "); Serial.print(ch1);
    Serial.print("\t R Stick (Pitch): "); Serial.print(ch2);
    Serial.print("\t L Stick (Gas): "); Serial.print(ch3);
    Serial.print("\t L Stick (Yaw): "); Serial.print(ch4);
    // Serial.print("\t AUX1: "); Serial.print(aux1);
    // Serial.print("\t AUX2: "); Serial.println(aux2);
    Serial.print("\t ESC1 PWM: "); Serial.print(esc1.readMicroseconds());
    Serial.print("\t ESC2 PWM: "); Serial.println(esc2.readMicroseconds());
    Serial.print("\t ESC3 PWM: "); Serial.print(esc3.readMicroseconds());
    Serial.print("\t ESC4 PWM: "); Serial.println(esc4.readMicroseconds());

    lastPrint = millis();
  }
}