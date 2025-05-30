#include "LedMagnetController.h"
#include "OSCHelper.h"

// Initialize static members
float LedMagnetController::globalLuminosityValue = 1.0f; // Default to full brightness

LedMagnetController::LedMagnetController(std::shared_ptr<ISerialPort> serialPort)
	: lastSendTime(0.0f)
	, totalTime(0.0f)
	, sampleCount(0)
	, lastSentRGB(0, 0, 0) // Initialize lastSentRGB
	, lastSentMainLED(0)
	, lastSentPWM(0)
	, lastSentBlend(0)
	, lastSentOrigin(0)
	, lastSentArc(0)
	, rgbInitialized(false)
	, mainLedInitialized(false)
	, pwmInitialized(false)
	, serialPort(serialPort) {
}

LedMagnetController::~LedMagnetController() {
	disconnect();
}

LedMagnetController::ConnectionResult LedMagnetController::connect(const std::string & portName, int baudRate) {
	auto & portManager = SerialPortManager::getInstance();
	serialPort = portManager.getPort(portName, baudRate);

	if (!serialPort) {
		return ConnectionResult::DEVICE_NOT_FOUND;
	}

	connectedPortName = portName;
	ofLogNotice("CanProtocol") << "Connected to CAN device: " << portName;

	// Reset initialized flags on new connection so first commands always send
	rgbInitialized = false;
	mainLedInitialized = false;
	pwmInitialized = false;

	send({ 0x00 });
	ofLogNotice("CanProtocol") << "CAN protocol initialized";

	return ConnectionResult::SUCCESS;
}

LedMagnetController::ConnectionResult LedMagnetController::connect(int deviceIndex, int baudRate) {
	auto devices = getAvailableDevices();

	if (deviceIndex < 0 || deviceIndex >= devices.size()) {
		return ConnectionResult::INVALID_INDEX;
	}

	return connect(devices[deviceIndex], baudRate);
}

bool LedMagnetController::isConnected() const {
	return serialPort && serialPort->isInitialized();
}

void LedMagnetController::disconnect() {
	if (serialPort) {
		serialPort->close();
		serialPort.reset();
		connectedPortName.clear();
		ofLogNotice("CanProtocol") << "Disconnected from CAN device";
	}
}

std::vector<std::string> LedMagnetController::getAvailableDevices() {
	auto & portManager = SerialPortManager::getInstance();
	return portManager.getAvailablePorts();
}

std::string LedMagnetController::getConnectedDeviceName() const {
	return connectedPortName;
}

LedMagnetController & LedMagnetController::setId(int _id) {
	id = _id;
	ext = (id >= 2048);
	return *this;
}

LedMagnetController & LedMagnetController::setExtended(bool extended) {
	ext = extended;
	return *this;
}

LedMagnetController & LedMagnetController::setRemote(bool remote) {
	rtr = remote;
	return *this;
}

// Global Luminosity Static Methods
void LedMagnetController::setGlobalLuminosity(float luminosity) {
	globalLuminosityValue = OSCHelper::clamp(luminosity, 0.0f, 1.0f);
	ofLogNotice("LedMagnetController") << "Global luminosity set to: " << globalLuminosityValue;
	// Potentially, we might want to mark all 'initialized' flags to false here
	// to force re-sending of all values with the new luminosity, or handle it
	// in the OSCController by re-triggering the last known GUI values.
	// For now, new commands will pick up the new luminosity.
}

float LedMagnetController::getGlobalLuminosity() {
	return globalLuminosityValue;
}

LedMagnetController & LedMagnetController::sendLED(uint8_t value, float individualLuminosityFactor) { // Main LED
	uint8_t modulatedValue = static_cast<uint8_t>(OSCHelper::clamp(static_cast<float>(value) * globalLuminosityValue * individualLuminosityFactor, 0.0f, 255.0f));

	if (mainLedInitialized && modulatedValue == lastSentMainLED) {
		return *this;
	}
	send({ 1, modulatedValue });
	lastSentMainLED = modulatedValue;
	mainLedInitialized = true;
	return *this;
}

LedMagnetController & LedMagnetController::sendLED(uint8_t r, uint8_t g, uint8_t b, int blend, int origin, int arc, float individualLuminosityFactor, bool enabled) { // RGB LED
	uint8_t optR = optimizeRGB(r);
	uint8_t optG = optimizeRGB(g);
	uint8_t optB = optimizeRGB(b);

	// Apply global AND individual luminosity
	uint8_t finalR = static_cast<uint8_t>(OSCHelper::clamp(static_cast<float>(optR) * globalLuminosityValue * individualLuminosityFactor, 0.0f, 255.0f));
	uint8_t finalG = static_cast<uint8_t>(OSCHelper::clamp(static_cast<float>(optG) * globalLuminosityValue * individualLuminosityFactor, 0.0f, 255.0f));
	uint8_t finalB = static_cast<uint8_t>(OSCHelper::clamp(static_cast<float>(optB) * globalLuminosityValue * individualLuminosityFactor, 0.0f, 255.0f));

	// Clamp the new parameters according to JavaScript implementation
	int clampedBlend = OSCHelper::clamp(blend, 0, 768);
	int clampedOrigin = OSCHelper::clamp(origin, 0, 360);
	int clampedArc = OSCHelper::clamp(arc, 0, 360);
	int mode = 1; // Always 1 for now as requested

	ofColor currentColor(finalR, finalG, finalB);

	// Check if values have changed (including new parameters)
	if (rgbInitialized && currentColor == lastSentRGB && clampedBlend == lastSentBlend && clampedOrigin == lastSentOrigin && clampedArc == lastSentArc) {
		return *this;
	}

	if (r != finalR || g != finalG || b != finalB) {
		// ofLogNotice("RGB Transform") << "Original RGB(" << (int)r << "," << (int)g << "," << (int)b
		// 							 << ") -> Optimized (" << (int)optR << "," << (int)optG << "," << (int)optB
		// 							 << ") -> Final (" << (int)finalR << "," << (int)finalG << "," << (int)finalB
		// 							 << ") GlobalLum: " << globalLuminosityValue << " IndivLum: " << individualLuminosityFactor;
	}

	// Create bitsMap according to JavaScript implementation:
	// 10bits blend, 9bits origin, 9 bits arc, 4 bits mode
	uint32_t bitsMap = ((clampedBlend & 0x3FF) << 22) | ((clampedOrigin & 0x1FF) << 13) | ((clampedArc & 0x1FF) << 4) | (mode & 0xF);

	// Extract 4 bytes from bitsMap
	uint8_t byte1 = (bitsMap >> 24) & 0xFF;
	uint8_t byte2 = (bitsMap >> 16) & 0xFF;
	uint8_t byte3 = (bitsMap >> 8) & 0xFF;
	uint8_t byte4 = bitsMap & 0xFF;

	// Send 8-byte command: command(3), r, g, b, byte1, byte2, byte3, byte4

	send({ 3, finalR, finalG, finalB, byte1, byte2, byte3, byte4 });

	// Store last sent values
	lastSentRGB = currentColor;
	lastSentBlend = clampedBlend;
	lastSentOrigin = clampedOrigin;
	lastSentArc = clampedArc;
	rgbInitialized = true;
	return *this;
}

LedMagnetController & LedMagnetController::sendPWM(uint8_t value) { // PWM not affected by global luminosity
	if (pwmInitialized && value == lastSentPWM) {
		return *this;
	}
	send({ 2, value });
	lastSentPWM = value;
	pwmInitialized = true;
	return *this;
}

LedMagnetController & LedMagnetController::sendDotStar(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness) {
	send(ControlType::DOTSTAR, { r, g, b, brightness });
	return *this;
}

LedMagnetController & LedMagnetController::sendDotStar(const ofColor & color, uint8_t brightness) {
	return sendDotStar(color.r, color.g, color.b, brightness);
}

bool LedMagnetController::send(ControlType type, const std::vector<uint8_t> & data) {
	std::vector<uint8_t> packet = { static_cast<uint8_t>(type) };
	packet.insert(packet.end(), data.begin(), data.end());
	return send(packet);
}

bool LedMagnetController::send(const std::vector<uint8_t> & data) {
	if (!isConnected()) {
		ofLogError("CanProtocol") << "Cannot send command: Not connected to device";
		return false;
	}

	if (data.size() > 8) {
		ofLogError("CanProtocol") << "Data too large (max 8 bytes): " << data.size();
		return false;
	}

	float currentTime = ofGetElapsedTimef();
	float timeSinceLastSend = currentTime - lastSendTime;

	if (lastSendTime > 0.0f) {
		timingHistory.push_back(timeSinceLastSend);
		totalTime += timeSinceLastSend;
		sampleCount++;

		if (timingHistory.size() > MAX_TIMING_SAMPLES) {
			float oldestSample = timingHistory.front();
			timingHistory.erase(timingHistory.begin());
			totalTime -= oldestSample;
			sampleCount--;
		}
		if (sampleCount > 0) { // Prevent division by zero if all samples were removed
			// Removed unused averageTime calculation
		}
	}

	lastSendTime = currentTime;

	std::vector<uint8_t> packet = buildPacket(data);

	// Log what we're sending including device ID
	std::string dataStr = "";
	for (size_t i = 0; i < data.size(); i++) {
		if (i > 0) dataStr += " ";
		dataStr += std::to_string(static_cast<int>(data[i]));
	}
	ofLogNotice("LedMagnetController") << "Sending to ID " << id << ": [" << dataStr << "]";

	serialPort->writeBytes(packet.data(), packet.size());

	// Notify SerialPortManager for statistics
	SerialPortManager::getInstance().trackWrite(packet.size()); // Use existing trackWrite method

	return true;
}

std::vector<uint8_t> LedMagnetController::buildPacket(const std::vector<uint8_t> & data) const {
	std::vector<uint8_t> packet;
	uint8_t checksum = 0;

	std::vector<uint8_t> idBytes;
	int numIdBytes = (id < 2048 ? 2 : 4);
	for (int i = 0; i < numIdBytes; i++) {
		uint8_t id_byte = (id >> (8 * i)) & 255;
		idBytes.insert(idBytes.begin(), id_byte);
		checksum = (checksum + id_byte) % 256;
	}

	for (uint8_t dataByte : data) {
		checksum = (checksum + dataByte) % 256;
	}

	uint8_t flags = (ext ? 0x80 : 0) | (rtr ? 0x40 : 0) | (data.size() & 0x3F);
	checksum = (checksum + flags) % 256;

	packet.push_back(START_BYTE);
	packet.push_back(flags);

	for (uint8_t idByte : idBytes) {
		packet.push_back(idByte);
	}

	for (uint8_t dataByte : data) {
		packet.push_back(dataByte);
	}

	packet.push_back(checksum);
	packet.push_back(END_BYTE);

	return packet;
}

// RGB optimization static variables
uint8_t LedMagnetController::gammaLUT[256];
float LedMagnetController::currentGamma = 2.2f;
uint8_t LedMagnetController::minThreshold = 3;
bool LedMagnetController::lutInitialized = false;

void LedMagnetController::initializeLUT() {
	for (int i = 0; i < 256; i++) {
		if (i == 0) {
			gammaLUT[i] = 0;
		} else {
			float normalized = i / 255.0f;
			float corrected = pow(normalized, 1.0f / currentGamma);
			uint8_t gammaValue = (uint8_t)(corrected * 255.0f);

			if (gammaValue > 0 && gammaValue < minThreshold) {
				gammaValue = minThreshold;
			}
			gammaLUT[i] = gammaValue;
		}
	}
	lutInitialized = true;
}

uint8_t LedMagnetController::optimizeRGB(uint8_t value) {
	if (!lutInitialized) {
		initializeLUT();
	}
	uint8_t optimized = gammaLUT[value];

	if (value != optimized && value > 0) {
		// ofLogNotice("RGB Optimization") << "Input: " << (int)value << " -> Optimized: " << (int)optimized;
	}

	return optimized;
}

void LedMagnetController::setGammaCorrection(float gamma) {
	currentGamma = gamma;
	lutInitialized = false;
}

void LedMagnetController::setMinimumThreshold(uint8_t threshold) {
	minThreshold = threshold;
	lutInitialized = false;
}

// LED Circle System Helper Functions
int LedMagnetController::getCircleBlendValue(int circleNumber) {
	switch (circleNumber) {
	case 1:
		return CIRCLE_1_BLEND; // 0 - Inner circle
	case 2:
		return CIRCLE_2_BLEND; // 384 - Middle circle
	case 3:
		return CIRCLE_3_BLEND; // 768 - Outer circle
	default:
		return CIRCLE_1_BLEND; // Default to circle 1
	}
}

int LedMagnetController::calculateBlendTransition(int fromCircle, int toCircle, float progress) {
	if (fromCircle < 1 || fromCircle > 3 || toCircle < 1 || toCircle > 3) {
		return CIRCLE_1_BLEND; // Default to circle 1 on invalid input
	}

	// Clamp progress to 0.0-1.0
	progress = OSCHelper::clamp(progress, 0.0f, 1.0f);

	int fromBlend = getCircleBlendValue(fromCircle);
	int toBlend = getCircleBlendValue(toCircle);

	// Linear interpolation between circles
	return static_cast<int>(fromBlend + (toBlend - fromBlend) * progress);
}

bool LedMagnetController::isArcActive(int currentAngle, int origin, int arcEnd) {
	// Normalize all angles to 0-360 range
	currentAngle = currentAngle % 360;
	if (currentAngle < 0) currentAngle += 360;

	origin = origin % 360;
	if (origin < 0) origin += 360;

	arcEnd = arcEnd % 360;
	if (arcEnd < 0) arcEnd += 360;

	// Special case: if origin equals arcEnd, consider full circle active
	if (origin == arcEnd) {
		return true;
	}

	// Check if current angle is within the arc
	if (origin <= arcEnd) {
		// Arc doesn't cross 0 degrees
		return currentAngle >= origin && currentAngle <= arcEnd;
	} else {
		// Arc crosses 0 degrees (e.g., from 350° to 10°)
		return currentAngle >= origin || currentAngle <= arcEnd;
	}
}

// Unified LED command - sends all parameters in one consistent format
LedMagnetController & LedMagnetController::sendAllLEDParameters(
	uint8_t r, uint8_t g, uint8_t b,
	int blend, int origin, int arc,
	uint8_t mainLedValue, uint8_t pwmValue,
	float individualLuminosityFactor) {

	// Pre-check each parameter type to avoid unnecessary calls

	// 1. Check if RGB/arc/blend/origin parameters changed
	uint8_t optR = optimizeRGB(r);
	uint8_t optG = optimizeRGB(g);
	uint8_t optB = optimizeRGB(b);
	uint8_t finalR = static_cast<uint8_t>(OSCHelper::clamp(static_cast<float>(optR) * globalLuminosityValue * individualLuminosityFactor, 0.0f, 255.0f));
	uint8_t finalG = static_cast<uint8_t>(OSCHelper::clamp(static_cast<float>(optG) * globalLuminosityValue * individualLuminosityFactor, 0.0f, 255.0f));
	uint8_t finalB = static_cast<uint8_t>(OSCHelper::clamp(static_cast<float>(optB) * globalLuminosityValue * individualLuminosityFactor, 0.0f, 255.0f));
	int clampedBlend = OSCHelper::clamp(blend, 0, 768);
	int clampedOrigin = OSCHelper::clamp(origin, 0, 360);
	int clampedArc = OSCHelper::clamp(arc, 0, 360);
	ofColor currentColor(finalR, finalG, finalB);

	bool rgbNeedsUpdate = !rgbInitialized || !(currentColor == lastSentRGB && clampedBlend == lastSentBlend && clampedOrigin == lastSentOrigin && clampedArc == lastSentArc);

	// 2. Check if Main LED value changed
	uint8_t finalMainLed = static_cast<uint8_t>(OSCHelper::clamp(static_cast<float>(mainLedValue) * globalLuminosityValue * individualLuminosityFactor, 0.0f, 255.0f));
	bool mainLedNeedsUpdate = !mainLedInitialized || (finalMainLed != lastSentMainLED);

	// 3. Check if PWM value changed
	bool pwmNeedsUpdate = !pwmInitialized || (pwmValue != lastSentPWM);

	// ONLY send commands for parameters that actually changed
	if (rgbNeedsUpdate) {
		ofLogNotice("LedMagnetController") << "ID " << id << " - Sending RGB command (changed)";
		sendLED(r, g, b, blend, origin, arc, individualLuminosityFactor, true);
	} else {
		ofLogVerbose("LedMagnetController") << "ID " << id << " - Skipping RGB command (same values)";
	}

	if (mainLedNeedsUpdate) {
		ofLogNotice("LedMagnetController") << "ID " << id << " - Sending Main LED command (changed)";
		sendLED(mainLedValue, individualLuminosityFactor);
	} else {
		ofLogVerbose("LedMagnetController") << "ID " << id << " - Skipping Main LED command (same value)";
	}

	if (pwmNeedsUpdate) {
		ofLogNotice("LedMagnetController") << "ID " << id << " - Sending PWM command (changed)";
		sendPWM(pwmValue);
	} else {
		ofLogVerbose("LedMagnetController") << "ID " << id << " - Skipping PWM command (same value)";
	}

	return *this;
}