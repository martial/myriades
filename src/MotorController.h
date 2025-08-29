#pragma once

#include "ofMain.h"
#include <memory>

class MotorController {
public:
	// Motor command types
	enum class MotorCommand : uint8_t {
		ENABLE = 0xF3,
		SET_USTEP = 0x84,
		SET_ZERO = 0x92,
		EMERGENCY_STOP = 0xF7,
		MOVE_RELATIVE = 0xF4,
		MOVE_ABSOLUTE = 0xF5
	};

	// Connection result enum for better error handling
	enum class ConnectionResult {
		SUCCESS,
		DEVICE_NOT_FOUND,
		CONNECTION_FAILED,
		INVALID_INDEX,
		PORT_IN_USE
	};

	// Constructor for OSC-only operation
	MotorController();
	~MotorController();

	// Setup and connection
	ConnectionResult connect(const std::string & portName, int baudRate = 230400);
	ConnectionResult connect(int deviceIndex, int baudRate = 230400);
	bool isConnected() const;
	void disconnect();

	// Device management
	std::vector<std::string> getAvailableDevices();
	std::string getConnectedDeviceName() const;

	// Fluent interface for configuration
	MotorController & setId(int _id);
	MotorController & setExtended(bool extended);
	MotorController & setRemote(bool remote);

	// Motor control functions with fluent interface
	MotorController & enable(bool enabled = true);
	MotorController & disable() { return enable(false); }
	MotorController & setMicrostep(int ustep);
	MotorController & setZero();
	MotorController & emergencyStop();

	// Movement commands
	MotorController & moveRelative(int speed, int accel, int axis);
	MotorController & moveAbsolute(int speed, int accel, int axis);
	MotorController & stopRelative(int accel);
	MotorController & stopAbsolute(int accel);

	// Angle-based movement commands
	MotorController & moveRelativeAngle(int speed, int accel, float degrees, float gearRatio, float calibrationFactor);
	MotorController & moveAbsoluteAngle(int speed, int accel, float degrees, float gearRatio, float calibrationFactor);

	// Generic send method
	bool send(const std::vector<uint8_t> & data);

	// Current motor state
	int getCurrentId() const { return id; }
	bool isExtended() const { return ext; }
	bool isRemote() const { return rtr; }
	int getCurrentMicrostep() const { return currentMicrostep; }

	// Conversion utilities
	int degreesToAxis(float degrees, float gearRatio, float calibrationFactor) const;
	float axisToDegrees(int axis, float gearRatio, float calibrationFactor) const;

	// Calibration utilities
	void startCalibration();
	float measureActualRotation(float commandedDegrees, float gearRatio, float calibrationFactor);
	float calculateOptimalGearRatio(float commandedDegrees, float actualDegrees, float currentGearRatio);
	float calculateOptimalCalibrationFactor(float commandedDegrees, float actualDegrees, float gearRatio);
	void logCalibrationData(float commanded, float actual, float gearRatio, float calibrationFactor);

private:
	std::string connectedPortName;

	// Protocol parameters
	int id = 1;
	bool ext = false;
	bool rtr = false;
	int currentMicrostep = 16; // Track current microstep setting
	static constexpr uint8_t START_BYTE = 0xE7; // 231
	static constexpr uint8_t END_BYTE = 0x7E; // 126

	// Encoder constants from manual
	static constexpr int ENCODER_COUNTS_PER_REVOLUTION = 0x4000; // 16384 counts = 360 degrees

	// State for preventing redundant commands
	bool motorEnabledStateInitialized = false;
	bool lastMotorEnabledState = false;
	bool microstepInitialized = false;
	int lastMicrostepValue = 0;
	// Note: Movement commands are already handled by HourGlass's flag system
	// to be one-shot, so MotorController doesn't need to cache last move target/speed/accel.

	// Build protocol packet (same as CAN protocol)
	std::vector<uint8_t> buildPacket(const std::vector<uint8_t> & data) const;

	// Helper functions
	template <typename T>
	static T clamp(T value, T min, T max) {
		return value < min ? min : (value > max ? max : value);
	}

	// Convert multi-byte values to byte arrays
	std::vector<uint8_t> speedToBytes(int speed) const;
	std::vector<uint8_t> axisToBytes(int axis) const;
};