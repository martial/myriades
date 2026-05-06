# Myriades OpenFrameworks Setup

This repository contains an automated setup script for the Myriades OpenFrameworks project with OSC output capabilities.

## 🚀 Quick Start

### Option 1: One-Command Setup

```bash
curl -sSL https://raw.githubusercontent.com/martial/myriades/main/setup_openframeworks_project.sh | bash
```

### Option 2: Download and Run

```bash
# Download the setup script
curl -O https://raw.githubusercontent.com/martial/myriades/main/setup_openframeworks_project.sh
chmod +x setup_openframeworks_project.sh

# Run the setup
./setup_openframeworks_project.sh
```

### Option 3: Clone and Build

```bash
# Clone the repository first
git clone https://github.com/martial/myriades.git
cd myriades

# Run the setup script
./setup_openframeworks_project.sh
```

## 📋 What the Script Does

1. **System Check**: Verifies macOS and required tools (Xcode CLI tools, Git)
2. **Download OpenFrameworks**: Downloads OF v0.12.1 for macOS
3. **Clone Project**: Clones the Myriades project repository
4. **Compile Libraries**: Builds OpenFrameworks core libraries
5. **Build Project**: Compiles the Myriades application
6. **Create Run Scripts**: Sets up convenient run scripts

## 🎯 Requirements

- **macOS** (tested on macOS 10.15+)
- **Xcode Command Line Tools**: `xcode-select --install`
- **Git**: Usually pre-installed on macOS
- **Internet Connection**: For downloading OpenFrameworks and cloning repo

## 📁 Project Structure After Setup

```
of_v0.12.1_osx_release/
├── apps/
│   └── myApps/
│       └── emptyExample/          # ← Your project is here
│           ├── src/
│           │   ├── HourGlass.cpp
│           │   ├── OSCOutController.cpp
│           │   └── ...
│           ├── bin/
│           │   └── data/
│           │       └── hourglasses.json
│           └── Makefile
├── libs/
│   └── openFrameworks/
└── ...
```

## 🏃‍♂️ Running the Application

After setup completes, you can run the application in several ways:

### Using the Generated Run Script
```bash
./run_emptyExample.sh
```

### Manual Method
```bash
cd of_v0.12.1_osx_release/apps/myApps/emptyExample
make RunRelease
```

### Development Method (with automatic recompilation)
```bash
cd of_v0.12.1_osx_release/apps/myApps/emptyExample
make clean && make Release && make RunRelease
```

## 🎛️ OSC Configuration

The application includes a complete OSC output system:

### Features
- ✅ **OSC Output Controller** with JSON configuration
- ✅ **Position-based addressing**: `/rgb/top`, `/rgb/bot`, `/pwr/top`, `/pwr/bot`, `/mag/top`, `/mag/bot`
- ✅ **Motor control**: `/motor/zero`, `/motor/relative`, `/motor/absolute`, `/motor/emergency`
- ✅ **Change detection** to prevent message flooding
- ✅ **Embedded configuration** in `hourglasses.json`

### Default OSC Setup
The application sends OSC messages to:
- **HourGlass1**: `localhost:9001`
- **HourGlass2**: `localhost:9002`

### Testing with Osculator
1. Download [Osculator](https://osculator.net/)
2. Set listen port to `9002` (for HourGlass2)
3. Run the application and select HourGlass2 (key `2`)
4. Adjust LED controls and observe OSC messages

## 🔧 Configuration Files

### `bin/data/hourglasses.json`
Contains the main configuration including OSC settings:

```json
{
  "name": "HourGlass2",
  "upLedId": 12,
  "downLedId": 22,
  "motorId": 2,
  "oscOut": {
    "enabled": true,
    "destinations": [
      {
        "name": "localhost_test_hg2",
        "ip": "127.0.0.1",
        "port": 9002,
        "enabled": true
      }
    ]
  }
}
```

## 🎮 Controls

### Keyboard Shortcuts
- `1-2`: Select HourGlass
- `c`: Connect all
- `x`: Disconnect all
- `z`: Set motor zero
- `s`: Emergency stop
- `Arrow keys`: Rotate motor
- `u/d`: Move motor steps
- `v`: Toggle view mode

### GUI Panels
- **Settings & Actions**: HourGlass selection, global controls
- **Motor**: Movement commands, speed/acceleration
- **Up/Down LED Controllers**: RGB, Main LED, PWM controls
- **Module Luminosity**: Individual and global brightness
- **Effects**: Add/clear visual effects

## 🚨 Troubleshooting

### Build Errors
```bash
# Clean and rebuild
cd of_v0.12.1_osx_release/apps/myApps/emptyExample
make clean
make Release
```

### Missing Dependencies
```bash
# Install Xcode command line tools
xcode-select --install

# Update macOS if needed
softwareupdate -i -a
```

### OSC Not Working
1. Check `hourglasses.json` configuration
2. Verify target application is listening on correct port
3. Check console for OSC output logs
4. Ensure firewall allows local connections

### Permission Issues
```bash
# Fix script permissions
chmod +x setup_openframeworks_project.sh
chmod +x quick-start.sh
```

## 📚 Documentation

- **OSC Protocol**: See `OSC_OUT_DOCUMENTATION.md`
- **Integration Guide**: See `INTEGRATED_OSC_DOCUMENTATION.md`
- **OpenFrameworks**: [openframeworks.cc](https://openframeworks.cc/)

## 🤝 Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Test thoroughly
5. Submit a pull request

## 📄 License

This project is part of the Myriades interactive installation system.

---

**Happy coding! 🎨✨**
