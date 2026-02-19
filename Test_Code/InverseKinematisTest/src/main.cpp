#include <math.h>

#define S_RXD 9
#define S_TXD 8
#include <SCServo.h>
SMS_STS st;

// ===== ARM PHYSICAL DIMENSIONS (mm) =====
const float L1 = 250.0;  // Shoulder to elbow
const float L2 = 250.0;  // Elbow to wrist

const int NUM_SERVOS = 4;

// Servo position limits
const int SERVO_MIN[]  = {250,  820,  735,   1};
const int SERVO_MAX[]  = {2200, 2200, 2500, 2500};
const int SERVO_HOME[] = {400,  1950,  735,  1200};

// ===== ANGLE <-> SERVO POSITION CALIBRATION =====
// STS servos: 4096 steps = 360 degrees
const float STEPS_PER_DEG = 4096.0 / 360.0;  // ~11.378

// Joint roles:
//   Servo 1 (base, Y axis):  first arm link in vertical plane (shoulder pitch)
//   Servo 2 (shoulder, X axis): horizontal rotation (azimuth/yaw)
//   Servo 3 (elbow, Y axis):  second arm link in vertical plane (elbow pitch)
//   Servo 4 (wrist, Y axis):  wrist pitch
//
// Conversion: servo_pos = ANGLE_OFFSET + angle_deg * ANGLE_DIR * STEPS_PER_DEG
//
// ANGLE_OFFSET = servo position when joint angle = 0°
// ANGLE_DIR    = +1 or -1 to flip direction (flip if servo moves the wrong way)
//
// Calibrated assuming home pose = arm straight up:
//   Servo 1 home 400 at 90° (pointing up), DIR=-1 → offset = 400 + 90*11.378 = 1424
//   Servo 2 home 1950 at 0° (forward)              → offset = 1950
//   Servo 3 home 735 at 0° (straight)              → offset = 735
//   Servo 4 home 1200 at 0° (aligned)              → offset = 1200
//
// Angle ranges per servo (based on position limits):
//   Servo 1: -68° to 103°  (250-2200)
//   Servo 2: -99° to  22°  (820-2200)
//   Servo 3:   0° to 155°  (735-2500)
//   Servo 4:-105° to 114°  (1-2500)

const float ANGLE_OFFSET[] = {1424.0, 1950.0, 735.0, 1200.0};
const float ANGLE_DIR[]    = {-1.0,   1.0,    1.0,   1.0};

// ===== COORDINATE SYSTEM (relative to base/servo 1 joint) =====
//   X = right   (positive)
//   Y = up      (positive)
//   Z = forward (positive)
//
// Home pose (arm straight up): wrist at (0, 500, 0)

// =====================================================
//  Utility functions
// =====================================================

int angleToPos(int idx, float angleDeg) {
  return (int)(ANGLE_OFFSET[idx] + angleDeg * ANGLE_DIR[idx] * STEPS_PER_DEG);
}

float posToAngle(int idx, int pos) {
  return (float)(pos - ANGLE_OFFSET[idx]) / (ANGLE_DIR[idx] * STEPS_PER_DEG);
}

// Forward kinematics: servo positions → XYZ (mm)
void forwardKinematics(float &x, float &y, float &z) {
  int s1 = st.ReadPos(1);  // base/shoulder pitch (Y axis)
  int s2 = st.ReadPos(2);  // horizontal rotation (X axis)
  int s3 = st.ReadPos(3);  // elbow pitch (Y axis)

  float shoulderDeg = posToAngle(0, s1);  // servo 1 = arm link 1 angle
  float azimuthDeg  = posToAngle(1, s2);  // servo 2 = horizontal rotation
  float elbowDeg    = posToAngle(2, s3);  // servo 3 = arm link 2 angle

  float shoulderRad = shoulderDeg * M_PI / 180.0;
  float azimuthRad  = azimuthDeg  * M_PI / 180.0;
  float elbowRad    = elbowDeg    * M_PI / 180.0;

  // Wrist position in the arm's vertical plane (servo 1 + servo 3)
  float reach  = L1 * cos(shoulderRad) + L2 * cos(shoulderRad + elbowRad);
  float height = L1 * sin(shoulderRad) + L2 * sin(shoulderRad + elbowRad);

  // Project into 3D using servo 2 horizontal rotation
  x = reach * sin(azimuthRad);
  z = reach * cos(azimuthRad);
  y = height;
}

// Inverse kinematics: XYZ (mm) → servo positions
// wristDeg = desired wrist angle (servo 4), 0 = aligned with forearm
// Returns true if solution found and within servo limits
bool inverseKinematics(float x, float y, float z, float wristDeg,
                       int &s1, int &s2, int &s3, int &s4) {

  // --- Horizontal rotation (servo 2, X axis) ---
  float azimuthDeg = atan2(x, z) * 180.0 / M_PI;
  s2 = angleToPos(1, azimuthDeg);

  // --- 2-link IK in the vertical plane (servo 1 + servo 3) ---
  float reach  = sqrt(x * x + z * z);  // horizontal distance
  float height = y;                      // vertical distance from base

  float distSq = reach * reach + height * height;
  float D = (distSq - L1 * L1 - L2 * L2) / (2.0 * L1 * L2);

  if (D < -1.0 || D > 1.0) {
    Serial.println("Error: target out of reach!");
    return false;
  }

  // Elbow angle (servo 3, elbow-down solution)
  float elbowRad = acos(D);
  float elbowDeg = elbowRad * 180.0 / M_PI;

  // Shoulder angle (servo 1, base pitch)
  float shoulderRad = atan2(height, reach) -
                      atan2(L2 * sin(elbowRad), L1 + L2 * cos(elbowRad));
  float shoulderDeg = shoulderRad * 180.0 / M_PI;

  s1 = angleToPos(0, shoulderDeg);  // servo 1 = shoulder/base pitch
  s3 = angleToPos(2, elbowDeg);     // servo 3 = elbow
  s4 = angleToPos(3, wristDeg);     // servo 4 = wrist

  // Clamp check - report which servo is out of range
  bool ok = true;
  const char* names[] = {"Base", "Shoulder", "Elbow", "Wrist"};
  int positions[] = {s1, s2, s3, s4};

  for (int i = 0; i < NUM_SERVOS; i++) {
    if (positions[i] < SERVO_MIN[i] || positions[i] > SERVO_MAX[i]) {
      Serial.print("Error: ");
      Serial.print(names[i]);
      Serial.print(" (servo ");
      Serial.print(i + 1);
      Serial.print(") position ");
      Serial.print(positions[i]);
      Serial.print(" out of range [");
      Serial.print(SERVO_MIN[i]);
      Serial.print("-");
      Serial.print(SERVO_MAX[i]);
      Serial.println("]");
      ok = false;
    }
  }
  return ok;
}

// Move a single servo with range checking
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

void printMenu() {
  Serial.println();
  Serial.println("===== Inverse Kinematics Servo Control =====");

  // Show current servo positions and angles
  for (int i = 0; i < NUM_SERVOS; i++) {
    int pos = st.ReadPos(i + 1);
    float angle = posToAngle(i, pos);
    Serial.print("  Servo "); Serial.print(i + 1);
    Serial.print(" | pos: "); Serial.print(pos);
    Serial.print(" | angle: "); Serial.print(angle, 1);
    Serial.print("° | range: "); Serial.print(SERVO_MIN[i]);
    Serial.print("-"); Serial.println(SERVO_MAX[i]);
  }

  // Show current XYZ position
  float x, y, z;
  forwardKinematics(x, y, z);
  Serial.print("  Position: X="); Serial.print(x, 1);
  Serial.print("  Y="); Serial.print(y, 1);
  Serial.print("  Z="); Serial.print(z, 1);
  Serial.println(" mm");

  Serial.println();
  Serial.println("Commands:");
  Serial.println("  goto X Y Z           move to XYZ in mm (e.g. 'goto 0 300 200')");
  Serial.println("  goto X Y Z W         ... with wrist angle W° (default 0)");
  Serial.println("  <servo> <position>   direct servo control (e.g. '1 1000')");
  Serial.println("  home                 all servos to home");
  Serial.println("  home <servo>         one servo to home");
  Serial.println("  pos                  show positions + XYZ");
  Serial.println("  read                 read raw servo positions");
  Serial.println("=============================================");
  Serial.print("> ");
}

// =====================================================
//  Setup & Loop
// =====================================================

void setup() {
  Serial.begin(115200);
  Serial1.begin(1000000, SERIAL_8N1, S_RXD, S_TXD);
  st.pSerial = &Serial1;
  delay(1000);

  Serial.println("Inverse Kinematics Servo Control Ready!");
  Serial.println("Moving all servos to home (arm up)...");

  for (int i = 0; i < NUM_SERVOS; i++) {
    st.WritePosEx(i + 1, SERVO_HOME[i], 1000, 50);
  }
  delay(2000);

  printMenu();
}

void loop() {
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();

    if (input.length() == 0) {
      printMenu();
      return;
    }

    // ---- goto X Y Z [W] ----
    if (input.startsWith("goto ") || input.startsWith("GOTO ")) {
      String args = input.substring(5);
      args.trim();

      float x, y, z, w = 0;
      int n = sscanf(args.c_str(), "%f %f %f %f", &x, &y, &z, &w);

      if (n < 3) {
        Serial.println("Usage: goto X Y Z [wristAngle]");
      } else {
        Serial.print("Target: X="); Serial.print(x, 1);
        Serial.print(" Y="); Serial.print(y, 1);
        Serial.print(" Z="); Serial.print(z, 1);
        if (n >= 4) { Serial.print(" wrist="); Serial.print(w, 1); Serial.print("°"); }
        Serial.println();

        int s1, s2, s3, s4;
        if (inverseKinematics(x, y, z, w, s1, s2, s3, s4)) {
          Serial.print("Solution: S1="); Serial.print(s1);
          Serial.print(" S2="); Serial.print(s2);
          Serial.print(" S3="); Serial.print(s3);
          Serial.print(" S4="); Serial.println(s4);

          st.WritePosEx(1, s1, 1000, 50);
          st.WritePosEx(2, s2, 1000, 50);
          st.WritePosEx(3, s3, 1000, 50);
          st.WritePosEx(4, s4, 1000, 50);

          Serial.println("Moving...");
        } else {
          Serial.println("Cannot reach target (out of range).");
        }
      }
    }
    // ---- home ----
    else if (input.equalsIgnoreCase("home")) {
      Serial.println("All servos -> home (arm up)");
      for (int i = 0; i < NUM_SERVOS; i++) {
        st.WritePosEx(i + 1, SERVO_HOME[i], 1000, 50);
      }
    }
    // ---- home <servo> ----
    else if (input.startsWith("home ") || input.startsWith("HOME ")) {
      int id = input.substring(5).toInt();
      if (id >= 1 && id <= NUM_SERVOS) {
        moveServo(id, SERVO_HOME[id - 1]);
      } else {
        Serial.println("Error: servo ID must be 1-4");
      }
    }
    // ---- pos / read ----
    else if (input.equalsIgnoreCase("pos") || input.equalsIgnoreCase("read")) {
      for (int i = 0; i < NUM_SERVOS; i++) {
        int pos = st.ReadPos(i + 1);
        float angle = posToAngle(i, pos);
        Serial.print("Servo "); Serial.print(i + 1);
        Serial.print(" pos: "); Serial.print(pos);
        Serial.print(" angle: "); Serial.print(angle, 1);
        Serial.println("°");
      }
      float x, y, z;
      forwardKinematics(x, y, z);
      Serial.print("XYZ: "); Serial.print(x, 1);
      Serial.print(", "); Serial.print(y, 1);
      Serial.print(", "); Serial.print(z, 1);
      Serial.println(" mm");
    }
    // ---- direct servo: <id> <pos> ----
    else {
      int spaceIdx = input.indexOf(' ');
      if (spaceIdx > 0) {
        int id  = input.substring(0, spaceIdx).toInt();
        int pos = input.substring(spaceIdx + 1).toInt();
        if (id >= 1 && id <= NUM_SERVOS) {
          moveServo(id, pos);
        } else {
          Serial.println("Unknown command. Press Enter for menu.");
        }
      } else {
        Serial.println("Unknown command. Press Enter for menu.");
      }
    }

    Serial.print("> ");
  }
}
