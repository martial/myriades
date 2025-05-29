# Myriades Project - HourGlass OSC API Reference (v1.0.6+)

## Overview
- **OSC Receive Port**: `8000`
- **Command Format**: `/address [type_char_for_arg1 value1] [type_char_for_arg2 value2] ...`
  - Example: `/hourglass/1/up/rgb iii 255 128 0` (sends 3 integers)
  - Common type chars: `i` (int), `f` (float), `s` (string)

## Important Notes
- **Luminosity Interaction**: Final LED brightness for an hourglass = `BaseColor * GlobalLuminosity * IndividualLuminosity`.
- **Blackout Behavior**:
    - `/blackout` (global): Sets `GlobalLuminosity` to `0.0`.
    - `/hourglass/{id}/blackout`: Sets `IndividualLuminosity` for hourglass `{id}` to `0.0`.
- **No OSC Responses**: Check the application console for status and error logs.
- **Parameter Order**: For commands with optional parameters (e.g., motor speed/accel), if providing a later optional parameter, preceding ones must also be provided.
- **GUI Synchronization**: The UI sliders for global and the currently selected hourglass's individual luminosity should update in response to OSC commands.

---

## I. Global Control Commands

These commands affect the entire system or all connected hourglasses.

| Address                      | Arguments              | Description                                                                 |
|------------------------------|------------------------|-----------------------------------------------------------------------------|
| `/blackout`                  | (none)                 | Sets **Global Luminosity** to `0.0`. All LEDs on all hourglasses will be OFF. |
| `/system/luminosity`         | `f [0.0-1.0]`          | Sets **Global Luminosity** multiplier for ALL hourglasses.                    |
| `/system/motor/preset`        | `s [name]`             | Sets default speed & accel for ALL HGs. Names: slow, smooth, medium, fast.        |
| `/system/motor/config/{speed}/{accel}` | (Path params)          | Sets default speed (0-500) & accel (0-255) for ALL HGs.                           |
| `/system/motor/rotate/{angle_degrees}/{speed?}/{acceleration?}` | Path: angle (f), speed (i, opt), accel (i, opt) | Rotates ALL connected hourglasses by angle. Uses individual defaults if speed/accel omitted. |
| `/system/motor/position/{angle_degrees}/{speed?}/{acceleration?}` | Path: angle (f), speed (i, opt), accel (i, opt) | Moves ALL connected hourglasses to absolute angle. Uses individual defaults if speed/accel omitted. |
| `/system/emergency_stop_all` | (none)                 | Stops motors on ALL connected hourglasses.                                  |
| `/system/list_devices`       | (none)                 | Logs available serial devices to the application console.                   |

---

## II. Per-Hourglass Commands

Replace `{id}` with the target hourglass ID (e.g., `1`, `2`, ...).

### A. Connection

| Address                          | Arguments | Description                                                              |
|----------------------------------|-----------|--------------------------------------------------------------------------|
| `/hourglass/{id}/connect`        | (none)    | Connects the specified hourglass.                                          |
| `/hourglass/{id}/disconnect`     | (none)    | Disconnects the specified hourglass.                                       |
*Note: `/hourglass/connect` and `/hourglass/disconnect` (for all) are available but less common.* 

### B. Luminosity & Blackout (Individual)

| Address                          | Arguments     | Description                                                                                                |
|----------------------------------|---------------|------------------------------------------------------------------------------------------------------------|
| `/hourglass/{id}/luminosity`     | `f [0.0-1.0]` | Sets **Individual Luminosity** multiplier for the specified hourglass.                                       |
| `/hourglass/{id}/blackout`       | (none)        | Shortcut: Sets **Individual Luminosity** for the specified hourglass to `0.0`. Global luminosity still applies. |

### C. Motor Control

Default motor parameters are set per hourglass. Movement commands can override these.

**1. Setting Default Motor Parameters:**

| Address                               | Arguments      | Description                                                                          |
|---------------------------------------|----------------|--------------------------------------------------------------------------------------|
| `/hourglass/{id}/motor/enable`        | `i [0_or_1]`   | Enable (1) or disable (0) the motor.                                                 |
| `/hourglass/{id}/motor/emergency_stop`| (none)         | Immediately stops the motor.                                                           |
| `/hourglass/{id}/motor/speed`         | `i [0-500]`    | Sets DEFAULT speed. Updates UI, NO immediate motor command. Use `/config` for both.    |
| `/hourglass/{id}/motor/acceleration`  | `i [0-255]`    | Sets DEFAULT acceleration. Updates UI, NO immediate motor command. Use `/config` for both.|
| `/hourglass/{id}/motor/preset`        | `s [name]`     | Sets default speed & accel from a preset. Names: `slow`, `smooth`, `medium`, `fast`. |
| `/hourglass/{id}/motor/config/{speed}/{accel}` | (Path params) | Sets default speed (0-500) & accel (0-255). Updates UI, NO immediate motor command.|

**2. Executing Motor Movements:**

Both path-based and parameter-based formats are available for `rotate` and `position`.
Optional `speed` (0-500) and `acceleration` (0-255) use defaults if not provided.

*   **Path Format (Preferred for new integrations):**
    *   `/hourglass/{id}/motor/rotate/{angle_degrees}/{speed?}/{acceleration?}`
    *   `/hourglass/{id}/motor/position/{angle_degrees}/{speed?}/{acceleration?}`
    *   *Example:* `/hourglass/1/motor/rotate/90/200/150` (Rotate 90°, speed 200, accel 150)
    *   *Example:* `/hourglass/1/motor/rotate/-45` (Rotate -45°, use default speed & accel)

*   **Parameter Format (Legacy):**
    *   `/hourglass/{id}/motor/rotate f[degrees] i[speed?] i[acceleration?]`
    *   `/hourglass/{id}/motor/position f[degrees] i[speed?] i[acceleration?]`
    *   *Example:* `/hourglass/1/motor/position f 0 i 250 i 120` (Move to 0°, speed 250, accel 120)

### D. LED Control (RGB and Main)

Brightness is affected by Individual and Global Luminosity. Values are 0-255 before modulation.

| Address                               | Arguments         | Description                                                        |
|---------------------------------------|-------------------|--------------------------------------------------------------------|
| `/hourglass/{id}/led/all/rgb`         | `iii [r] [g] [b]` | Set RGB color for both UP and DOWN LEDs.                             |
| `/hourglass/{id}/up/rgb`              | `iii [r] [g] [b]` | Set RGB color for the UP LED only.                                 |
| `/hourglass/{id}/down/rgb`            | `iii [r] [g] [b]` | Set RGB color for the DOWN LED only.                               |
| `/hourglass/{id}/up/brightness`       | `i [0-255]`       | Set brightness for UP LED (monochromatic R=G=B).                   |
| `/hourglass/{id}/down/brightness`     | `i [0-255]`       | Set brightness for DOWN LED (monochromatic R=G=B).                 |
| `/hourglass/{id}/main/up`             | `i [0-255]`       | Set brightness for the UP main LED.                                |
| `/hourglass/{id}/main/down`           | `i [0-255]`       | Set brightness for the DOWN main LED.                              |
| `/hourglass/{id}/main/all`            | `i [0-255]`       | Set brightness for both UP and DOWN main LEDs.                     |

**LED Effect Parameters:**

| Address                               | Arguments         | Description                                                        |
|---------------------------------------|-------------------|--------------------------------------------------------------------|
| `/hourglass/{id}/up/blend`            | `i [0-768]`       | Set blend effect parameter for UP LED only.                        |
| `/hourglass/{id}/down/blend`          | `i [0-768]`       | Set blend effect parameter for DOWN LED only.                      |
| `/hourglass/{id}/led/all/blend`       | `i [0-768]`       | Set blend effect parameter for both UP and DOWN LEDs.              |
| `/hourglass/{id}/up/origin`           | `i [0-360]`       | Set origin angle (degrees) for UP LED effect.                      |
| `/hourglass/{id}/down/origin`         | `i [0-360]`       | Set origin angle (degrees) for DOWN LED effect.                    |
| `/hourglass/{id}/led/all/origin`      | `i [0-360]`       | Set origin angle (degrees) for both UP and DOWN LED effects.       |
| `/hourglass/{id}/up/arc`              | `i [0-360]`       | Set arc angle (degrees) for UP LED effect.                         |
| `/hourglass/{id}/down/arc`            | `i [0-360]`       | Set arc angle (degrees) for DOWN LED effect.                       |
| `/hourglass/{id}/led/all/arc`         | `i [0-360]`       | Set arc angle (degrees) for both UP and DOWN LED effects.          |

### E. PWM Control (Electromagnets)

Not affected by luminosity. PWM values are 0-255.

| Address                          | Arguments   | Description                                                        |
|----------------------------------|-------------|--------------------------------------------------------------------|
| `/hourglass/{id}/pwm/up`         | `i [0-255]` | Set PWM value for the UP electromagnet.                            |
| `/hourglass/{id}/pwm/down`       | `i [0-255]` | Set PWM value for the DOWN electromagnet.                          |
| `/hourglass/{id}/pwm/all`        | `i [0-255]` | Set PWM value for both UP and DOWN electromagnets.                 |

---

## Examples
```
# Connect and setup Hourglass 1
/hourglass/1/connect
/hourglass/1/motor/enable i 1
/system/luminosity f 0.8                # Global brightness to 80%
/hourglass/1/luminosity f 0.5           # HG1 individual brightness to 50% (effective: 0.8*0.5 = 40%)

# Control LEDs for HG1
/hourglass/1/up/rgb 255 0 128             # Set UP LED to magenta
/hourglass/1/down/brightness 200          # Set DOWN LED brightness
/hourglass/1/up/blend 400                 # Set UP LED blend effect to 400
/hourglass/1/down/origin 180              # Set DOWN LED origin to 180 degrees
/hourglass/1/led/all/arc 270              # Set both LEDs arc to 270 degrees
/hourglass/1/main/all 100                 # Set both main LEDs to brightness 100
/hourglass/1/pwm/up 255                   # UP electromagnet full power

# Restore brightness
/system/luminosity f 1.0                # Global brightness to 100% (HG1 still at 0% due to its individual setting)
/hourglass/1/luminosity f 1.0           # HG1 individual brightness to 100% (HG1 now fully bright)

# Global blackout
/blackout                               # All hourglasses effectively off (global luminosity set to 0)

# Motor preset and system-wide movement
/system/motor/preset s fast             # All hourglasses will use "fast" defaults for next moves
/hourglass/1/motor/config/250/120       # HG1 default speed=250, accel=120 (overrides preset for HG1)
/system/motor/rotate/-90                # Rotate ALL HGs -90 deg. HG1 uses 250/120, others use "fast" defaults.
```

## Global Commands
```
/blackout                           # Turn off ALL LEDs (RGB and Main) on ALL connected hourglasses
/system/emergency_stop_all         # Stop all motors on ALL connected hourglasses
```

## Hourglass Specific Commands

### Connection
```
/hourglass/connect                  # Connect all hourglasses
/hourglass/disconnect              # Disconnect all hourglasses
/hourglass/{id}/connect            # Connect specific hourglass (id = 1, 2, 3...)
/hourglass/{id}/disconnect         # Disconnect specific hourglass
```

### Blackout (Individual)
```
/hourglass/{id}/blackout           # Turn off ALL LEDs (RGB and Main) on a specific hourglass
```

### Motor Control
Replace `{id}` with the target hourglass ID (e.g., 1, 2, ...).
```
/hourglass/{id}/motor/enable [1|0]              # Enable (1) or disable (0) the motor
/hourglass/{id}/motor/emergency_stop            # Immediately stop the motor
/hourglass/{id}/motor/speed [0-500]             # Set the default speed for subsequent moves (0-500 range)
/hourglass/{id}/motor/acceleration [0.0-1.0]    # Set default acceleration (0.0 to 1.0 float)

# Path-based format (Preferred for new integrations):
# - Allows specifying angle, speed, and acceleration directly in the OSC path.
# - If speed or accel are omitted, uses the last set default values.
/hourglass/{id}/motor/rotate/{angle_degrees}/{speed?}/{acceleration?}
# Example: /hourglass/1/motor/rotate/90/200/0.5  (Rotate 90 deg, speed 200, accel 0.5)
# Example: /hourglass/1/motor/rotate/-45         (Rotate -45 deg, use default speed & accel)

/hourglass/{id}/motor/position/{angle_degrees}/{speed?}/{acceleration?}
# Example: /hourglass/1/motor/position/180/100/0.2 (Move to 180 deg, speed 100, accel 0.2)
# Example: /hourglass/1/motor/position/0           (Move to 0 deg, use default speed & accel)

# Parameter format (Legacy, still supported):
# - Speed and acceleration are optional and use defaults if not provided.
/hourglass/{id}/motor/rotate [degrees] [speed?] [acceleration?]
/hourglass/{id}/motor/position [degrees] [speed?] [acceleration?]
```

### LED Control (RGB and Main)
Replace `{id}` with the target hourglass ID.
Values for R, G, B, and brightness are typically 0-255.
```
/hourglass/{id}/led/all/rgb [r] [g] [b]        # Set RGB color for both UP and DOWN LEDs
/hourglass/{id}/up/rgb [r] [g] [b]             # Set RGB color for the UP LED only
/hourglass/{id}/down/rgb [r] [g] [b]           # Set RGB color for the DOWN LED only

/hourglass/{id}/up/brightness [0-255]        # Set brightness for UP LED (monochromatic, sets R=G=B)
/hourglass/{id}/down/brightness [0-255]      # Set brightness for DOWN LED (monochromatic)

/hourglass/{id}/main/up [0-255]               # Set brightness for the UP main LED
/hourglass/{id}/main/down [0-255]             # Set brightness for the DOWN main LED
/hourglass/{id}/main/all [0-255]              # Set brightness for both UP and DOWN main LEDs
```

### LED Effect Parameters
Replace `{id}` with the target hourglass ID.
```
/hourglass/{id}/up/blend [0-768]              # Set blend effect parameter for UP LED only
/hourglass/{id}/down/blend [0-768]            # Set blend effect parameter for DOWN LED only
/hourglass/{id}/led/all/blend [0-768]         # Set blend effect parameter for both UP and DOWN LEDs

/hourglass/{id}/up/origin [0-360]             # Set origin angle (degrees) for UP LED effect
/hourglass/{id}/down/origin [0-360]           # Set origin angle (degrees) for DOWN LED effect
/hourglass/{id}/led/all/origin [0-360]        # Set origin angle (degrees) for both UP and DOWN LED effects

/hourglass/{id}/up/arc [0-360]                # Set arc angle (degrees) for UP LED effect
/hourglass/{id}/down/arc [0-360]              # Set arc angle (degrees) for DOWN LED effect
/hourglass/{id}/led/all/arc [0-360]           # Set arc angle (degrees) for both UP and DOWN LED effects
```

### PWM Control (Electromagnets)
Replace `{id}` with the target hourglass ID.
PWM values are typically 0-255.
```
/hourglass/{id}/pwm/up [0-255]                # Set PWM value for the UP electromagnet
/hourglass/{id}/pwm/down [0-255]              # Set PWM value for the DOWN electromagnet
/hourglass/{id}/pwm/all [0-255]               # Set PWM value for both UP and DOWN electromagnets
```

## System Commands
```
/system/list_devices               # Logs available serial devices to the console (no OSC response)
/system/emergency_stop_all         # Stops motors on ALL connected hourglasses
```

## Examples
```
/hourglass/1/connect
/hourglass/1/motor/enable 1
/hourglass/1/motor/speed 200
/hourglass/1/motor/acceleration 0.75

/hourglass/1/motor/rotate/90                # Rotate 90 deg (uses current speed/accel)
/hourglass/1/motor/position/0/100/0.5     # Move to 0 deg, speed 100, accel 0.5

/hourglass/1/up/rgb 255 0 128             # Set UP LED to magenta
/hourglass/1/down/brightness 200          # Set DOWN LED brightness
/hourglass/1/main/all 100                 # Set both main LEDs to brightness 100
/hourglass/1/pwm/up 255                   # UP electromagnet full power

/blackout                                 # All LEDs on all hourglasses OFF
/hourglass/2/blackout                     # All LEDs on hourglass 2 OFF
```

## Important Notes
- **Luminosity Interaction**: Final LED brightness for an hourglass = `BaseColor * GlobalLuminosity * IndividualLuminosity`.
- **Motor Config vs. Movement**: `/motor/speed`, `/motor/acceleration`, `/motor/preset`, and `/motor/config` commands ONLY set default parameters for an hourglass or system-wide. They do NOT initiate movement. Actual movement commands (`/rotate`, `/position`) will use these defaults if speed/acceleration are not provided directly in the movement command itself.
- **Blackout Behavior**:
    - `/blackout` sets GlobalLuminosity to 0.
    - `/hourglass/{id}/blackout` sets the IndividualLuminosity for that specific hourglass to 0.
- **No OSC Responses**: Check application console for status and error logs.
- **Parameter Order**: For motor movement commands with optional speed/accel in parameter format, provide preceding ones if specifying later ones. Path format is generally clearer.
- **GUI Synchronization**: The application's UI should reflect changes made via OSC in real-time. 