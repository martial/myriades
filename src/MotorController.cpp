#include "MotorController.h"

MotorController::MotorController() {
	// OSC-only constructor - no serial port needed
}

MotorController::~MotorController() {
	disconnect();
}

MotorController::ConnectionResult MotorController::connect(const std::string & portName, int baudRate) {
	// Serial disabled - operating in OSC-only mode
	connectedPortName = portName;
	ofLogNotice("MotorController") << "Motor operating in OSC-only mode (port: " << portName << ")";

	// Reset initialized flags
	microstepInitialized = false;

	return ConnectionResult::SUCCESS;
}

MotorController::ConnectionResult MotorController::connect(int deviceIndex, int baudRate) {
	// Serial disabled - just log the request
	ofLogNotice("MotorController") << "Motor operating in OSC-only mode (device index: " << deviceIndex << ")";
	connectedPortName = "OSC_DEVICE_" + std::to_string(deviceIndex);
	return ConnectionResult::SUCCESS;
}

bool MotorController::isConnected() const {
	// Always connected in OSC-only mode
	return true;
}

void MotorController::disconnect() {
	// Serial disabled - just clear the name
	connectedPortName.clear();
	ofLogNotice("MotorController") << "Motor disconnected (OSC-only mode)";
}

std::vector<std::string> MotorController::getAvailableDevices() {
	// Serial disabled - return empty list
	return std::vector<std::string>();
}

std::string MotorController::getConnectedDeviceName() const {
	return connectedPortName;
}

MotorController & MotorController::setId(int _id) {
	id = _id;
	ext = (id >= 2048); // Auto-set extended flag like in JavaScript
	return *this;
}

MotorController & MotorController::setExtended(bool extended) {
	ext = extended;
	return *this;
}

MotorController & MotorController::setRemote(bool remote) {
	rtr = remote;
	return *this;
}

// Motor control functions
MotorController & MotorController::enable(bool enabled_command) {
	// No-op in OSC-only mode
	return *this;
}

MotorController & MotorController::setMicrostep(int ustep) {
	ustep = ofClamp(ustep, 1, 256);

	if (microstepInitialized && ustep == lastMicrostepValue) {
		return *this; // No change, don't send
	}
	currentMicrostep = ustep;
	send({ static_cast<uint8_t>(MotorCommand::SET_USTEP), static_cast<uint8_t>(ustep % 256) });
	lastMicrostepValue = ustep;
	microstepInitialized = true;
	return *this;
}

MotorController & MotorController::setZero() {
	send({ static_cast<uint8_t>(MotorCommand::SET_ZERO) });
	return *this;
}

MotorController & MotorController::emergencyStop() {
	send({ static_cast<uint8_t>(MotorCommand::EMERGENCY_STOP) });
	return *this;
}

MotorController & MotorController::sendMove(MotorCommand command, int speed, int accel /* 0-255 */, int axis) {
	speed = ofClamp(speed, 0, 500);
	uint8_t hardwareAccel = static_cast<uint8_t>(ofClamp(accel, 0, 255));
	axis = ofClamp(axis, -8388607, 8388607);

	std::vector<uint8_t> data = { static_cast<uint8_t>(command) };

	// Add speed bytes (16-bit, big endian)
	data.push_back((speed >> 8) & 0xFF);
	data.push_back(speed & 0xFF);

	// Add scaled acceleration byte
	data.push_back(hardwareAccel);

	// Add axis bytes (24-bit, big endian)
	data.push_back((axis >> 16) & 0xFF);
	data.push_back((axis >> 8) & 0xFF);
	data.push_back(axis & 0xFF);

	send(data);
	return *this;
}

MotorController & MotorController::moveRelative(int speed, int accel, int axis) {
	return sendMove(MotorCommand::MOVE_RELATIVE, speed, accel, axis);
}

MotorController & MotorController::moveAbsolute(int speed, int accel, int axis) {
	return sendMove(MotorCommand::MOVE_ABSOLUTE, speed, accel, axis);
}

// Angle-based movement implementations
MotorController & MotorController::moveRelativeAngle(int speed, int accel /* 1-300 */, float degrees, float gearRatio, float calibrationFactor) {
	int axis = degreesToAxis(degrees, gearRatio, calibrationFactor);
	return moveRelative(speed, accel, axis);
}

MotorController & MotorController::moveAbsoluteAngle(int speed, int accel /* 1-300 */, float degrees, float gearRatio, float calibrationFactor) {
	int axis = degreesToAxis(degrees, gearRatio, calibrationFactor);
	return moveAbsolute(speed, accel, axis);
}

// Conversion utilities
int MotorController::degreesToAxis(float degrees, float gearRatio, float calibrationFactor) const {

	float motorDegrees = degrees * gearRatio;
	float encoderCounts = (motorDegrees / 360.0f) * ENCODER_COUNTS_PER_REVOLUTION;
	int result = static_cast<int>(encoderCounts * calibrationFactor);

	return result;
}

float MotorController::axisToDegrees(int axis, float gearRatio, float calibrationFactor) const {
	// Reverse the degreesToAxis process for encoder-based axis values

	// 1. Convert raw axis counts back using calibration factor
	float calibratedEncoderCounts = static_cast<float>(axis) / calibrationFactor;

	// 2. Convert encoder counts back to motor degrees (pure encoder-based, no microsteps)
	float motorDegrees = (calibratedEncoderCounts / ENCODER_COUNTS_PER_REVOLUTION) * 360.0f;

	// 3. Convert motor degrees back to hourglass degrees using gear ratio
	// Divide by gearRatio to reverse the multiplication in degreesToAxis
	return motorDegrees / gearRatio;
}

bool MotorController::send(const std::vector<uint8_t> & data) {
	if (data.size() > 7) {
		ofLogError("MotorController") << "Data too large (max 7 bytes): " << data.size();
		return false;
	}

	// Serial disabled - command processed for OSC-only operation
	return true;
}
