# OSC Test Script for HourGlass

Interactive Python script to test OSC commands for the HourGlass LED system.

## Setup

```bash
# Install dependency
pip install python-osc

# Or install from requirements.txt
pip install -r requirements.txt
```

## Usage

```bash
# Interactive mode (default)
python test_osc_commands.py

# Automated test sequence
python test_osc_commands.py auto
```

## Interactive Commands

| Command | Format | Example | Description |
|---------|--------|---------|-------------|
| `rgb` | `rgb <hg> <r> <g> <b>` | `rgb 1 255 0 0` | Set RGB color on all LEDs |
| `up` | `up <hg> <r> <g> <b>` | `up 1 255 255 0` | Set RGB color on UP LEDs only |
| `down` | `down <hg> <r> <g> <b>` | `down 1 0 255 255` | Set RGB color on DOWN LEDs only |
| `blend` | `blend <hg> <value>` | `blend 1 384` | Set LED blend (0-768) |
| `origin` | `origin <hg> <degrees>` | `origin 1 90` | Set LED origin (0-360°) |
| `arc` | `arc <hg> <degrees>` | `arc 1 180` | Set LED arc (0-360°) |
| `lum` | `lum <hg> <value>` | `lum 1 0.5` | Set individual luminosity (0-1) |
| `global` | `global <value>` | `global 0.8` | Set global luminosity (0-1) |
| `blackout` | `blackout` | `blackout` | Turn off all LEDs |
| `demo` | `demo` | `demo` | Run quick color demo |
| `help` | `help` | `help` | Show command help |
| `quit` | `quit` | `quit` | Exit program |

## Hourglass ID Formats

- Single: `1`, `2`, `3`
- All: `all`
- Range: `1-3`
- List: `1,2,3`

## Example Session

```
OSC> rgb 1 255 0 0        # Red on hourglass 1
OSC> rgb all 0 255 0      # Green on all hourglasses
OSC> up 1 255 255 0       # Yellow on UP LEDs only
OSC> down 1 255 0 255     # Magenta on DOWN LEDs only
OSC> blend 1 384          # Switch to middle LED circle
OSC> arc 1 180            # Half circle
OSC> origin 1 90          # Start at 90 degrees
OSC> lum 1 0.5            # 50% brightness
OSC> blackout             # Turn everything off
OSC> quit                 # Exit
```

## Testing the "/all" Commands Fix

The main issue was that `/hourglass/1/led/all/rgb` commands from Vezer weren't affecting both UP and DOWN LEDs. Test this fix:

```
OSC> rgb 1 255 0 0        # Should affect BOTH up and down LEDs
OSC> up 1 0 255 0         # Should affect UP LED only  
OSC> down 1 0 0 255       # Should affect DOWN LED only
OSC> rgb 1 255 255 255    # Should reset both to white
```

If the fix works correctly, you should see both LEDs change color with the `rgb` command!
