// the uart used to control servos.
// GPIO 18 - S_RXD, GPIO 19 - S_TXD, as default.
#define S_RXD 9
#define S_TXD 8
#include <SCServo.h>
SMS_STS st;

// Servo limits: {min, max, home}
const int SERVO_MIN[]  = {250,  820,  735, 1};
const int SERVO_MAX[]  = {2200, 2200, 2500, 2500};
const int SERVO_HOME[] = {400,  1950, 735,  1200};
const int NUM_SERVOS = 4;

// Demo mode
bool demoMode = true;
unsigned long demoTimer = 0;
bool demoAtTarget = false; // false = moving to target, true = at target waiting

// Demo target positions (servo IDs 1-indexed)
const int DEMO_POS_1 = 1300;
const int DEMO_POS_3 = 2500;
const int DEMO_POS_4 = 2200;

const unsigned long DEMO_INTERVAL = 10000; // 10 seconds

void printMenu() {
  Serial.println();
  Serial.println("===== Servo Control =====");
  for (int i = 0; i < NUM_SERVOS; i++) {
    int pos = st.ReadPos(i + 1);
    Serial.print("  Servo "); Serial.print(i + 1);
    Serial.print(" | pos: "); Serial.print(pos);
    Serial.print(" | range: "); Serial.print(SERVO_MIN[i]);
    Serial.print("-"); Serial.print(SERVO_MAX[i]);
    Serial.print(" | home: "); Serial.println(SERVO_HOME[i]);
  }
  Serial.println();
  Serial.println("Commands:");
  Serial.println("  <servo> <position>   e.g. '1 1000'");
  Serial.println("  home                 all servos to home");
  Serial.println("  home <servo>         one servo to home");
  Serial.println("  read                 read all positions");
  Serial.println("  demo                 start demo mode");
  Serial.println("=========================");
  Serial.print("> ");
}

void moveServo(int id, int pos, int speed = 1000, int acc = 50) {
  if (id < 1 || id > NUM_SERVOS) {
    Serial.println("Error: servo ID must be 1-4");
    return;
  }
  int idx = id - 1;
  if (pos < SERVO_MIN[idx] || pos > SERVO_MAX[idx]) {
    Serial.print("Error: position out of range for servo ");
    Serial.print(id);
    Serial.print(". Allowed: ");
    Serial.print(SERVO_MIN[idx]);
    Serial.print("-");
    Serial.println(SERVO_MAX[idx]);
    return;
  }
  st.WritePosEx(id, pos, speed, acc);
  Serial.print("Servo "); Serial.print(id);
  Serial.print(" -> "); Serial.println(pos);
}

void demoMoveToTarget() {
  Serial.println("[Demo] Moving to target position...");
  st.WritePosEx(1, DEMO_POS_1, 1000, 50);
  st.WritePosEx(3, DEMO_POS_3, 1000, 50);
  st.WritePosEx(4, DEMO_POS_4, 1000, 50);
}

void demoMoveToHome() {
  Serial.println("[Demo] Moving to home position...");
  for (int i = 0; i < NUM_SERVOS; i++) {
    st.WritePosEx(i + 1, SERVO_HOME[i], 1000, 50);
  }
}

void startDemo() {
  demoMode = true;
  demoAtTarget = false;
  Serial.println("[Demo] Demo mode started. Type 'dev' to exit to dev mode.");
  demoMoveToTarget();
  demoTimer = millis();
}

void setup()
{
  Serial.begin(115200);
  Serial1.begin(1000000, SERIAL_8N1, S_RXD, S_TXD);
  st.pSerial = &Serial1;
  delay(1000);

  Serial.println("Servo Control Ready!");
  Serial.println("Moving all servos to home positions...");

  // Move all servos to home on startup
  for (int i = 0; i < NUM_SERVOS; i++) {
    st.WritePosEx(i + 1, SERVO_HOME[i], 1000, 50);
  }
  delay(2000);

  // Start in demo mode
  startDemo();
}

void loop()
{
  // Check for serial input (works in both modes)
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();

    if (input.equalsIgnoreCase("dev")) {
      if (demoMode) {
        demoMode = false;
        Serial.println("[Demo] Demo mode stopped. Entering dev mode.");
        printMenu();
      }
      return;
    }

    if (input.equalsIgnoreCase("demo")) {
      startDemo();
      return;
    }

    // Only process other commands in dev mode
    if (!demoMode) {
      if (input.length() == 0) {
        printMenu();
        return;
      }

      if (input.equalsIgnoreCase("home")) {
        Serial.println("All servos -> home");
        for (int i = 0; i < NUM_SERVOS; i++) {
          st.WritePosEx(i + 1, SERVO_HOME[i], 1000, 50);
        }
      }
      else if (input.startsWith("home ") || input.startsWith("HOME ")) {
        int id = input.substring(5).toInt();
        if (id >= 1 && id <= NUM_SERVOS) {
          moveServo(id, SERVO_HOME[id - 1]);
        } else {
          Serial.println("Error: servo ID must be 1-4");
        }
      }
      else if (input.equalsIgnoreCase("read")) {
        for (int i = 0; i < NUM_SERVOS; i++) {
          int pos = st.ReadPos(i + 1);
          Serial.print("Servo "); Serial.print(i + 1);
          Serial.print(" pos: "); Serial.println(pos);
        }
      }
      else {
        // Parse "<servo> <position>"
        int spaceIdx = input.indexOf(' ');
        if (spaceIdx > 0) {
          int id = input.substring(0, spaceIdx).toInt();
          int pos = input.substring(spaceIdx + 1).toInt();
          moveServo(id, pos);
        } else {
          Serial.println("Unknown command. Press Enter for menu.");
        }
      }

      Serial.print("> ");
    }
  }

  // Demo mode loop
  if (demoMode && (millis() - demoTimer >= DEMO_INTERVAL)) {
    if (!demoAtTarget) {
      // Was at target, now go home
      demoMoveToHome();
      demoAtTarget = true;
    } else {
      // Was at home, now go to target
      demoMoveToTarget();
      demoAtTarget = false;
    }
    demoTimer = millis();
  }
}
