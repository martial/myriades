#include "OSCOutController.h"
#include "ofMain.h"

OSCOutController::OSCOutController()
	: enabled(true)
	, sentMessageCount(0) {
}

OSCOutController::~OSCOutController() {
	senders.clear();
}

void OSCOutController::setup() {
	loadConfiguration();
}

void OSCOutController::loadConfigurationFromJson(const ofJson & json) {
	destinations.clear();

	try {
		if (json.contains("enabled")) {
			enabled = json["enabled"];
		}

		if (json.contains("destinations")) {
			loadDestinationsFromJson(json["destinations"]);
		}

	} catch (const std::exception & e) {
		ofLogError("OSCOutController") << "Failed to load config from JSON: " << e.what();
		// Fall back to default config
		addDestination("default", "127.0.0.1", 9000);
	}
}

void OSCOutController::loadConfiguration(const std::string & configPath) {
	ofFile configFile(configPath);

	if (!configFile.exists()) {
		ofLogWarning("OSCOutController") << "Config file not found: " << configPath
										 << " - Creating default configuration";

		// Create default configuration
		destinations.clear();
		addDestination("default", "127.0.0.1", 9000);
		saveConfiguration(configPath);
		return;
	}

	try {
		ofJson json = ofLoadJson(configPath);

		if (json.contains("enabled")) {
			enabled = json["enabled"];
		}

		if (json.contains("destinations")) {
			loadDestinationsFromJson(json["destinations"]);
		}

	} catch (const std::exception & e) {
		ofLogError("OSCOutController") << "Failed to load config: " << e.what();
		// Fall back to default config
		addDestination("default", "127.0.0.1", 9000);
	}
}

void OSCOutController::saveConfiguration(const std::string & configPath) {
	try {
		ofJson json;
		json["enabled"] = enabled;
		json["destinations"] = destinationsToJson();

		ofSaveJson(configPath, json);

	} catch (const std::exception & e) {
		ofLogError("OSCOutController") << "Failed to save config: " << e.what();
	}
}

void OSCOutController::addDestination(const std::string & name, const std::string & ip, int port) {
	// Remove existing destination with same name if it exists
	removeDestination(name);

	OSCDestination dest;
	dest.name = name;
	dest.ip = ip;
	dest.port = port;
	dest.enabled = true;

	destinations.push_back(dest);
	ensureSenderExists(dest);
}

void OSCOutController::removeDestination(const std::string & name) {
	auto it = std::remove_if(destinations.begin(), destinations.end(),
		[&name](const OSCDestination & dest) { return dest.name == name; });

	if (it != destinations.end()) {
		destinations.erase(it, destinations.end());
		senders.erase(name);
	}
}

void OSCOutController::setDestinationEnabled(const std::string & name, bool enabled) {
	for (auto & dest : destinations) {
		if (dest.name == name) {
			dest.enabled = enabled;

			break;
		}
	}
}

std::vector<OSCDestination> OSCOutController::getDestinations() const {
	return destinations;
}

// Motor control messages
void OSCOutController::sendMotorZero(int deviceId) {
	if (!enabled) return;

	ofxOscMessage msg;
	msg.setAddress("/motor/zero");

	// Detailed logging for motor zero commands
	ofLogNotice("OSCOutController") << "🎌 SENDING MOTOR ZERO - Device: " << deviceId
									<< " | Address: " << msg.getAddress();

	// Log destinations
	for (const auto & dest : destinations) {
		if (dest.enabled) {
			ofLogNotice("OSCOutController") << "  → Destination: " << dest.name
											<< " | " << dest.ip << ":" << dest.port << " ✅";
		} else {
			ofLogNotice("OSCOutController") << "  → Destination: " << dest.name
											<< " | " << dest.ip << ":" << dest.port << " ❌ (disabled)";
		}
	}

	sendMessageToAll(msg);
}

void OSCOutController::sendMotorHoming(int deviceId) {
	if (!enabled) return;

	ofxOscMessage msg;
	msg.setAddress("/motor/homing");

	sendMessageToAll(msg);
}

void OSCOutController::sendMotorEmergency(int deviceId) {
	if (!enabled) return;

	ofxOscMessage msg;
	msg.setAddress("/motor/emergency");

	sendMessageToAll(msg);
}

void OSCOutController::sendMotorUstep(int deviceId, int ustepValue) {
	if (!enabled || !validateDeviceId(deviceId)) return;

	// Clamp ustep value to valid range (1-256)
	ustepValue = ofClamp(ustepValue, 1, 256);

	ofxOscMessage msg;
	msg.setAddress("/motor/ustep");
	msg.addInt32Arg(ustepValue);

	sendMessageToAll(msg);
}

void OSCOutController::sendMotorRelative(int deviceId, float speedRotMin, float accDegPerS2, float moveDeg) {
	if (!enabled || !validateDeviceId(deviceId)) return;

	ofxOscMessage msg;
	msg.setAddress("/motor/relative");
	msg.addFloatArg(speedRotMin);
	msg.addFloatArg(accDegPerS2);
	msg.addFloatArg(moveDeg);

	// Detailed logging for motor relative commands
	ofLogNotice("OSCOutController") << "🚀 SENDING MOTOR RELATIVE - Device: " << deviceId
									<< " | Speed: " << speedRotMin << " rot/min"
									<< " | Accel: " << accDegPerS2 << " deg/s²"
									<< " | Move: " << moveDeg << " degrees"
									<< " | Address: " << msg.getAddress();

	// Log destinations
	for (const auto & dest : destinations) {
		if (dest.enabled) {
			ofLogNotice("OSCOutController") << "  → Destination: " << dest.name
											<< " | " << dest.ip << ":" << dest.port << " ✅";
		} else {
			ofLogNotice("OSCOutController") << "  → Destination: " << dest.name
											<< " | " << dest.ip << ":" << dest.port << " ❌ (disabled)";
		}
	}

	sendMessageToAll(msg);
}

void OSCOutController::sendMotorRelativeStop(int deviceId, float accDegPerS2) {
	if (!enabled || !validateDeviceId(deviceId)) return;

	ofxOscMessage msg;
	msg.setAddress("/motor/relative/stop");
	msg.addFloatArg(accDegPerS2);

	sendMessageToAll(msg);
}

void OSCOutController::sendMotorAbsolute(int deviceId, float speedRotMin, float accDegPerS2, float moveDeg) {
	if (!enabled || !validateDeviceId(deviceId)) return;

	ofxOscMessage msg;
	msg.setAddress("/motor/absolute");
	msg.addFloatArg(speedRotMin);
	msg.addFloatArg(accDegPerS2);
	msg.addFloatArg(moveDeg);

	// Detailed logging for motor absolute commands
	ofLogNotice("OSCOutController") << "🎯 SENDING MOTOR ABSOLUTE - Device: " << deviceId
									<< " | Speed: " << speedRotMin << " rot/min"
									<< " | Accel: " << accDegPerS2 << " deg/s²"
									<< " | Position: " << moveDeg << " degrees"
									<< " | Address: " << msg.getAddress();

	// Log destinations
	for (const auto & dest : destinations) {
		if (dest.enabled) {
			ofLogNotice("OSCOutController") << "  → Destination: " << dest.name
											<< " | " << dest.ip << ":" << dest.port << " ✅";
		} else {
			ofLogNotice("OSCOutController") << "  → Destination: " << dest.name
											<< " | " << dest.ip << ":" << dest.port << " ❌ (disabled)";
		}
	}

	sendMessageToAll(msg);
}

void OSCOutController::sendMotorAbsoluteStop(int deviceId, float accDegPerS2) {
	if (!enabled || !validateDeviceId(deviceId)) return;

	ofxOscMessage msg;
	msg.setAddress("/motor/absolute/stop");
	msg.addFloatArg(accDegPerS2);

	sendMessageToAll(msg);
}

// Electromagnet control messages
void OSCOutController::sendMagnet(const std::string & position, int pwmValue) {
	if (!enabled || !validatePWMValue(pwmValue)) return;

	ofxOscMessage msg;
	msg.setAddress("/mag/" + position);
	msg.addInt32Arg(pwmValue);

	sendMessageToAll(msg);
}

// Power LED control messages
void OSCOutController::sendPowerLED(const std::string & position, int pwmValue) {
	if (!enabled || !validatePWMValue(pwmValue)) return;

	ofxOscMessage msg;
	msg.setAddress("/pwr/" + position);
	msg.addInt32Arg(pwmValue);

	sendMessageToAll(msg);
}

// RGB LED circle control messages
void OSCOutController::sendRGBLED(const std::string & position, uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha,
	int originDeg, int arcDeg) {
	if (!enabled || !validateAngle(originDeg) || !validateAngle(arcDeg)) return;

	uint32_t rgba = encodeRGBA(red, green, blue, alpha);

	ofxOscMessage msg;
	msg.setAddress("/rgb/" + position);
	msg.addInt32Arg(static_cast<int32_t>(rgba));
	msg.addInt32Arg(originDeg);
	msg.addInt32Arg(arcDeg);

	sendMessageToAll(msg);
}

void OSCOutController::sendRGBLED(const std::string & position, const ofColor & color, uint8_t alpha, int originDeg, int arcDeg) {
	sendRGBLED(position, color.r, color.g, color.b, alpha, originDeg, arcDeg);
}

// Internal helpers
void OSCOutController::ensureSenderExists(const OSCDestination & dest) {
	if (senders.find(dest.name) == senders.end()) {
		auto sender = std::unique_ptr<ofxOscSender>(new ofxOscSender());
		sender->setup(dest.ip, dest.port);
		senders[dest.name] = std::move(sender);
	}
}

void OSCOutController::sendMessageToAll(const ofxOscMessage & message) {
	if (!enabled) return;

	for (const auto & dest : destinations) {
		if (dest.enabled) {
			sendMessageToDestination(message, dest.name);
		}
	}
	sentMessageCount++;
}

void OSCOutController::sendMessageToDestination(const ofxOscMessage & message, const std::string & destName) {
	auto it = senders.find(destName);
	if (it != senders.end()) {
		it->second->sendMessage(message);
	}
}

std::string OSCOutController::buildMotorAddress(const std::string & command, int deviceId) {
	if (deviceId >= 0) {
		return "/motor/" + ofToString(deviceId) + "/" + command;
	}
	return "/motor/" + command;
}

std::string OSCOutController::buildDeviceAddress(const std::string & prefix, int deviceId) {
	return prefix + "/" + ofToString(deviceId);
}

uint32_t OSCOutController::encodeRGBA(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha) {
	// Encode according to spec: bit 31-24 RED, bit 16-23 GREEN, bit 8-15 BLUE, bit 0-7 ALPHA
	return (static_cast<uint32_t>(red) << 24) | (static_cast<uint32_t>(green) << 16) | (static_cast<uint32_t>(blue) << 8) | static_cast<uint32_t>(alpha);
}

// JSON helpers
void OSCOutController::loadDestinationsFromJson(const ofJson & json) {
	destinations.clear();

	for (const auto & destJson : json) {
		if (destJson.contains("ip") && destJson.contains("port")) {
			OSCDestination dest;
			// Auto-generate name if not provided
			if (destJson.contains("name")) {
				dest.name = destJson["name"];
			} else {
				dest.name = destJson["ip"].get<std::string>() + ":" + std::to_string(destJson["port"].get<int>());
			}
			dest.ip = destJson["ip"];
			dest.port = destJson["port"];
			dest.enabled = destJson.value("enabled", true);

			destinations.push_back(dest);
			ensureSenderExists(dest);
		}
	}
}

ofJson OSCOutController::destinationsToJson() const {
	ofJson json = ofJson::array();

	for (const auto & dest : destinations) {
		ofJson destJson;
		destJson["name"] = dest.name;
		destJson["ip"] = dest.ip;
		destJson["port"] = dest.port;
		destJson["enabled"] = dest.enabled;
		json.push_back(destJson);
	}

	return json;
}

// Validation
bool OSCOutController::validateDeviceId(int deviceId) const {
	if (deviceId < 0) {
		ofLogWarning("OSCOutController") << "Invalid device ID: " << deviceId;
		return false;
	}
	return true;
}

bool OSCOutController::validatePWMValue(int pwmValue) const {
	if (pwmValue < 0 || pwmValue > 255) {
		ofLogWarning("OSCOutController") << "Invalid PWM value (must be 0-255): " << pwmValue;
		return false;
	}
	return true;
}

bool OSCOutController::validateAngle(int angleDeg) const {
	if (angleDeg < 0 || angleDeg > 360) {
		ofLogWarning("OSCOutController") << "Invalid angle (must be 0-360°): " << angleDeg;
		return false;
	}
	return true;
}
