# Integrated OSC Out System - Complete Documentation

## 🎯 **Overview**

The OSC Out system is now **fully integrated** into the HourGlass configuration. Each HourGlass can automatically send OSC messages when motor or LED commands are executed, with all configuration stored directly in `hourglasses.json`.

## 📁 **Configuration Structure**

### **Single Configuration File: `bin/data/hourglasses.json`**

```json
{
  "serialPort": "tty.usbmodem101",
  "baudRate": 230400,
  "hourglasses": [
    {
      "name": "HourGlass1",
      "upLedId": 11,
      "downLedId": 21,
      "motorId": 1,
      "oscOut": {
        "enabled": true,
        "destinations": [
          {
            "name": "main_controller_hg1",
            "ip": "192.168.1.100",
            "port": 9000,
            "enabled": true
          },
          {
            "name": "backup_controller_hg1", 
            "ip": "192.168.1.101",
            "port": 9000,
            "enabled": false
          }
        ]
      }
    },
    {
      "name": "HourGlass2",
      "upLedId": 12,
      "downLedId": 22,
      "motorId": 2,
      "oscOut": {
        "enabled": true,
        "destinations": [
          {
            "name": "main_controller_hg2",
            "ip": "192.168.1.100",
            "port": 9001,
            "enabled": true
          }
        ]
      }
    }
  ]
}
```

## 🚀 **Automatic Setup Process**

### **1. Application Startup**
```cpp
// In ofApp::setup() - No manual OSC configuration needed!
hourglassManager.loadConfiguration("hourglasses.json");  // Loads OSC config automatically
hourglassManager.connectAll();                           // Connects hardware + OSC
```

### **2. Behind the Scenes**
```cpp
// HourGlassManager::parseHourGlassJson() automatically:
// 1. Creates HourGlass with hardware config
// 2. Parses oscOut section if present
// 3. Calls hg->setupOSCOutFromJson(oscConfig)
// 4. Enables OSC Out automatically

// Each HourGlass is now ready to send OSC messages!
```

## 📡 **OSC Message Flow**

### **Motor Commands → OSC Messages**

| **Hardware Command** | **OSC Message** | **Parameters** |
|---------------------|-----------------|----------------|
| `hg->setMotorZero()` | `/motor/zero` | - |
| `hg->emergencyStop()` | `/motor/emergency` | - |
| `hg->commandRelativeAngle(90.0f)` | `/motor/relative` | `[speed] [accel] [90.0]` |
| `hg->commandAbsoluteAngle(180.0f)` | `/motor/absolute` | `[speed] [accel] [180.0]` |
| `hg->applyMotorParameters()` | `/motor/ustep` | `[value]` |

### **LED Commands → OSC Messages**

| **Hardware Command** | **OSC Message** | **Parameters** |
|---------------------|-----------------|----------------|
| `hg->applyLedParameters()` | `/rgb/{deviceId}` | `[RGBA] [origin] [arc]` |
| | `/pwr/{deviceId}` | `[PWM]` |
| | `/mag/{deviceId}` | `[mainLED_PWM]` |

## 🔧 **Device ID Mapping**

Each HourGlass controls multiple devices with specific IDs:

```cpp
// From hourglasses.json:
"name": "HourGlass1",
"upLedId": 11,      // → /rgb/11, /pwr/11, /mag/11
"downLedId": 21,    // → /rgb/21, /pwr/21, /mag/21  
"motorId": 1        // → /motor/* (device 1)
```

## ⚡ **Smart Features**

### **✅ Feedback Loop Prevention**
```cpp
// OSC messages are only sent when NOT updating from incoming OSC
if (isOSCOutEnabled() && !updatingFromOSC) {
    oscOutController->sendMotorRelative(motorId, speed, accel, degrees);
}
```

### **✅ Per-Device Configuration**
- Each HourGlass can send to different IP addresses/ports
- Independent enable/disable per destination
- Automatic configuration loading and saving

### **✅ Multi-Destination Broadcasting**
```json
"destinations": [
  {"name": "main", "ip": "192.168.1.100", "port": 9000, "enabled": true},
  {"name": "backup", "ip": "192.168.1.101", "port": 9000, "enabled": false},
  {"name": "local", "ip": "127.0.0.1", "port": 9002, "enabled": true}
]
```

## 💻 **Usage Examples**

### **Basic Motor Control**
```cpp
auto hg = hourglassManager.getHourGlass("HourGlass1");

// These commands send both hardware AND OSC messages automatically:
hg->setMotorZero();                    // Hardware + OSC: /motor/zero
hg->commandRelativeAngle(90.0f);       // Hardware + OSC: /motor/relative [speed] [accel] [90.0]
hg->emergencyStop();                   // Hardware + OSC: /motor/emergency
```

### **LED Control**
```cpp
// Set LED color and apply - sends hardware + OSC automatically
hg->upLedColor.set(ofColor::red);
hg->upLedOrigin.set(45);              // 45° origin
hg->upLedArc.set(180);                // 180° arc
hg->applyLedParameters();             // Hardware + OSC: /rgb/11 [RGBA] [45] [180]
```

### **Parameter Synchronization**
```cpp
// Changes to parameters automatically send OSC when applied
hg->motorSpeed.set(200);              // Will send OSC with next motor command
hg->upLedColor.set(ofColor::blue);    // Will send OSC when applyLedParameters() called
```

## 🔍 **Configuration Validation**

The system automatically validates configuration and provides helpful logging:

```
[notice] HourGlassManager: 📡 Setting up OSC Out for: HourGlass1
[notice] OSCOutController: 📋 Loaded configuration from JSON  
[notice] OSCOutController: 📡 Configured 2 destinations
[notice] HourGlass: HourGlass1 - OSC Out configured from JSON
[notice] HourGlassManager: ✅ OSC Out configured for: HourGlass1
```

## 🛠️ **Advanced Configuration**

### **Runtime OSC Control**
```cpp
// Enable/disable OSC for specific HourGlass
hg->enableOSCOut(false);              // Disable OSC sending
hg->enableOSCOut(true);               // Re-enable OSC sending

// Check OSC status
if (hg->isOSCOutEnabled()) {
    auto oscOut = hg->getOSCOut();
    int messagesSent = oscOut->getSentMessageCount();
}
```

### **Dynamic Destination Management**
```cpp
// Add new destination at runtime
auto oscOut = hg->getOSCOut();
oscOut->addDestination("new_dest", "192.168.1.200", 9010);
oscOut->setDestinationEnabled("backup_controller_hg1", true);
```

### **Save Configuration Changes**
```cpp
// Save current configuration back to JSON
hourglassManager.saveConfiguration("hourglasses.json");
```

## 🎯 **Benefits of Integrated Approach**

✅ **Single Configuration File** - Everything in `hourglasses.json`  
✅ **Automatic Setup** - No manual OSC configuration code needed  
✅ **Type Safety** - Consistent device ID mapping  
✅ **Easy Maintenance** - One file to manage both hardware and OSC  
✅ **Version Control Friendly** - All config in one trackable file  
✅ **Backup & Restore** - Simple JSON file backup  

## 🚨 **Troubleshooting**

### **OSC Not Sending**
1. Check `"enabled": true` in JSON configuration
2. Verify IP addresses and ports are correct
3. Ensure `hg->isOSCOutEnabled()` returns `true`
4. Check console for OSC setup messages

### **Configuration Not Loading**
1. Verify JSON syntax with a JSON validator
2. Check `bin/data/hourglasses.json` file exists
3. Look for parsing error messages in console
4. Ensure `oscOut` section is properly nested

### **Device ID Conflicts**
1. Ensure each device has a unique ID across all HourGlasses
2. Check that `upLedId`, `downLedId`, and `motorId` don't overlap
3. Verify OSC messages use correct device IDs in logs

## 🎉 **Ready to Use!**

The integrated OSC Out system is now fully configured and ready to use. Every motor movement and LED change will automatically send corresponding OSC messages to your configured destinations, maintaining perfect synchronization between your hardware control and external systems.

**No additional setup required - just configure your `hourglasses.json` and run!** 🚀
