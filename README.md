# Myriades Project - HourGlass LED Control System

![Myriades HourGlass Control System](emptyExample.png)

An open source comprehensive control system for multi-hourglass LED installations built with openFrameworks. This application provides intuitive GUI controls and extensive OSC API support for managing LED effects, motor controls, and electromagnetic systems across multiple hourglass units.

## Features

### LED Control System
- **RGB LED Control**: Full color control for UP and DOWN LEDs on each hourglass
- **Effect Parameters**: Advanced blend, origin, and arc parameters for LED effects
- **Main LED Support**: Individual brightness control for main LEDs
- **Global & Individual Luminosity**: Dual-layer brightness control system
- **Gamma Correction**: Built-in gamma correction and minimum threshold optimization

### Motor Control
- **Precise Movement**: Absolute and relative angle positioning
- **Speed & Acceleration**: Configurable movement parameters
- **Motor Presets**: Pre-defined speed/acceleration combinations (slow, smooth, medium, fast)
- **Emergency Stop**: Safety controls for immediate motor shutdown
- **Gear Ratio Support**: Customizable gear ratios and calibration factors

### Electromagnetic Controls
- **PWM Control**: Variable electromagnetic field strength control
- **Independent Control**: Separate UP/DOWN electromagnet management

### OSC API
- **Multi-Targeting**: Support for single IDs, comma-separated lists (1,3), ranges (1-3), and "all"
- **Comprehensive Commands**: 40+ OSC commands for complete system control
- **Real-time Control**: Low-latency command processing and hardware communication

### User Interface
- **Multi-Hourglass Management**: Support for multiple connected hourglasses
- **Real-time Monitoring**: Connection status and serial communication statistics
- **Parameter Sliders**: Intuitive controls for all LED and motor parameters
- **Preset Management**: Save and load motor configuration presets

## Quick Start

### Prerequisites
- openFrameworks 0.12.1 or later
- macOS (current build configuration)
- Serial USB connection to hourglass hardware

### Building
```bash
# Clone the repository
git clone https://github.com/YOUR_USERNAME/myriades-project.git
cd myriades-project

# Build using make
make Release

# Or open in Xcode
open emptyExample.xcodeproj
```

### Running
```bash
# Run the built application
./bin/emptyExample.app/Contents/MacOS/emptyExample

# Or use the build script
./build_and_run.sh
```

## OSC API Examples

### Basic LED Control
```bash
# Set red color on all hourglasses
/hourglass/all/led/all/rgb 255 0 0

# Set blend effect on hourglasses 1 and 3
/hourglass/1,3/up/blend 400

# Set arc effect on hourglass range 1-3
/hourglass/1-3/down/arc 180
```

### Motor Control
```bash
# Rotate all hourglasses by 90 degrees
/system/motor/rotate/90

# Move hourglass 2 to absolute position 180°
/hourglass/2/motor/position/180/200/100

# Apply smooth preset to all hourglasses
/system/motor/preset smooth
```

### System Control
```bash
# Global blackout
/blackout

# Set global luminosity to 50%
/system/luminosity 0.5

# Emergency stop all motors
/system/emergency_stop_all
```

## Multi-Targeting Syntax

The system supports flexible targeting for LED, PWM, and connection commands:

- **Single ID**: `/hourglass/1/led/all/rgb 255 0 0`
- **Comma-separated**: `/hourglass/1,3,5/up/blend 200`
- **Range**: `/hourglass/1-4/down/origin 90`
- **All units**: `/hourglass/all/pwm/all 128`

## Project Structure

```
src/
├── OSCController.*         # OSC message handling and routing
├── HourGlassManager.*      # Multi-hourglass management
├── HourGlass.*             # Individual hourglass control
├── LedMagnetController.*   # LED and electromagnet hardware control
├── MotorController.*       # Motor movement and control
├── SerialPortManager.*     # Serial communication management
├── UIWrapper.*             # GUI interface and controls
├── OSCHelper.*             # OSC utility functions
└── ofApp.*                 # Main application entry point

OSC_API.csv                 # Complete OSC command documentation
OSC_API_Documentation.md    # Detailed API documentation
```

## Hardware Communication

The system communicates with hourglass hardware via serial protocol using a custom CAN-like frame structure:

```
Frame Format: [START_BYTE][FLAGS][ID_BYTES][DATA][CHECKSUM][END_BYTE]
- RGB Commands: 8 bytes (command + RGB + 4 effect parameter bytes)
- Main LED: 2 bytes (command + brightness)
- PWM: 2 bytes (command + PWM value)
```

## Serial Statistics

Real-time monitoring of serial communication with rolling averages:
- Commands per second (instant, 1s, 5s, 60-frame)
- Bytes per second tracking
- Connection status monitoring

## Open Source

This project is released under the MIT License, making it free to use, modify, and distribute. We welcome contributions from the community to help improve and extend the system's capabilities.

## Contributing

We welcome contributions to the Myriades Project! Here's how you can help:

1. **Fork the repository** on GitHub
2. **Create a feature branch** (`git checkout -b feature/amazing-feature`)
3. **Commit your changes** (`git commit -m 'Add amazing feature'`)
4. **Push to the branch** (`git push origin feature/amazing-feature`)
5. **Open a Pull Request**

### Development Guidelines
- Follow the existing code style and structure
- Add documentation for new features
- Test your changes thoroughly
- Update the OSC API documentation if adding new commands

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Technical Specifications

- **OSC Port**: 8000 (default)
- **Serial Baud Rate**: Configurable per device
- **LED Color Range**: 0-255 RGB
- **Motor Speed Range**: 0-500
- **Motor Acceleration**: 0-255
- **PWM Range**: 0-255
- **Effect Parameters**: Blend (0-768), Origin/Arc (0-360°)

## Support

For questions, bug reports, or feature requests, please open an issue on GitHub.

## Acknowledgments

Built with openFrameworks and ofxOsc addon for robust real-time multimedia control. This open source project is part of the Myriades initiative.