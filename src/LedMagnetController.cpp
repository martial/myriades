#include "LedMagnetController.h"
#include "OSCHelper.h"

// Initialize static members
float LedMagnetController::globalLuminosityValue = 1.0f; // Default to full brightness

LedMagnetController::LedMagnetController(std::shared_ptr<ISerialPort> serialPort)
	: lastSendTime(0.0f)
	, totalTime(0.0f)
	, sampleCount(0)
	, serialPort(serialPort)
	, lastSentRGB(0, 0, 0) // Initialize lastSentRGB
	, lastSentMainLED(0)
	, lastSentPWM(0)
	, lastSentBlend(0)
	, lastSentOrigin(0)
	, lastSentArc(0)
	, rgbInitialized(false)
	, mainLedInitialized(false)
	, pwmInitialized(false) {
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
			float averageTime = totalTime / sampleCount;
			// ofLogNotice("LedMagnetController") << "Average send interval: " << averageTime * 1000.0f << " ms (" << sampleCount << " samples)";
		}
	}

	lastSendTime = currentTime;

	std::vector<uint8_t> packet = buildPacket(data);

	// Log the complete frame being sent
	// std::string frameLog = "frame: ";
	// for (size_t i = 0; i < packet.size(); i++) {
	// 	if (i > 0) frameLog += " ";
	// 	frameLog += std::to_string(static_cast<int>(packet[i]));
	// }
	// ofLogNotice("SerialFrame") << frameLog;

	serialPort->writeBytes(packet.data(), packet.size());
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