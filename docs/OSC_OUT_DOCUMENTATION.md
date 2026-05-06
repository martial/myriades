# OSCOutController Documentation

## Overview

The `OSCOutController` enables your openFrameworks application to send OSC messages according to the specified protocol for motor control, electromagnets, power LEDs, and RGB LED circles.

## Features

✅ **Motor Control**: Send motor commands (zero, homing, emergency, relative/absolute moves)  
✅ **Electromagnet Control**: Control electromagnet PWM (0-255)  
✅ **Power LED Control**: Control power LED PWM (0-255)  
✅ **RGB LED Circles**: Send RGBA color data with origin/arc parameters  
✅ **JSON Configuration**: Configure multiple OSC destinations via JSON  
✅ **Multi-destination**: Send to multiple IP addresses simultaneously  
✅ **Validation**: Parameter validation with range checking  
✅ **Statistics**: Track sent message counts  

## Quick Start

### 1. Configuration

Create `bin/data/osc_out_config.json`:

```json
{
  "enabled": true,
  "destinations": [
    {
      "name": "main_controller",
      "ip": "192.168.1.100", 
      "port": 9000,
      "enabled": true
    },
    {
      "name": "backup_controller",
      "ip": "192.168.1.101",
      "port": 9000, 
      "enabled": false
    }
  ]
}
```

### 2. Integration

```cpp
// In your header (.h):
#include "OSCOutController.h"

class ofApp : public ofBaseApp {
    std::unique_ptr<OSCOutController> oscOutController;
};

// In your implementation (.cpp):
void ofApp::setup() {
    oscOutController = std::make_unique<OSCOutController>();
    oscOutController->setup(); // Loads config automatically
}
```

### 3. Usage Examples

```cpp
// Motor Commands
oscOutController->sendMotorZero();                              // /motor/zero
oscOutController->sendMotorHoming();                            // /motor/homing  
oscOutController->sendMotorEmergency();                         // /motor/emergency
oscOutController->sendMotorUstep(1, 64);                       // /motor/ustep [64]
oscOutController->sendMotorRelative(1, 10.0f, 5.0f, 90.0f);   // /motor/relative [10.0] [5.0] [90.0]
oscOutController->sendMotorAbsolute(1, 15.0f, 8.0f, 180.0f);  // /motor/absolute [15.0] [8.0] [180.0]

// Electromagnet Control
oscOutController->sendMagnet(1, 128);                          // /mag/1 [128] (50% PWM)

// Power LED Control  
oscOutController->sendPowerLED(1, 255);                        // /pwr/1 [255] (100% PWM)

// RGB LED Control
oscOutController->sendRGBLED(1, 255, 100, 50, 200, 0, 180);   // /rgb/1 [RGBA] [0] [180]
oscOutController->sendRGBLED(1, ofColor::red, 255, 45, 90);   // Using ofColor
```

## API Reference

### Configuration Methods

```cpp
void setup();                                          // Load config and initialize
void loadConfiguration(const std::string& path);      // Load specific config file
void saveConfiguration(const std::string& path);      // Save current config

void addDestination(const std::string& name, const std::string& ip, int port);
void removeDestination(const std::string& name);
void setDestinationEnabled(const std::string& name, bool enabled);
std::vector<OSCDestination> getDestinations() const;
```

### Motor Control Methods

```cpp
void sendMotorZero(int deviceId = -1);
void sendMotorHoming(int deviceId = -1);  
void sendMotorEmergency(int deviceId = -1);
void sendMotorUstep(int deviceId, int ustepValue);            // ustepValue: 1-256
void sendMotorRelative(int deviceId, float speedRotMin, float accDegPerS2, float moveDeg);
void sendMotorRelativeStop(int deviceId, float accDegPerS2);
void sendMotorAbsolute(int deviceId, float speedRotMin, float accDegPerS2, float moveDeg);
void sendMotorAbsoluteStop(int deviceId, float accDegPerS2);
```

**Parameters:**
- `speedRotMin`: Speed in rotations per minute
- `accDegPerS2`: Acceleration in degrees per second²
- `moveDeg`: Movement angle in degrees

### Hardware Control Methods

```cpp
void sendMagnet(int deviceId, int pwmValue);           // pwmValue: 0-255
void sendPowerLED(int deviceId, int pwmValue);         // pwmValue: 0-255
void sendRGBLED(int deviceId, uint8_t r, uint8_t g, uint8_t b, uint8_t alpha, int originDeg, int arcDeg);
void sendRGBLED(int deviceId, const ofColor& color, uint8_t alpha, int originDeg, int arcDeg);
```

**RGB Parameters:**
- `r,g,b`: Color values 0-255
- `alpha`: Alpha channel 0-255 (master brightness)
- `originDeg`: Arc origin angle 0-360°
- `arcDeg`: Arc span 0-360°

### Utility Methods

```cpp
bool isEnabled() const;
void setEnabled(bool enable);
int getSentMessageCount() const;
void resetStats();
```

## OSC Message Format

### Motor Messages
```
/motor/zero                     (no arguments)
/motor/homing                   (no arguments)  
/motor/emergency                (no arguments)
/motor/ustep [int32]            (1-256)
/motor/relative [float] [float] [float]    (speed, accel, move)
/motor/relative/stop [float]    (accel)
/motor/absolute [float] [float] [float]    (speed, accel, move)
/motor/absolute/stop [float]    (accel)
```

### Hardware Messages
```
/mag/{deviceId} [int32]         (PWM 0-255)
/pwr/{deviceId} [int32]         (PWM 0-255)  
/rgb/{deviceId} [int32] [int32] [int32]    (RGBA, origin°, arc°)
```

### RGBA Encoding
The RGB LED command uses a 32-bit integer encoding:
- Bits 31-24: RED (0-255)
- Bits 16-23: GREEN (0-255)  
- Bits 8-15: BLUE (0-255)
- Bits 0-7: ALPHA (0-255)

Example: RGB(255,100,50) + Alpha(200) = `0xFF643200`

## Integration Patterns

### Manual Control
```cpp
// Direct method calls for UI buttons or keyboard shortcuts
void ofApp::keyPressed(int key) {
    switch(key) {
        case 'z': oscOutController->sendMotorZero(); break;
        case 'r': oscOutController->sendMotorRelative(1, 10, 5, 90); break;
    }
}
```

### Parameter Synchronization
```cpp
// Automatically send OSC when HourGlass parameters change
hg->upLedColor.addListener([this, hg](ofColor& color) {
    if (!hg->updatingFromOSC) {  // Avoid OSC feedback loops
        oscOutController->sendRGBLED(hg->getUpLedId(), color, 255, 
                                   hg->upLedOrigin.get(), hg->upLedArc.get());
    }
});
```

### Preset Systems
```cpp
// Send multiple commands as a preset
void sendLightingPreset1() {
    oscOutController->sendRGBLED(1, ofColor::red, 255, 0, 180);
    oscOutController->sendRGBLED(2, ofColor::blue, 255, 180, 180);  
    oscOutController->sendPowerLED(1, 128);
    oscOutController->sendPowerLED(2, 128);
}
```

## Troubleshooting

### Common Issues

1. **No messages sent**: Check `isEnabled()` and destination configuration
2. **Invalid parameters**: Check console for validation warnings
3. **Network issues**: Verify IP addresses and ports in JSON config
4. **Missing JSON**: Default config will be created automatically

### Debug Information

```cpp
// Check status
ofLogNotice() << "OSC Out enabled: " << oscOutController->isEnabled();
ofLogNotice() << "Messages sent: " << oscOutController->getSentMessageCount();

// List destinations
for (const auto& dest : oscOutController->getDestinations()) {
    ofLogNotice() << "Destination: " << dest.name << " (" << dest.ip << ":" 
                  << dest.port << ") " << (dest.enabled ? "enabled" : "disabled");
}
```

## Performance Notes

- Messages are sent immediately when methods are called
- No internal buffering or throttling (add your own if needed)
- Multiple destinations receive identical messages simultaneously
- Validation is performed on each send (minimal overhead)

## Security Considerations

- OSC messages are sent over UDP (no encryption)
- Validate all parameters before sending
- Consider network security when exposing control interfaces
- Emergency stop commands should be easily accessible
