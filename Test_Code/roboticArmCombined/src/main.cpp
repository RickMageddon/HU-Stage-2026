#include <Arduino.h>
#include <stdio.h>
#include <math.h>

// put function declarations here:
float l1, l2, L3; //link lentghs (linear actuator arms)
float dx, dy, dz, dw; //desired change in position (x,y,z) and wrist angle (w)
float phi; // orientation of the end effector (wrist angle)
float theta1, theta2, theta3, theta4; //joint angles for the 4 DOF arm
float A, B, C; //intermediate variables for IK calculations

void setup() {
  // put your setup code here, to run once:
  int result = myFunction(2, 3);
}

void loop() {
  // put your main code here, to run repeatedly:
}

// put function definitions here:
int myFunction(int x, int y) {
  return x + y;
}