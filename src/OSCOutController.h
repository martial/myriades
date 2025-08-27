#pragma once

#include "ofMain.h"
#include "ofxOsc.h"
#include <map>
#include <memory>
#include <string>
#include <vector>

// OSC Destination configuration
struct OSCDestination {
	std::string name;
	std::string ip;
	int port;
	bool enabled = true;
};

class OSCOutController {
public:
	OSCOutController();
	~OSCOutController();

	// Setup and configuration
	void setup();
	void loadConfiguration(const std::string & configPath = "osc_out_config.json");
	void loadConfigurationFromJson(const ofJson & json);
	void saveConfiguration(const std::string & configPath = "osc_out_config.json");

	// Destination management
	void addDestination(const std::string & name, const std::string & ip, int port);
	void removeDestination(const std::string & name);
	void setDestinationEnabled(const std::string & name, bool enabled);
	std::vector<OSCDestination> getDestinations() const;

	// Motor control messages
	void sendMotorZero(int deviceId = -1);
	void sendMotorHoming(int deviceId = -1);
	void sendMotorEmergency(int deviceId = -1);
	void sendMotorUstep(int deviceId, int ustepValue);
	void sendMotorRelative(int deviceId, float speedRotMin, float accDegPerS2, float moveDeg);
	void sendMotorRelativeStop(int deviceId, float accDegPerS2);
	void sendMotorAbsolute(int deviceId, float speedRotMin, float accDegPerS2, float moveDeg);
	void sendMotorAbsoluteStop(int deviceId, float accDegPerS2);

	// Electromagnet control messages
	void sendMagnet(const std::string & position, int pwmValue); // 0-255, position: "top" or "bot"

	// Power LED control messages
	void sendPowerLED(const std::string & position, int pwmValue); // 0-255, position: "top" or "bot"

	// RGB LED circle control messages
	void sendRGBLED(const std::string & position, uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha,
		int originDeg, int arcDeg); // position: "top" or "bot"
	void sendRGBLED(const std::string & position, const ofColor & color, uint8_t alpha, int originDeg, int arcDeg);

	// Utility functions
	bool isEnabled() const { return enabled; }
	void setEnabled(bool enable) { enabled = enable; }

	// Get statistics
	int getSentMessageCount() const { return sentMessageCount; }
	void resetStats() { sentMessageCount = 0; }

private:
	bool enabled;
	std::vector<OSCDestination> destinations;
	std::map<std::string, std::unique_ptr<ofxOscSender>> senders;
	int sentMessageCount;

	// Internal helpers
	void ensureSenderExists(const OSCDestination & dest);
	void sendMessageToAll(const ofxOscMessage & message);
	void sendMessageToDestination(const ofxOscMessage & message, const std::string & destName);

	// Message creation helpers
	std::string buildMotorAddress(const std::string & command, int deviceId = -1);
	std::string buildDeviceAddress(const std::string & prefix, int deviceId);
	uint32_t encodeRGBA(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha);

	// JSON helpers
	void loadDestinationsFromJson(const ofJson & json);
	ofJson destinationsToJson() const;

	// Validation
	bool validateDeviceId(int deviceId) const;
	bool validatePWMValue(int pwmValue) const;
	bool validateAngle(int angleDeg) const;
};
