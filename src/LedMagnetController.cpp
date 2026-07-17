#include "LedMagnetController.h"

// Initialize static members
float LedMagnetController::globalLuminosityValue = 1.0f; // Default to full brightness

LedMagnetController::LedMagnetController()
	: lastSentRGB(0, 0, 0)
	, lastSentMainLED(0)
	, lastSentPWM(0)
	, lastSentBlend(0)
	, lastSentOrigin(0)
	, lastSentArc(0)
	, rgbInitialized(false)
	, mainLedInitialized(false)
	, pwmInitialized(false) {
	// Serial port removed - OSC-only mode
}

LedMagnetController::~LedMagnetController() {
	disconnect();
}

LedMagnetController::ConnectionResult LedMagnetController::connect(const std::string & portName, int baudRate) {
	// Serial connection removed - operating in OSC-only mode
	connectedPortName = portName; // Keep for reference only
	ofLogNotice("LedMagnetController") << "Operating in OSC-only mode (no serial)";

	// Reset initialized flags so first commands always send
	rgbInitialized = false;
	mainLedInitialized = false;
	pwmInitialized = false;

	return ConnectionResult::SUCCESS;
}

LedMagnetController::ConnectionResult LedMagnetController::connect(int deviceIndex, int baudRate) {
	// Serial connection removed - operating in OSC-only mode
	ofLogNotice("LedMagnetController") << "Operating in OSC-only mode (device index " << deviceIndex << ")";
	return ConnectionResult::SUCCESS;
}

bool LedMagnetController::isConnected() const {
	// Always return true for OSC operation - serial connection not required
	return true;
}

void LedMagnetController::disconnect() {
	// Serial connection removed - nothing to disconnect
	connectedPortName.clear();
	ofLogNotice("LedMagnetController") << "Disconnected (OSC-only mode)";
}

std::vector<std::string> LedMagnetController::getAvailableDevices() {
	// Serial connection removed - return empty list
	return std::vector<std::string>();
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
	globalLuminosityValue = ofClamp(luminosity, 0.0f, 1.0f);
	ofLogNotice("LedMagnetController") << "Global luminosity set to: " << globalLuminosityValue;
}

float LedMagnetController::getGlobalLuminosity() {
	return globalLuminosityValue;
}

void LedMagnetController::resetLastSentValues() {
	// Only the luminosity-modulated channels need re-sending; PWM is unaffected
	rgbInitialized = false;
	mainLedInitialized = false;
}

LedMagnetController & LedMagnetController::sendLED(uint8_t value, float individualLuminosityFactor) { // Main LED
	uint8_t modulatedValue = static_cast<uint8_t>(ofClamp(static_cast<float>(value) * globalLuminosityValue * individualLuminosityFactor, 0.0f, 255.0f));

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
	uint8_t finalR = static_cast<uint8_t>(ofClamp(static_cast<float>(optR) * globalLuminosityValue * individualLuminosityFactor, 0.0f, 255.0f));
	uint8_t finalG = static_cast<uint8_t>(ofClamp(static_cast<float>(optG) * globalLuminosityValue * individualLuminosityFactor, 0.0f, 255.0f));
	uint8_t finalB = static_cast<uint8_t>(ofClamp(static_cast<float>(optB) * globalLuminosityValue * individualLuminosityFactor, 0.0f, 255.0f));

	// Clamp the new parameters according to JavaScript implementation
	int clampedBlend = ofClamp(blend, 0, 768);
	int clampedOrigin = ofClamp(origin, 0, 360);
	int clampedArc = ofClamp(arc, 0, 360);
	int mode = 1; // Always 1 for now as requested

	ofColor currentColor(finalR, finalG, finalB);

	// Check if values have changed (including new parameters)
	if (rgbInitialized && currentColor == lastSentRGB && clampedBlend == lastSentBlend && clampedOrigin == lastSentOrigin && clampedArc == lastSentArc) {
		return *this;
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

bool LedMagnetController::send(const std::vector<uint8_t> & data) {
	// Serial communication completely removed - state tracking only (OSC-only mode)
	return true;
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
	return gammaLUT[value];
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
	progress = ofClamp(progress, 0.0f, 1.0f);

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

// Unified LED command - each send* call dedups internally against last-sent state
LedMagnetController & LedMagnetController::sendAllLEDParameters(
	uint8_t r, uint8_t g, uint8_t b,
	int blend, int origin, int arc,
	uint8_t mainLedValue, uint8_t pwmValue,
	float individualLuminosityFactor) {

	sendLED(r, g, b, blend, origin, arc, individualLuminosityFactor, true);
	sendLED(mainLedValue, individualLuminosityFactor);
	sendPWM(pwmValue);

	return *this;
}
