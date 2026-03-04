#!/usr/bin/env python3
"""
Real-time URDF Robot Arm Visualizer
====================================

Reads joint angle data from the ESP32 over serial and displays a single
3D digital twin that mirrors the commanded position of the physical arm.

This script also acts as a serial terminal pass-through, so you can type
commands (goto, home, jog, demo, etc.) directly into the console.
It replaces the PlatformIO serial monitor.

Usage:
    python visualizer.py                        # Auto-detect serial port
    python visualizer.py --port /dev/ttyACM0    # Specify port
    python visualizer.py --no-serial            # Demo mode (sliders only)

PyBullet window keyboard shortcuts (when the 3D window is focused):
    W/S  = jog forward/backward  (sends to ESP32)
    A/D  = jog left/right
    Q/E  = jog up/down
    H    = home all servos
"""

import pybullet as p
import pybullet_data
import serial
import serial.tools.list_ports
import time
import math
import sys
import os
import argparse
import threading

# ─── Configuration ─────────────────────────────────────────────────────

URDF_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'new_arm.URDF')

# URDF joint names mapped to firmware servo IDs
JOINT_MAP = {
    1: 'base_link_to_servo1',       # Servo 1: Shoulder pitch
    2: 'servo2Base_to_servo2',      # Servo 2: Base rotation
    3: 'linkendcorner_to_servo3',   # Servo 3: Elbow pitch
    4: 'link2_to_servo4',           # Servo 4: Wrist
}

# Sign correction per joint: flip to -1.0 if the URDF joint rotates
# opposite to the firmware convention.  Tuned per-joint from testing:
#   goto 100 0 0  → correct (shoulder -1, elbow near 0 so sign irrelevant)
#   goto 0 100 0  → base must be +1 (was inverted at -1)
#   goto 0 0 100  → elbow must be +1 (was inverted at -1)
JOINT_SIGN = {1: -1.0, 2: 1.0, 3: 1.0, 4: -1.0}

# Angle offset (degrees) added before converting to radians.
# Use if the URDF zero-pose doesn't match the firmware zero.
JOINT_OFFSET_DEG = {1: 0.0, 2: 0.0, 3: 0.0, 4: 0.0}

# Scale: firmware works in mm, URDF in its own units.
# URDF Link1 box = 1.55 units = 180 mm  →  1 unit ≈ 116 mm
# URDF Link2 box = 1.85 units = 210 mm  →  1 unit ≈ 113 mm
MM_TO_URDF = 1.0 / 115.0  # average scale factor

# Approximate shoulder joint origin in URDF world coordinates
# (base_link at Z=0.5, servo1 joint at ~X=0.6 from base center)
URDF_SHOULDER_ORIGIN = (0.6, 0.0, 0.5)


# ─── Arm Visualizer Class ──────────────────────────────────────────────

class ArmVisualizer:
    def __init__(self, port=None, baud=115200, no_serial=False):
        self.port = port
        self.baud = baud
        self.no_serial = no_serial
        self.ser = None
        self.running = True

        # Joint angles in degrees (updated from serial #VIS lines)
        self.cmd_angles = {1: 0.0, 2: 0.0, 3: 0.0, 4: 0.0}
        # Display angles: smoothly interpolated toward cmd_angles each frame
        self.display_angles = {1: 0.0, 2: 0.0, 3: 0.0, 4: 0.0}
        self.lock = threading.Lock()

        # Interpolation speed: degrees per second each servo can move
        # (ST3235 servos at speed=1000 → ~88 deg/s; tune to taste)
        self.interp_speed = 90.0  # deg/s

        # Last goto target coordinates (mm)
        self.target_coords = None   # (x, y, z) or None

        # PyBullet handles
        self.physics_client = None
        self.robot = None
        self.joint_indices = {}

        # Status text overlay
        self.status_text_id = None

    # ── Serial port helpers ────────────────────────────────────────

    def find_serial_port(self):
        """Auto-detect the ESP32 serial port."""
        ports = serial.tools.list_ports.comports()
        keywords = ['esp32', 'cp210', 'ch340', 'ch910', 'usb', 'acm',
                     'uart', 'jtag', 'serial']
        for port_info in ports:
            desc = (port_info.description or '').lower()
            hwid = (port_info.hwid or '').lower()
            if any(k in desc or k in hwid for k in keywords):
                print(f"  Found serial port: {port_info.device}  "
                      f"({port_info.description})")
                return port_info.device
        if ports:
            print(f"  Using first available port: {ports[0].device}")
            return ports[0].device
        return None

    def connect_serial(self):
        """Open the serial connection to the ESP32."""
        if self.no_serial:
            print("  --no-serial flag set. Running without serial.")
            return False
        if self.port is None:
            self.port = self.find_serial_port()
        if self.port is None:
            print("  WARNING: No serial port found. "
                  "Running in demo mode (sliders only).")
            return False
        try:
            self.ser = serial.Serial(self.port, self.baud, timeout=0.1)
            print(f"  Connected to {self.port} @ {self.baud} baud")
            time.sleep(0.5)  # let the port settle
            return True
        except Exception as e:
            print(f"  WARNING: Could not open {self.port}: {e}")
            print("  Running in demo mode (sliders only).")
            return False

    # ── PyBullet setup ─────────────────────────────────────────────

    def setup_pybullet(self):
        """Initialize PyBullet GUI and load the URDF."""
        self.physics_client = p.connect(p.GUI)
        p.setAdditionalSearchPath(pybullet_data.getDataPath())
        p.setGravity(0, 0, 0)   # no gravity — pure visualization
        p.setRealTimeSimulation(0)

        # Ground plane
        p.loadURDF("plane.urdf")

        # Draw coordinate grid and axes
        self._draw_grid_and_axes()

        # Camera — sized for new_arm.URDF scale
        p.resetDebugVisualizerCamera(
            cameraDistance=6.0,
            cameraYaw=45,
            cameraPitch=-25,
            cameraTargetPosition=[0, 0, 0.5]
        )

        # Hide default GUI panels for a cleaner view
        p.configureDebugVisualizer(p.COV_ENABLE_GUI, 0)
        p.configureDebugVisualizer(p.COV_ENABLE_SHADOWS, 1)

        urdf_path = os.path.abspath(URDF_PATH)
        if not os.path.exists(urdf_path):
            print(f"ERROR: URDF file not found at {urdf_path}")
            sys.exit(1)

        # ── Load single arm model ──
        self.robot = p.loadURDF(
            urdf_path,
            basePosition=[0, 0, 0],
            useFixedBase=True,
            flags=p.URDF_USE_SELF_COLLISION_EXCLUDE_PARENT
        )

        # Build joint index map
        self.joint_indices = self._get_joint_indices(self.robot)

        # Label
        p.addUserDebugText(
            "Digital Twin",
            [0, 0, 2.5],
            textColorRGB=[1.0, 1.0, 1.0],
            textSize=1.5
        )

        # Target marker (hidden until user places one)
        self.target_marker_id = None
        self.target_label_id = None

        # Goto marker (shows where the goto coordinate is in 3D space)
        self.goto_marker_id = None
        self.goto_label_id = None

        print("  PyBullet visualization ready.")

    # ── Grid and axes ──────────────────────────────────────────────

    def _draw_grid_and_axes(self):
        """Draw XYZ axes and a grid on the ground plane."""
        # Axis length
        L = 4.0

        # X axis = Red
        p.addUserDebugLine([0, 0, 0.01], [L, 0, 0.01],
                           lineColorRGB=[1, 0, 0], lineWidth=3)
        p.addUserDebugText("X", [L + 0.2, 0, 0.01],
                           textColorRGB=[1, 0, 0], textSize=1.5)

        # Y axis = Green
        p.addUserDebugLine([0, 0, 0.01], [0, L, 0.01],
                           lineColorRGB=[0, 1, 0], lineWidth=3)
        p.addUserDebugText("Y", [0, L + 0.2, 0.01],
                           textColorRGB=[0, 1, 0], textSize=1.5)

        # Z axis = Blue
        p.addUserDebugLine([0, 0, 0], [0, 0, L],
                           lineColorRGB=[0, 0.4, 1], lineWidth=3)
        p.addUserDebugText("Z", [0, 0, L + 0.2],
                           textColorRGB=[0, 0.4, 1], textSize=1.5)

        # Grid lines on the ground (XY plane, Z=0)
        grid_size = 4.0
        step = 1.0  # 1 unit per grid line
        grey = [0.4, 0.4, 0.4]
        v = -grid_size
        while v <= grid_size:
            # Lines parallel to X
            p.addUserDebugLine([v, -grid_size, 0.005], [v, grid_size, 0.005],
                               lineColorRGB=grey, lineWidth=1)
            # Lines parallel to Y
            p.addUserDebugLine([-grid_size, v, 0.005], [grid_size, v, 0.005],
                               lineColorRGB=grey, lineWidth=1)
            # Number labels along X
            if abs(v) > 0.01:
                p.addUserDebugText(f"{v:.0f}", [v, -0.3, 0.01],
                                   textColorRGB=[0.7, 0.7, 0.7], textSize=0.8)
            # Number labels along Y
            if abs(v) > 0.01:
                p.addUserDebugText(f"{v:.0f}", [-0.3, v, 0.01],
                                   textColorRGB=[0.7, 0.7, 0.7], textSize=0.8)
            v += step

        # Number labels along Z
        z = step
        while z <= grid_size:
            p.addUserDebugText(f"{z:.0f}", [0.15, 0, z],
                               textColorRGB=[0.7, 0.7, 0.7], textSize=0.8)
            z += step

        # Origin label
        p.addUserDebugText("0", [0.15, 0.15, 0.01],
                           textColorRGB=[1, 1, 1], textSize=1.0)

    def place_target_marker(self, x, y, z):
        """Place/move a visible sphere at the given coordinates."""
        pos = [x, y, z]
        color = [1, 0.2, 0.2]  # red

        # Remove old marker
        if self.target_marker_id is not None:
            p.removeBody(self.target_marker_id)
        if self.target_label_id is not None:
            p.removeUserDebugItem(self.target_label_id)

        # Create a small red sphere
        vs = p.createVisualShape(p.GEOM_SPHERE, radius=0.12,
                                  rgbaColor=[1, 0.2, 0.2, 0.8])
        self.target_marker_id = p.createMultiBody(
            baseMass=0, baseVisualShapeIndex=vs, basePosition=pos)

        # Label
        label = f"({x:.1f}, {y:.1f}, {z:.1f})"
        self.target_label_id = p.addUserDebugText(
            label, [x, y, z + 0.25],
            textColorRGB=color, textSize=1.2)

        print(f"  Target marker placed at ({x}, {y}, {z})")

    def place_goto_marker(self, x_mm, y_mm, z_mm):
        """Place a small sphere at the goto target, converting mm to URDF coords.
        Firmware: X=forward, Y=sideways, Z=up.
        URDF world: X=forward, Y=left, Z=up (same orientation).
        Origin = shoulder joint position in URDF world.
        """
        ox, oy, oz = URDF_SHOULDER_ORIGIN
        ux = ox + x_mm * MM_TO_URDF
        uy = oy + y_mm * MM_TO_URDF
        uz = oz + z_mm * MM_TO_URDF

        # Remove previous goto marker
        if self.goto_marker_id is not None:
            p.removeBody(self.goto_marker_id)
        if self.goto_label_id is not None:
            p.removeUserDebugItem(self.goto_label_id)

        # Small green sphere
        vs = p.createVisualShape(p.GEOM_SPHERE, radius=0.08,
                                  rgbaColor=[0.2, 1.0, 0.2, 0.85])
        self.goto_marker_id = p.createMultiBody(
            baseMass=0, baseVisualShapeIndex=vs, basePosition=[ux, uy, uz])

        label = f"goto ({x_mm:.0f},{y_mm:.0f},{z_mm:.0f})"
        self.goto_label_id = p.addUserDebugText(
            label, [ux, uy, uz + 0.2],
            textColorRGB=[0.2, 1.0, 0.2], textSize=1.0)

    def remove_goto_marker(self):
        """Remove the goto marker (e.g. on 'home')."""
        if self.goto_marker_id is not None:
            p.removeBody(self.goto_marker_id)
            self.goto_marker_id = None
        if self.goto_label_id is not None:
            p.removeUserDebugItem(self.goto_label_id)
            self.goto_label_id = None

    def _get_joint_indices(self, robot_id, label=""):
        """Map firmware servo IDs → PyBullet joint indices."""
        name_to_idx = {}
        for i in range(p.getNumJoints(robot_id)):
            info = p.getJointInfo(robot_id, i)
            name = info[1].decode('utf-8')
            name_to_idx[name] = i

        indices = {}
        for servo_id, joint_name in JOINT_MAP.items():
            if joint_name in name_to_idx:
                indices[servo_id] = name_to_idx[joint_name]
            else:
                print(f"  [{label}] WARNING: joint '{joint_name}' "
                      f"not found in URDF")
        return indices

    # ── Joint update ───────────────────────────────────────────────

    def update_robot(self, robot_id, joint_indices, angles_deg):
        """Set URDF joint angles from firmware degree values."""
        for servo_id, deg in angles_deg.items():
            if servo_id not in joint_indices:
                continue
            adj = (deg + JOINT_OFFSET_DEG.get(servo_id, 0.0)) \
                  * JOINT_SIGN.get(servo_id, 1.0)
            p.resetJointState(robot_id,
                              joint_indices[servo_id],
                              math.radians(adj))

    # ── Serial parsing ─────────────────────────────────────────────

    def parse_vis_line(self, line):
        """
        Parse a #VIS line from the firmware.
        Format: #VIS,cmd_s1,cmd_s2,cmd_s3,cmd_s4,act_s1,act_s2,act_s3,act_s4
        We only use the commanded angles (first 4) for the digital twin.
        """
        if not line.startswith('#VIS,'):
            return False
        try:
            parts = line[5:].split(',')
            if len(parts) < 4:
                return False
            with self.lock:
                self.cmd_angles[1] = float(parts[0])
                self.cmd_angles[2] = float(parts[1])
                self.cmd_angles[3] = float(parts[2])
                self.cmd_angles[4] = float(parts[3])
            return True
        except (ValueError, IndexError):
            return False

    # ── Background threads ─────────────────────────────────────────

    def serial_reader_thread(self):
        """Read serial data from ESP32 in a background thread."""
        while self.running and self.ser:
            try:
                if self.ser.in_waiting:
                    raw = self.ser.readline()
                    line = raw.decode('utf-8', errors='ignore').strip()
                    if not line:
                        continue
                    if self.parse_vis_line(line):
                        pass   # consumed by visualizer, don't echo
                    else:
                        print(line)  # pass-through to console
            except serial.SerialException:
                if self.running:
                    print("[Serial] Connection lost.")
                break
            except Exception as e:
                if self.running:
                    print(f"[Serial] Error: {e}")
                time.sleep(0.05)

    def console_input_thread(self):
        """Read user keyboard input and forward to ESP32."""
        print("\n  Type commands below (goto, home, jog, demo, read, …).")
        print("  Extra visualizer commands:")
        print("    target x y z   — place a red marker sphere at (x,y,z)")
        print("  Press Ctrl-C to quit.\n")
        while self.running:
            try:
                cmd = input("> ")
                stripped = cmd.strip().lower()
                # Handle visualizer-only commands
                if stripped.startswith('target '):
                    parts = stripped.split()
                    if len(parts) == 4:
                        try:
                            tx, ty, tz = float(parts[1]), float(parts[2]), float(parts[3])
                            self.place_target_marker(tx, ty, tz)
                        except ValueError:
                            print("  Usage: target x y z  (numbers)")
                    else:
                        print("  Usage: target x y z")
                    continue
                # Capture goto coordinates for the overlay
                if stripped.startswith('goto '):
                    parts = stripped.split()
                    try:
                        if len(parts) == 3:
                            self.target_coords = (float(parts[1]), float(parts[2]), 0.0)
                            self.place_goto_marker(float(parts[1]), float(parts[2]), 0.0)
                        elif len(parts) == 4:
                            self.target_coords = (float(parts[1]), float(parts[2]), float(parts[3]))
                            self.place_goto_marker(float(parts[1]), float(parts[2]), float(parts[3]))
                    except ValueError:
                        pass
                elif stripped == 'home':
                    self.target_coords = None
                    self.remove_goto_marker()
                # Forward everything else to ESP32
                if self.ser and self.ser.is_open:
                    self.ser.write((cmd + '\n').encode('utf-8'))
                else:
                    print("  (no serial connection)")
            except EOFError:
                break
            except KeyboardInterrupt:
                self.running = False
                break

    # ── PyBullet keyboard → serial jog ─────────────────────────────

    _JOG_KEYS = {
        ord('w'): 'w', ord('s'): 's',
        ord('a'): 'a', ord('d'): 'd',
        ord('q'): 'q', ord('e'): 'e',
        ord('h'): 'home\n',
    }

    def handle_pybullet_keys(self):
        """Check PyBullet keyboard events and send jog chars to ESP32."""
        if not self.ser or not self.ser.is_open:
            return
        keys = p.getKeyboardEvents()
        for k, state in keys.items():
            if state & p.KEY_WAS_TRIGGERED:
                char = self._JOG_KEYS.get(k)
                if char:
                    try:
                        self.ser.write(char.encode('utf-8'))
                    except Exception:
                        pass

    # ── Status overlay ─────────────────────────────────────────────

    def update_status_text(self):
        """Show current angles as on-screen text in PyBullet."""
        with self.lock:
            ca = self.cmd_angles.copy()

        coord_str = ""
        if self.target_coords is not None:
            cx, cy, cz = self.target_coords
            coord_str = f"  |  Target: X={cx:.0f}  Y={cy:.0f}  Z={cz:.0f} mm"

        text = (
            f"S1:{ca[1]:6.1f}  S2:{ca[2]:6.1f}"
            f"  S3:{ca[3]:6.1f}  S4:{ca[4]:6.1f}"
            + coord_str
        )

        if self.status_text_id is not None:
            self.status_text_id = p.addUserDebugText(
                text,
                [0, -2, 0.1],
                textColorRGB=[0.7, 0.2, 1.0],
                textSize=1.2,
                replaceItemUniqueId=self.status_text_id
            )
        else:
            self.status_text_id = p.addUserDebugText(
                text,
                [0, -2, 0.1],
                textColorRGB=[0.7, 0.2, 1.0],
                textSize=1.2
            )

    # ── Main loop ──────────────────────────────────────────────────

    def run(self):
        print("\n=== URDF Arm Visualizer ===\n")

        self.setup_pybullet()
        has_serial = self.connect_serial()

        sliders = {}

        if has_serial:
            # Start serial reader
            t_reader = threading.Thread(target=self.serial_reader_thread,
                                        daemon=True)
            t_reader.start()

            # Start console input forwarder
            t_input = threading.Thread(target=self.console_input_thread,
                                       daemon=True)
            t_input.start()
        else:
            # No serial → add manual debug sliders
            p.configureDebugVisualizer(p.COV_ENABLE_GUI, 1)
            for sid in range(1, 5):
                sliders[sid] = p.addUserDebugParameter(
                    f"Servo {sid} (deg)", -180, 180, 0)
            print("\n  No serial — use the sliders in the PyBullet window.\n")

        print("  Close the 3D window or press Ctrl-C to exit.\n")

        frame_dt = 1.0 / 60.0
        status_interval = 0.25   # seconds between status overlay updates
        last_status = 0.0

        try:
            while self.running:
                # Check if PyBullet is still alive
                try:
                    p.getConnectionInfo(self.physics_client)
                except Exception:
                    break

                now = time.time()

                # Slider fallback (no serial)
                if not has_serial:
                    for sid, sl in sliders.items():
                        val = p.readUserDebugParameter(sl)
                        self.cmd_angles[sid] = val

                # Keyboard jog forwarding
                if has_serial:
                    self.handle_pybullet_keys()

                # Copy target angles under lock
                with self.lock:
                    target = self.cmd_angles.copy()

                # Smoothly interpolate display angles toward targets
                max_step = self.interp_speed * frame_dt  # max deg this frame
                for sid in self.display_angles:
                    diff = target[sid] - self.display_angles[sid]
                    if abs(diff) < 0.1:          # close enough → snap
                        self.display_angles[sid] = target[sid]
                    elif abs(diff) <= max_step:   # within one step
                        self.display_angles[sid] = target[sid]
                    else:                          # move at servo speed
                        self.display_angles[sid] += max_step * (1 if diff > 0 else -1)

                # Update 3D model with interpolated angles
                self.update_robot(self.robot,
                                  self.joint_indices, self.display_angles)

                # Status text overlay
                if now - last_status > status_interval:
                    self.update_status_text()
                    last_status = now

                p.stepSimulation()
                time.sleep(frame_dt)

        except KeyboardInterrupt:
            print("\n  Shutting down…")
        finally:
            self.running = False
            if self.ser and self.ser.is_open:
                self.ser.close()
            try:
                p.disconnect()
            except Exception:
                pass
        print("  Done.")


# ─── CLI entry point ───────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(
        description='Real-time URDF arm visualizer with serial pass-through')
    parser.add_argument('--port', '-p', type=str, default=None,
                        help='Serial port (auto-detected if omitted)')
    parser.add_argument('--baud', '-b', type=int, default=115200,
                        help='Baud rate (default: 115200)')
    parser.add_argument('--no-serial', action='store_true',
                        help='Run without serial (slider demo mode)')
    args = parser.parse_args()

    viz = ArmVisualizer(port=args.port, baud=args.baud,
                        no_serial=args.no_serial)
    viz.run()


if __name__ == '__main__':
    main()
