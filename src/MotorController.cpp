#include "MotorController.h"
#include "ofMath.h" // For ofMap
#include <algorithm> // For std::min and std::max

// Helper function for clamp
namespace {
template <typename T>
const T & local_clamp(const T & value, const T & low, const T & high) {
	return std::max(low, std::min(value, high));
}
}

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
	motorEnabledStateInitialized = false;
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

	/*
	if (motorEnabledStateInitialized && enabled_command == lastMotorEnabledState) {
		return *this; // No change, don't send
	}
	send({ static_cast<uint8_t>(MotorCommand::ENABLE), enabled_command ? uint8_t(1) : uint8_t(0) });
	lastMotorEnabledState = enabled_command;
	motorEnabledStateInitialized = true;
	*/
	return *this;
}

MotorController & MotorController::setMicrostep(int ustep) {
	ustep = local_clamp(ustep, 1, 256);

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

MotorController & MotorController::moveRelative(int speed, int accel /* 0-255 */, int axis) {
	speed = local_clamp(speed, 0, 500);
	// Accel is now 0-255, clamp it and cast to uint8_t for hardware
	uint8_t hardwareAccel = static_cast<uint8_t>(local_clamp(accel, 0, 255));
	axis = local_clamp(axis, -8388607, 8388607);

	std::vector<uint8_t> data = { static_cast<uint8_t>(MotorCommand::MOVE_RELATIVE) };

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

MotorController & MotorController::moveAbsolute(int speed, int accel /* 0-255 */, int axis) {
	speed = local_clamp(speed, 0, 500);
	// Accel is now 0-255, clamp it and cast to uint8_t for hardware
	uint8_t hardwareAccel = static_cast<uint8_t>(local_clamp(accel, 0, 255));
	axis = local_clamp(axis, -8388607, 8388607);

	std::vector<uint8_t> data = { static_cast<uint8_t>(MotorCommand::MOVE_ABSOLUTE) };

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

MotorController & MotorController::stopRelative(int accel /* 1-300 */) {
	return moveRelative(0, accel, 0); // accel will be scaled by moveRelative
}

MotorController & MotorController::stopAbsolute(int accel /* 1-300 */) {
	return moveAbsolute(0, accel, 0); // accel will be scaled by moveAbsolute
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

	// Log what we're sending including motor device ID
	std::string dataStr = "";
	for (size_t i = 0; i < data.size(); i++) {
		if (i > 0) dataStr += " ";
		dataStr += std::to_string(static_cast<int>(data[i]));
	}
	//ofLogNotice("MotorController") << "Motor ID " << id << " command processed for OSC-only: [" << dataStr << "]";

	// Serial disabled - command processed for OSC-only operation

	// Debug logging removed - no packet to log in OSC-only mode

	return true;
}

std::vector<uint8_t> MotorController::buildPacket(const std::vector<uint8_t> & data) const {
	std::vector<uint8_t> packet;
	uint8_t checksum = 0;

	// Start byte
	packet.push_back(START_BYTE);

	// Build ID array first (like JavaScript idar)
	int numIdBytes = (id < 2048 ? 2 : 4);
	std::vector<uint8_t> idBytes;

	for (int i = 0; i < numIdBytes; i++) {
		uint8_t id_byte = (id >> (8 * i)) & 255;
		idBytes.insert(idBytes.begin(), id_byte);
		checksum = (checksum + id_byte) % 256;
	}

	// Add data bytes to checksum (like JavaScript data array)
	for (uint8_t dataByte : data) {
		checksum = (checksum + dataByte) % 256;
	}

	// Add servo CAN frame CRC (first checksum)
	uint8_t servoCrc = checksum;

	// Calculate flags: ext << 7 | rtr << 6 | data.length + 1 (for CRC)
	uint8_t flags = (ext ? 0x80 : 0) | (rtr ? 0x40 : 0) | ((data.size() + 1) & 0x3F);
	packet.push_back(flags);

	// Update checksum with flags (like JavaScript)
	checksum = (checksum + flags) % 256;

	// Add ID bytes to packet
	packet.insert(packet.end(), idBytes.begin(), idBytes.end());

	// Add data bytes to packet
	for (uint8_t dataByte : data) {
		packet.push_back(dataByte);
	}

	// Add servo CAN frame CRC
	packet.push_back(servoCrc);

	// Add final checksum and end byte
	packet.push_back(checksum);
	packet.push_back(END_BYTE);

	return packet;
}