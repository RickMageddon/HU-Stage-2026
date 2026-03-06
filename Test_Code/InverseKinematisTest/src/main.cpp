#include <FABRIK2D.h>
#include <cmath>
#include <SCServo.h>
#include <Arduino.h>

// =====================================================================
//  INVERSE KINEMATICS + SERVO CONTROL
//  2-link planar arm with base rotation (Waveshare ST3235 servos)
// =====================================================================

// --- SCServo Communication ---
SMS_STS st;
#define S_RXD 9
#define S_TXD 8

// --- Servo Configuration (Waveshare ST3235: 0-4095 = 0-360°) ---
// Servo IDs: 1=Base rotation, 2=Shoulder, 3=Elbow, 4=Wrist
const int NUM_SERVOS   = 4;
const int SERVO_MIN[]  = {250,  820,  735,   1};
const int SERVO_MAX[]  = {2200, 2200, 2500, 2500};
const int SERVO_HOME[] = {400,  1950, 735,  1200};
const int SERVO_SPEED  = 1000;
const int SERVO_ACC    = 50;

// --- Arm Link Lengths (mm) ---
const int LINK1 = 180;   // Shoulder to elbow
const int LINK2 = 210;   // Elbow to end-effector
int linkLengths[] = {LINK1, LINK2};

// --- IK Solver: 2 links = 3 joints (shoulder, elbow, end-effector) ---
const int NUM_JOINTS = 3;
Fabrik2D fabrik(NUM_JOINTS, linkLengths, 0.5);

// --- ST3235 Servo Angle Mapping ---
// Full range: 0-4095 counts = 0°-360°
const float COUNTS_PER_DEG = 4095.0 / 360.0;  // ~11.375

// =====================================================================
//  SERVO CALIBRATION
//
//  Each servo needs two values:
//    homeAngleDeg - what IK angle (degrees) the home position represents
//    direction    - +1 if increasing IK angle = increasing servo position
//                   -1 if they move in opposite directions
//
//  HOW TO CALIBRATE:
//    1. Use 'servo <id> <pos>' to manually move joints
//    2. Measure the physical angle at known servo positions
//    3. Adjust homeAngleDeg and direction below
//    4. Test with 'goto <x> <y>' and refine
// =====================================================================

// Servo 1 - Base rotation (rotates the whole flat arm)
//   Home (400): arm facing forward = 0° rotation
const float BASE_HOME_DEG     = 0.0;
const int   BASE_DIR          = 1;

// Servo 2 - Shoulder (horizontal plane, first IK joint)
//   Home (1950): arm extending flat forward = 0° from forward axis
//   (Arm is mounted sideways, so it operates in the horizontal plane)
const float SHOULDER_HOME_DEG = 0.0;
const int   SHOULDER_DIR      = -1;

// Servo 3 - Elbow (horizontal plane, second IK joint)
//   Home (735): straight continuation of upper arm = 0° relative angle
const float ELBOW_HOME_DEG    = 0.0;
const int   ELBOW_DIR         = 1;

// Servo 4 - Wrist (end-effector orientation)
const float WRIST_HOME_DEG    = 0.0;
const int   WRIST_DIR         = 1;

// --- Visualization Data (sent to PC for URDF viewer) ---
float cmdBaseDeg = 0, cmdShoulderDeg = 0, cmdElbowDeg = 0, cmdWristDeg = 0;
unsigned long lastVisSend = 0;
const unsigned long VIS_INTERVAL = 250; // ms between VIS updates

// =====================================================================
//  HELPER FUNCTIONS
// =====================================================================

float radToDeg(float rad) {
  return rad * 180.0 / M_PI;
}

float degToRad(float deg) {
  return deg * M_PI / 180.0;
}

/**
 * Convert an IK angle (degrees) to a servo position, clamped to safe limits.
 */
int angleToServo(float angleDeg, float homeDeg, int homePos,
                 int dir, int sMin, int sMax) {
  float delta = (angleDeg - homeDeg) * (float)dir;
  int pos = homePos + (int)(delta * COUNTS_PER_DEG);
  return constrain(pos, sMin, sMax);
}

/**
 * Convert a servo position back to the corresponding IK angle (degrees).
 */
float servoToAngle(int servoPos, float homeDeg, int homePos, int dir) {
  float delta = (float)(servoPos - homePos) / COUNTS_PER_DEG;
  return homeDeg + delta / (float)dir;
}

/**
 * Move a single servo with range checking.
 */
void moveServo(int id, int pos) {
  if (id < 1 || id > NUM_SERVOS) {
    Serial.println("Error: servo ID must be 1-4");
    return;
  }
  int idx = id - 1;
  if (pos < SERVO_MIN[idx] || pos > SERVO_MAX[idx]) {
    Serial.print("Error: pos out of range for servo ");
    Serial.print(id);
    Serial.print(". Allowed: ");
    Serial.print(SERVO_MIN[idx]);
    Serial.print("-");
    Serial.println(SERVO_MAX[idx]);
    return;
  }
  st.WritePosEx(id, pos, SERVO_SPEED, SERVO_ACC);
  // Track commanded angle for visualization
  switch(id) {
    case 1: cmdBaseDeg     = servoToAngle(pos, BASE_HOME_DEG,     SERVO_HOME[0], BASE_DIR);     break;
    case 2: cmdShoulderDeg = servoToAngle(pos, SHOULDER_HOME_DEG, SERVO_HOME[1], SHOULDER_DIR); break;
    case 3: cmdElbowDeg    = servoToAngle(pos, ELBOW_HOME_DEG,    SERVO_HOME[2], ELBOW_DIR);    break;
    case 4: cmdWristDeg    = servoToAngle(pos, WRIST_HOME_DEG,    SERVO_HOME[3], WRIST_DIR);    break;
  }
  Serial.print("Servo "); Serial.print(id);
  Serial.print(" -> "); Serial.println(pos);
}

/**
 * Move all servos to home position.
 */
void moveHome() {
  Serial.println("Moving all servos to home...");
  for (int i = 0; i < NUM_SERVOS; i++) {
    st.WritePosEx(i + 1, SERVO_HOME[i], SERVO_SPEED, SERVO_ACC);
  }
  cmdBaseDeg = BASE_HOME_DEG;
  cmdShoulderDeg = SHOULDER_HOME_DEG;
  cmdElbowDeg = ELBOW_HOME_DEG;
  cmdWristDeg = WRIST_HOME_DEG;
}

/**
 * Read and print current positions of all servos.
 */
void readPositions() {
  for (int i = 0; i < NUM_SERVOS; i++) {
    int pos = st.ReadPos(i + 1);
    Serial.print("Servo "); Serial.print(i + 1);
    Serial.print(" pos: "); Serial.println(pos);
  }
}

/**
 * Send visualization data over serial for the PC-side URDF viewer.
 * Format: #VIS,cmd_base,cmd_shoulder,cmd_elbow,cmd_wrist,act_base,act_shoulder,act_elbow,act_wrist
 * All values in degrees.
 */
void sendVisualizationData() {
  float actBase     = servoToAngle(st.ReadPos(1), BASE_HOME_DEG,     SERVO_HOME[0], BASE_DIR);
  float actShoulder = servoToAngle(st.ReadPos(2), SHOULDER_HOME_DEG, SERVO_HOME[1], SHOULDER_DIR);
  float actElbow    = servoToAngle(st.ReadPos(3), ELBOW_HOME_DEG,    SERVO_HOME[2], ELBOW_DIR);
  float actWrist    = servoToAngle(st.ReadPos(4), WRIST_HOME_DEG,    SERVO_HOME[3], WRIST_DIR);

  Serial.print("#VIS,");
  Serial.print(cmdBaseDeg, 2);     Serial.print(",");
  Serial.print(cmdShoulderDeg, 2); Serial.print(",");
  Serial.print(cmdElbowDeg, 2);    Serial.print(",");
  Serial.print(cmdWristDeg, 2);    Serial.print(",");
  Serial.print(actBase, 2);        Serial.print(",");
  Serial.print(actShoulder, 2);    Serial.print(",");
  Serial.print(actElbow, 2);       Serial.print(",");
  Serial.println(actWrist, 2);
}

// =====================================================================
//  INVERSE KINEMATICS
// =====================================================================

/**
 * Move the arm to a 2D target in the horizontal plane.
 *   x = forward distance from base (mm)
 *   y = sideways distance from base (mm)
 *
 * The arm is mounted flat (sideways), so shoulder and elbow
 * move in the horizontal plane.
 *
 * Only moves Servo 2 (shoulder) and Servo 3 (elbow).
 * Base rotation (Servo 1) is unchanged.
 */
bool moveToXY(float x, float y) {
  uint8_t result = fabrik.solve(x, y, linkLengths);

  if (result == 0) {
    Serial.println("IK failed: target unreachable!");
    Serial.print("  Max reach: "); Serial.print(LINK1 + LINK2); Serial.println(" mm");
    Serial.print("  Target dist: "); Serial.print(sqrt(x * x + y * y)); Serial.println(" mm");
    return false;
  }

  // FABRIK angles: joint 0 = absolute angle of first link from +X axis
  //                joint 1 = relative angle at elbow
  float shoulderDeg = radToDeg(fabrik.getAngle(0));
  float elbowDeg    = radToDeg(fabrik.getAngle(1));

  // Map to servo positions
  int shoulderPos = angleToServo(shoulderDeg, SHOULDER_HOME_DEG,
                                 SERVO_HOME[1], SHOULDER_DIR,
                                 SERVO_MIN[1], SERVO_MAX[1]);
  int elbowPos    = angleToServo(elbowDeg, ELBOW_HOME_DEG,
                                 SERVO_HOME[2], ELBOW_DIR,
                                 SERVO_MIN[2], SERVO_MAX[2]);

  // Debug output
  Serial.println("--- IK 2D Result ---");
  Serial.print("Target: ("); Serial.print(x);
  Serial.print(", ");        Serial.print(y); Serial.println(") mm");
  Serial.print("Shoulder angle: "); Serial.print(shoulderDeg); Serial.println(" deg");
  Serial.print("Elbow angle:    "); Serial.print(elbowDeg);    Serial.println(" deg");
  Serial.print("Shoulder servo: "); Serial.println(shoulderPos);
  Serial.print("Elbow servo:    "); Serial.println(elbowPos);

  // Print solved joint positions
  for (int i = 0; i < NUM_JOINTS; i++) {
    Serial.print("  Joint "); Serial.print(i);
    Serial.print(": ("); Serial.print(fabrik.getX(i));
    Serial.print(", ");  Serial.print(fabrik.getY(i)); Serial.println(")");
  }
  Serial.println("--------------------");

  // Move servos
  moveServo(2, shoulderPos);
  moveServo(3, elbowPos);

  return true;
}

/**
 * Move the arm to a 3D target position.
 *   x = forward distance from base (mm)
 *   y = sideways distance from base (mm)
 *   z = vertical height from base (mm) — ignored by flat arm,
 *       but base rotation adjusts to point toward (x, y)
 *
 * The arm is flat, so shoulder+elbow solve in the horizontal
 * plane. Base rotation (Servo 1) rotates the whole arm to
 * face the target direction in the XY plane.
 *
 * Moves Servo 1 (base), Servo 2 (shoulder), and Servo 3 (elbow).
 */
bool moveToXYZ(float x, float y, float z) {
  // Base rotation: angle in the horizontal XY plane
  float baseDeg = radToDeg(atan2(y, x));

  // Horizontal distance for the 2D flat-plane IK
  float r = sqrt(x * x + y * y);

  // Solve 2D IK in the horizontal plane (arm is flat)
  uint8_t result = fabrik.solve(r, 0, linkLengths);

  if (result == 0) {
    Serial.println("IK failed: target unreachable!");
    Serial.print("  Max reach: "); Serial.print(LINK1 + LINK2); Serial.println(" mm");
    Serial.print("  Target dist: "); Serial.print(sqrt(r * r + y * y)); Serial.println(" mm");
    return false;
  }

  float shoulderDeg = radToDeg(fabrik.getAngle(0));
  float elbowDeg    = radToDeg(fabrik.getAngle(1));

  // Map to servo positions
  int basePos     = angleToServo(baseDeg, BASE_HOME_DEG,
                                 SERVO_HOME[0], BASE_DIR,
                                 SERVO_MIN[0], SERVO_MAX[0]);
  int shoulderPos = angleToServo(shoulderDeg, SHOULDER_HOME_DEG,
                                 SERVO_HOME[1], SHOULDER_DIR,
                                 SERVO_MIN[1], SERVO_MAX[1]);
  int elbowPos    = angleToServo(elbowDeg, ELBOW_HOME_DEG,
                                 SERVO_HOME[2], ELBOW_DIR,
                                 SERVO_MIN[2], SERVO_MAX[2]);

  // Debug output
  Serial.println("--- IK 3D Result ---");
  Serial.print("Target: ("); Serial.print(x);
  Serial.print(", ");        Serial.print(y);
  Serial.print(", ");        Serial.print(z); Serial.println(") mm");
  Serial.print("Base angle:     "); Serial.print(baseDeg);     Serial.println(" deg");
  Serial.print("Shoulder angle: "); Serial.print(shoulderDeg); Serial.println(" deg");
  Serial.print("Elbow angle:    "); Serial.print(elbowDeg);    Serial.println(" deg");
  Serial.print("Base servo:     "); Serial.println(basePos);
  Serial.print("Shoulder servo: "); Serial.println(shoulderPos);
  Serial.print("Elbow servo:    "); Serial.println(elbowPos);
  Serial.println("--------------------");

  // Move servos
  moveServo(1, basePos);
  moveServo(2, shoulderPos);
  moveServo(3, elbowPos);

  return true;
}

/**
 * Set wrist angle independently.
 */
void setWristAngle(float angleDeg) {
  int wristPos = angleToServo(angleDeg, WRIST_HOME_DEG,
                              SERVO_HOME[3], WRIST_DIR,
                              SERVO_MIN[3], SERVO_MAX[3]);
  Serial.print("Wrist angle: "); Serial.print(angleDeg);
  Serial.print(" deg -> servo: "); Serial.println(wristPos);
  moveServo(4, wristPos);
}

// Forward declaration
void printMenu();

// =====================================================================
//  DEMO MODE
// =====================================================================

bool demoMode = false;
unsigned long demoTimer = 0;
bool demoAtTarget = false;
const unsigned long DEMO_INTERVAL = 10000; // 10 seconds between moves

// Demo target positions (servo values)
const int DEMO_POS_1 = 1300;
const int DEMO_POS_3 = 2500;
const int DEMO_POS_4 = 2200;

void demoMoveToTarget() {
  Serial.println("[Demo] Moving to target...");
  st.WritePosEx(1, DEMO_POS_1, SERVO_SPEED, SERVO_ACC);
  st.WritePosEx(3, DEMO_POS_3, SERVO_SPEED, SERVO_ACC);
  st.WritePosEx(4, DEMO_POS_4, SERVO_SPEED, SERVO_ACC);
}

void demoMoveToHome() {
  Serial.println("[Demo] Moving to home...");
  for (int i = 0; i < NUM_SERVOS; i++) {
    st.WritePosEx(i + 1, SERVO_HOME[i], SERVO_SPEED, SERVO_ACC);
  }
}

void startDemo() {
  demoMode = true;
  demoAtTarget = false;
  Serial.println("[Demo] Started. Type 'stop' to exit demo mode.");
  demoMoveToTarget();
  demoTimer = millis();
}

void stopDemo() {
  demoMode = false;
  Serial.println("[Demo] Stopped.");
  moveHome();
  delay(1000);
  printMenu();
}

// =====================================================================
//  SERIAL COMMAND INTERFACE
// =====================================================================

void printMenu() {
  Serial.println();
  Serial.println("===== IK Servo Control =====");

  // Print current positions
  for (int i = 0; i < NUM_SERVOS; i++) {
    int pos = st.ReadPos(i + 1);
    Serial.print("  Servo "); Serial.print(i + 1);
    Serial.print(" | pos: "); Serial.print(pos);
    Serial.print(" | range: "); Serial.print(SERVO_MIN[i]);
    Serial.print("-");         Serial.print(SERVO_MAX[i]);
    Serial.print(" | home: "); Serial.println(SERVO_HOME[i]);
  }

  Serial.println();
  Serial.println("Commands:");
  Serial.println("  goto x y         Move to position in horizontal plane (mm)");
  Serial.println("  goto x y z       Move to 3D position (x=fwd, y=side, z=up) (mm)");
  Serial.println("  wrist angle      Set wrist angle (deg)");
  Serial.println("  servo id pos     Direct servo control");
  Serial.println("  home             All servos to home");
  Serial.println("  read             Read all servo positions");
  Serial.println("  demo             Start demo mode");
  Serial.println("  stop             Stop demo mode");
  Serial.println("============================");
  Serial.print("> ");
}

void processCommand(String input) {
  input.trim();
  if (input.length() == 0) {
    printMenu();
    return;
  }

  if (input.equalsIgnoreCase("stop")) {
    if (demoMode) stopDemo();
    else Serial.println("Not in demo mode.");
    return;
  }

  if (input.equalsIgnoreCase("demo")) {
    startDemo();
    return;
  }

  // Ignore other commands while in demo mode
  if (demoMode) {
    Serial.println("In demo mode. Type 'stop' to exit.");
    return;
  }

  if (input.equalsIgnoreCase("home")) {
    moveHome();
  }
  else if (input.equalsIgnoreCase("read")) {
    readPositions();
  }
  else if (input.startsWith("goto ") || input.startsWith("GOTO ")) {
    // Parse coordinates: "goto x y" or "goto x y z"
    String args = input.substring(5);
    args.trim();

    float coords[3] = {0, 0, 0};
    int numCoords = 0;
    int idx = 0;

    while (numCoords < 3 && idx < (int)args.length()) {
      // Skip spaces
      while (idx < (int)args.length() && args.charAt(idx) == ' ') idx++;
      if (idx >= (int)args.length()) break;

      // Find end of number
      int start = idx;
      while (idx < (int)args.length() && args.charAt(idx) != ' ') idx++;
      coords[numCoords] = args.substring(start, idx).toFloat();
      numCoords++;
    }

    if (numCoords == 2) {
      moveToXY(coords[0], coords[1]);
    } else if (numCoords == 3) {
      moveToXYZ(coords[0], coords[1], coords[2]);
    } else {
      Serial.println("Usage: goto x y  OR  goto x y z");
    }
  }
  else if (input.startsWith("wrist ") || input.startsWith("WRIST ")) {
    float angle = input.substring(6).toFloat();
    setWristAngle(angle);
  }
  else if (input.startsWith("servo ") || input.startsWith("SERVO ")) {
    String args = input.substring(6);
    args.trim();
    int spaceIdx = args.indexOf(' ');
    if (spaceIdx > 0) {
      int id  = args.substring(0, spaceIdx).toInt();
      int pos = args.substring(spaceIdx + 1).toInt();
      moveServo(id, pos);
    } else {
      Serial.println("Usage: servo <id> <position>");
    }
  }
  else {
    Serial.println("Unknown command. Press Enter for menu.");
  }

  Serial.print("> ");
}

// =====================================================================
//  SETUP & LOOP
// =====================================================================

void setup() {
  Serial.begin(115200);
  Serial1.begin(1000000, SERIAL_8N1, S_RXD, S_TXD);
  st.pSerial = &Serial1;

  delay(1000);

  Serial.println("IK Servo Control Ready!");
  Serial.println("Arm: 2-link (" + String(LINK1) + "mm + " + String(LINK2) + "mm), flat mount");
  Serial.println("Max reach: " + String(LINK1 + LINK2) + " mm (horizontal plane)");

  // Move all servos to home on startup
  moveHome();
  delay(2000);

  printMenu();
}

void loop() {
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    processCommand(input);
  }

  // Demo mode loop
  if (demoMode && (millis() - demoTimer >= DEMO_INTERVAL)) {
    if (!demoAtTarget) {
      demoMoveToHome();
      demoAtTarget = true;
    } else {
      demoMoveToTarget();
      demoAtTarget = false;
    }
    demoTimer = millis();
  }

  // Send visualization data periodically
  if (millis() - lastVisSend >= VIS_INTERVAL) {
    sendVisualizationData();
    lastVisSend = millis();
  }
}

