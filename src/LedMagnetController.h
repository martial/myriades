#pragma once

#include "SerialPortManager.h"
#include "ofMain.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

class LedMagnetController {
public:
	// Control channel types
	enum class ControlType : uint8_t {
		LED = 1,
		PWM = 2,
		DOTSTAR = 3
	};

	// Connection result enum for better error handling
	enum class ConnectionResult {
		SUCCESS,
		DEVICE_NOT_FOUND,
		CONNECTION_FAILED,
		INVALID_INDEX,
		PORT_IN_USE
	};

	// Constructor with dependency injection
	explicit LedMagnetController(std::shared_ptr<ISerialPort> serialPort = nullptr);
	~LedMagnetController();

	// Setup and connection
	ConnectionResult connect(const std::string & portName, int baudRate = 230400);
	ConnectionResult connect(int deviceIndex, int baudRate = 230400);
	bool isConnected() const;
	void disconnect();

	// Device management
	std::vector<std::string> getAvailableDevices();
	std::string getConnectedDeviceName() const;

	// Fluent interface for configuration
	LedMagnetController & setId(int _id);
	LedMagnetController & setExtended(bool extended);
	LedMagnetController & setRemote(bool remote);

	// Control functions with fluent interface
	LedMagnetController & sendLED(uint8_t value, float individualLuminosityFactor = 1.0f);
	LedMagnetController & sendLED(uint8_t r, uint8_t g, uint8_t b, int blend = 0, int origin = 0, int arc = 360, float individualLuminosityFactor = 1.0f, bool enabled = true);
	LedMagnetController & sendPWM(uint8_t value);
	LedMagnetController & sendDotStar(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness = 255);
	LedMagnetController & sendDotStar(const ofColor & color, uint8_t brightness = 255);

	// RGB optimization for better low-value performance
	static uint8_t optimizeRGB(uint8_t value);
	static void setGammaCorrection(float gamma);
	static void setMinimumThreshold(uint8_t threshold);

	// Global Luminosity Control (static)
	static void setGlobalLuminosity(float luminosity);
	static float getGlobalLuminosity();

	// Generic send method
	bool send(ControlType type, const std::vector<uint8_t> & data);
	bool send(const std::vector<uint8_t> & data);

	// Current protocol state
	int getCurrentId() const { return id; }
	bool isExtended() const { return ext; }
	bool isRemote() const { return rtr; }

	// Getters for last sent values (for visualization)
	ofColor getLastSentRGB() const { return lastSentRGB; }
	uint8_t getLastSentMainLED() const { return lastSentMainLED; }
	uint8_t getLastSentPWM() const { return lastSentPWM; }
	int getLastSentBlend() const { return lastSentBlend; }
	int getLastSentOrigin() const { return lastSentOrigin; }
	int getLastSentArc() const { return lastSentArc; }

	bool isRgbInitialized() const { return rgbInitialized; }
	bool isMainLedInitialized() const { return mainLedInitialized; }
	bool isPwmInitialized() const { return pwmInitialized; }

	// For rate-limiting/logging send frequency
	float lastSendTime;

	// Timing statistics for average calculation
	std::vector<float> timingHistory;
	static constexpr size_t MAX_TIMING_SAMPLES = 100; // Keep last 100 samples
	float totalTime;
	size_t sampleCount;

	// Last sent values to prevent redundant serial commands
	ofColor lastSentRGB;
	uint8_t lastSentMainLED;
	uint8_t lastSentPWM;
	int lastSentBlend;
	int lastSentOrigin;
	int lastSentArc;
	bool rgbInitialized = false;
	bool mainLedInitialized = false;
	bool pwmInitialized = false;

	// RGB optimization static variables
	static uint8_t gammaLUT[256];
	static float currentGamma;
	static uint8_t minThreshold;
	static bool lutInitialized;

	// LED Circle System Constants
	static const int CIRCLE_1_BLEND = 0; // Inner circle (32 LEDs)
	static const int CIRCLE_2_BLEND = 384; // Middle circle (36 LEDs) - scaled to 768 range
	static const int CIRCLE_3_BLEND = 768; // Outer circle (42 LEDs)

	// LED Circle Helper Functions
	static int getCircleBlendValue(int circleNumber); // 1, 2, or 3 -> blend value
	static int calculateBlendTransition(int fromCircle, int toCircle, float progress); // 0.0-1.0
	static bool isArcActive(int currentAngle, int origin, int arcEnd);

	// RGB optimization methods
	static void initializeLUT();

private:
	std::shared_ptr<ISerialPort> serialPort;
	std::string connectedPortName;

	// Protocol parameters
	int id = 11;
	bool ext = false;
	bool rtr = false;
	static constexpr uint8_t START_BYTE = 0xE7; // 231
	static constexpr uint8_t END_BYTE = 0x7E; // 126

	// Build protocol packet
	std::vector<uint8_t> buildPacket(const std::vector<uint8_t> & data) const;

	// Helper to clamp values
	template <typename T>
	static T clamp(T value, T min, T max) {
		return std::max(min, std::min(value, max));
	}

	// Global luminosity static data
	static float globalLuminosityValue;
};