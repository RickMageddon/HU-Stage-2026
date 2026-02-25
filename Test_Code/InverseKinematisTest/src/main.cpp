#include <FABRIK2D.h>
#include <cmath>
#include <SCServo.h>
#include <Arduino.h>

// --- SCServo setup ---
SMS_STS st;
#define S_RXD 9
#define S_TXD 8
const int SERVO_ID[4] = {1, 2, 3, 4};
const int SERVO_MIN[4] = {250, 850, 735, 1};
const int SERVO_MAX[4] = {2250, 2200, 2450, 2500};
const int HOME_POS[4] = {400, 1950, 735, 1200};

// --- Arm dimensions ---
// The arm has 2 links: shoulder-to-elbow (180mm) and elbow-to-end-effector (210mm)
// FABRIK needs numJoints = numLinks + 1, so 3 joints: base, elbow, end-effector
int linkLengths[2] = {180, 210};
const int NUM_JOINTS = 4;

// --- FABRIK2D solver ---
// Create the solver with 3 joints and a tolerance of 0.5mm
Fabrik2D fabrik(NUM_JOINTS, linkLengths, 0.5);

// --- Helper: convert radians to degrees ---
float radToDeg(float rad) {
  return rad * 180.0 / M_PI;
}

// --- Helper: map a joint angle (degrees) to a servo position ---
// Maps an angle from [angleDegMin..angleDegMax] to [servoMin..servoMax]
int angleToServoPos(float angleDeg, int servoMin, int servoMax,
                    float angleDegMin, float angleDegMax) {
  // Clamp the angle within the expected range
  if (angleDeg < angleDegMin) angleDeg = angleDegMin;
  if (angleDeg > angleDegMax) angleDeg = angleDegMax;
  // Linear mapping from angle range to servo position range
  return (int)map((long)(angleDeg * 100), (long)(angleDegMin * 100),
                  (long)(angleDegMax * 100), servoMin, servoMax);
}

/**
 * calculateInverseKinematics
 *
 * Given a target (x, y) position in mm, this function:
 *   1. Runs the FABRIK2D solver to find joint angles
 *   2. Converts those angles (radians) to degrees
 *   3. Prints the results to Serial for debugging
 *
 * Parameters:
 *   targetX - horizontal distance from base (mm)
 *   targetY - vertical distance from base (mm)
 *
 * Returns true if the solver converged, false otherwise.
 */
bool calculateInverseKinematics(float targetX, float targetY) {
  // --- Step 1: Run the FABRIK solver ---
  // solve() returns: 0 = failed, 1 = converged, 2 = converged with looser tolerance
  uint8_t result = fabrik.solve(targetX, targetY, linkLengths);

  if (result == 0) {
    Serial.println("IK failed: target is unreachable!");
    return false;
  }

  // --- Step 2: Read the joint angles (in radians) and convert to degrees ---
  float baseAngleDeg  = radToDeg(fabrik.getAngle(0));  // angle at base joint
  float elbowAngleDeg = radToDeg(fabrik.getAngle(1));  // angle at elbow joint

  // --- Step 3: Print debug info ---
  Serial.println("--- Inverse Kinematics Result ---");
  Serial.print("Target: (");
  Serial.print(targetX); Serial.print(", "); Serial.print(targetY); Serial.println(") mm");

  Serial.print("Base  angle: "); Serial.print(baseAngleDeg);  Serial.println(" deg");
  Serial.print("Elbow angle: "); Serial.print(elbowAngleDeg); Serial.println(" deg");

  // Print the solved joint positions for verification
  for (int i = 0; i < NUM_JOINTS; i++) {
    Serial.print("Joint "); Serial.print(i);
    Serial.print(": ("); Serial.print(fabrik.getX(i));
    Serial.print(", "); Serial.print(fabrik.getY(i)); Serial.println(")");
  }
  Serial.println("---------------------------------");

  return true;
}


void setup() {
  Serial.begin(115200);
  Serial1.begin(1000000, SERIAL_8N1, S_RXD, S_TXD);
  st.pSerial = &Serial1;

  delay(1000);

  // --- Example: solve IK for a target position ---
  
}

void loop() {
Serial.println("Starting Inverse Kinematics test...");
  calculateInverseKinematics(100, 0); 
}

