# X-Plane 11 Setup & HID / UDP Configuration

## 1. Prerequisites & Installation

1. **Install Python:** Install a current Python interpreter on Windows.
2. **Install NASA X-Plane Connect (XPC v1.2.1):**
   - Download **X-Plane Connect (v1.2.1)** from NASA and extract the archive.
   - Copy the `xpcPlugin` folder (containing `win.xpl`) into `<X-Plane 11 Installation Folder>\Resources\Plugins` and rename it to `XPC`.
   - Open `cmd` in the extracted folder containing `setup.py` and run:
     ```bash
     pip install .
     ```
3. **Enable Plugin in X-Plane 11:**
   - Open a flight in X-Plane 11, go to **Plugins → Show Plugin Admin**, and enable **X-Plane Connect**.

---

## 2. UDP Configuration

- **IP Address:** `127.0.0.1`
- **Port:** `49000`

In X-Plane 11, go to **Settings → Data Output** and enable **Network via UDP** for the following indices:

| Index | Data to Output |
| :---: | :--- |
| `3` | Speeds |
| `4` | Mach, VVI, g-load |
| `11` | Flight controls aileron/elevator/rudder |
| `13` | Trim, flap, stats & speedbrakes |
| `14` | Gear & Brakes |
| `17` | Pitch, roll & headings |
| `20` | Latitude, longitude & altitude |
| `26` | Throttle (actual) |
| `29` | Mixture Setting |
| `117` | Autopilot modes |
| `118` | Autopilot values |

---

## 3. HID Joystick & Button Mapping

### Axes

| Axis | X-Plane 11 Assignment |
| :---: | :--- |
| `Axis 0` | View up/down |
| `Axis 1` | View left/right |
| `Axis 2` | Mixture |
| `Axis 3` | Throttle |
| `Axis 4` | Yaw |
| `Axis 5` | Pitch |
| `Axis 6` | Roll |

### Buttons

| Button | X-Plane 11 Assignment |
| :---: | :--- |
| `Btn 0` | Hold Brakes Maximum |
| `Btn 1` | *(Unassigned)* |
| `Btn 2` | Flaps up a notch |
| `Btn 3` | Flaps down a notch |
| `Btn 4` | Avionics toggle |
| `Btn 5` | Engage Starters |
| `Btn 6` | Batteries all toggle |
| `Btn 7` | Generators all toggle |
| `Btn 8` | Zoom in |
| `Btn 9` | Zoom out |
| `Btn 10` | Toggle Brakes regular effort |
| `Btn 11` | Autopilot heading-select |
| `Btn 12` | Autopilot altitude select or hold |
| `Btn 13` | Autopilot vertical speed, at current VSI |
| `Btn 14` | Autopilot VOR/LOC arm |
| `Btn 15` | Servos toggle |

---

## 4. Running the Bridge

Start the Python bridge script to receive telemetry data and exchange autopilot values with X-Plane 11