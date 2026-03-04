# URDF Arm Visualizer

Real-time 3D visualization of the robot arm alongside physical servo feedback.

## What it shows

Two side-by-side URDF models in a 3D PyBullet window:

| Model | Colour | Meaning |
|-------|--------|---------|
| Left  | Blue   | **Commanded** — the joint angles computed by the IK solver |
| Right | Green  | **Actual** — the joint angles read back from the physical servos |

If both models overlap perfectly, the physical arm is exactly where
the software thinks it is.

## Quick start

```bash
# 1. Install Python dependencies (once)
cd visualizer
pip install -r requirements.txt

# 2. Upload the firmware to the ESP32 (from project root)
#    pio run -t upload

# 3. Run the visualizer
python visualizer.py              # auto-detects serial port
python visualizer.py -p /dev/ttyACM0   # or specify port
python visualizer.py --no-serial  # demo mode without hardware
```

## How it works

1. The firmware (`main.cpp`) sends `#VIS,…` lines over USB serial every
   250 ms containing 8 comma-separated degree values:
   ```
   #VIS,cmd_shoulder,cmd_base,cmd_elbow,cmd_wrist,act_shoulder,act_base,act_elbow,act_wrist
   ```

2. The Python script reads these, maps them to the URDF joints, and
   updates the two PyBullet models at 60 FPS.

3. All other serial output (menus, debug, IK results) is printed to
   the console, so this script **replaces the PlatformIO serial monitor**.

## Controls

### Console (terminal where the script runs)
Type any command the firmware understands:  
`goto 200 100 50`, `home`, `demo`, `jog`, `read`, `servo 1 1000`, …

### 3D window (when PyBullet window is focused)
| Key | Action |
|-----|--------|
| W / S | Jog forward / backward (X) |
| A / D | Jog left / right (Y) |
| Q / E | Jog up / down (Z) |
| H | Home all servos |

These keys send the corresponding characters to the ESP32, so the arm
must be in **jog mode** (`jog` command) for them to work.

## Calibration

If a joint moves the wrong direction in the visualizer compared to the
real arm, edit `JOINT_SIGN` in `visualizer.py` and flip the sign for
that servo (e.g. `1: -1.0`).

If the zero-pose doesn't match, adjust `JOINT_OFFSET_DEG`.
