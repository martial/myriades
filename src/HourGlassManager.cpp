#include "HourGlassManager.h"

HourGlassManager::HourGlassManager()
	: configFilePath("hourglasses.json")
	, sharedSerialPort("tty.usbmodem1101")
	, sharedBaudRate() {
}

HourGlassManager::~HourGlassManager() {
	disconnectAll();
}

bool HourGlassManager::loadConfiguration(const std::string & configFile) {
	configFilePath = configFile;

	ofFile file(configFile);
	if (!file.exists()) {
		ofLogWarning("HourGlassManager") << "Config file not found: " << configFile << " - Creating default";
		createDefaultConfiguration();
		return saveConfiguration(configFile);
	}

	try {
		ofJson json = ofLoadJson(configFile);

		if (!json.contains("hourglasses")) {
			ofLogError("HourGlassManager") << "Invalid config file: missing 'hourglasses' array";
			return false;
		}

		// Load shared serial port configuration
		if (json.contains("serialPort")) {
			sharedSerialPort = json["serialPort"];
		}
		if (json.contains("baudRate")) {
			sharedBaudRate = json["baudRate"];
		}

		// Clear existing hourglasses
		disconnectAll();
		hourglasses.clear();

		// Load each hourglass

		for (const auto & hourglassJson : json["hourglasses"]) {
			if (!parseHourGlassJson(hourglassJson)) {
				ofLogError("HourGlassManager") << "Failed to parse hourglass configuration";
				return false;
			}
		}

		return true;

	} catch (const std::exception & e) {
		ofLogError("HourGlassManager") << "❌ Error loading config: " << e.what();
		ofLogWarning("HourGlassManager") << "🔄 Falling back to default configuration";
		createDefaultConfiguration();
		return false;
	}
}

bool HourGlassManager::saveConfiguration(const std::string & configFile) {

	try {
		ofJson json;
		json["serialPort"] = sharedSerialPort;
		json["baudRate"] = sharedBaudRate;
		json["hourglasses"] = ofJson::array();

		for (const auto & hourglass : hourglasses) {
			json["hourglasses"].push_back(createHourGlassJson(*hourglass));
		}

		ofSaveJson(configFile, json);

		return true;

	} catch (const std::exception & e) {
		ofLogError("HourGlassManager") << "❌ Error saving config: " << e.what();
		return false;
	}
}

void HourGlassManager::createDefaultConfiguration() {
	// Clear existing
	disconnectAll();
	hourglasses.clear();

	// Add default hourglasses
	addHourGlass("HourGlass1", 11, 12, 1);
	addHourGlass("HourGlass2", 21, 22, 2);
}

void HourGlassManager::addHourGlass(const std::string & name, int upLedId, int downLedId, int motorId) {
	auto hourglass = std::unique_ptr<HourGlass>(new HourGlass(name));
	hourglass->configure(sharedSerialPort, sharedBaudRate, upLedId, downLedId, motorId);
	hourglasses.push_back(std::move(hourglass));
}

bool HourGlassManager::removeHourGlass(const std::string & name) {
	auto it = std::find_if(hourglasses.begin(), hourglasses.end(),
		[&name](const std::unique_ptr<HourGlass> & hg) {
			return hg->getName() == name;
		});

	if (it != hourglasses.end()) {
		(*it)->disconnect();
		hourglasses.erase(it);

		return true;
	}

	ofLogWarning("HourGlassManager") << "Hourglass not found: " << name;
	return false;
}

HourGlass * HourGlassManager::getHourGlass(const std::string & name) {
	auto it = std::find_if(hourglasses.begin(), hourglasses.end(),
		[&name](const std::unique_ptr<HourGlass> & hg) {
			return hg->getName() == name;
		});

	return (it != hourglasses.end()) ? it->get() : nullptr;
}

HourGlass * HourGlassManager::getHourGlass(size_t index) {
	return (index < hourglasses.size()) ? hourglasses[index].get() : nullptr;
}

bool HourGlassManager::connectAll() {
	bool allConnected = true;
	for (auto & hourglass : hourglasses) {
		if (!hourglass->connect()) {
			allConnected = false;
		}
	}

	return allConnected;
}

bool HourGlassManager::connectHourGlass(const std::string & name) {
	auto * hourglass = getHourGlass(name);
	if (hourglass) {
		return hourglass->connect();
	}

	ofLogWarning("HourGlassManager") << "Cannot connect - hourglass not found: " << name;
	return false;
}

void HourGlassManager::disconnectAll() {
	for (auto & hourglass : hourglasses) {
		hourglass->disconnect();
	}
}

void HourGlassManager::disconnectHourGlass(const std::string & name) {
	auto * hourglass = getHourGlass(name);
	if (hourglass) {
		hourglass->disconnect();

	} else {
		ofLogWarning("HourGlassManager") << "Cannot disconnect - hourglass not found: " << name;
	}
}

// Bulk operations
void HourGlassManager::enableAllMotors() {
	for (auto & hourglass : hourglasses) {
		hourglass->enableMotor();
	}
}

void HourGlassManager::disableAllMotors() {
	for (auto & hourglass : hourglasses) {
		hourglass->disableMotor();
	}
}

void HourGlassManager::emergencyStopAll() {
	for (auto & hourglass : hourglasses) {
		hourglass->emergencyStop();
	}
}

void HourGlassManager::setAllLEDs(uint8_t r, uint8_t g, uint8_t b) {
	for (auto & hourglass : hourglasses) {
		hourglass->setAllLEDs(r, g, b);
	}
}

std::vector<std::string> HourGlassManager::getHourGlassNames() const {
	std::vector<std::string> names;
	for (const auto & hourglass : hourglasses) {
		names.push_back(hourglass->getName());
	}
	return names;
}

std::vector<std::string> HourGlassManager::getAvailableSerialPorts() const {
	// Serial disabled - return empty list
	return std::vector<std::string>();
}

// JSON helpers
ofJson HourGlassManager::createHourGlassJson(const HourGlass & hourglass) const {
	ofJson json;
	json["name"] = hourglass.getName();
	json["upLedId"] = hourglass.getUpLedId();
	json["downLedId"] = hourglass.getDownLedId();
	json["motorId"] = hourglass.getMotorId();

	// Add OSC configuration if OSC Out is configured
	if (hourglass.isOSCOutEnabled()) {
		auto oscOut = hourglass.getOSCOut();
		if (oscOut) {
			ofJson oscConfig;
			oscConfig["enabled"] = oscOut->isEnabled();

			// Save destinations
			auto destinations = oscOut->getDestinations();
			if (!destinations.empty()) {
				oscConfig["destinations"] = ofJson::array();
				for (const auto & dest : destinations) {
					ofJson destJson;
					destJson["name"] = dest.name;
					destJson["ip"] = dest.ip;
					destJson["port"] = dest.port;
					destJson["enabled"] = dest.enabled;
					oscConfig["destinations"].push_back(destJson);
				}
			}

			json["oscOut"] = oscConfig;
		}
	}

	return json;
}

bool HourGlassManager::parseHourGlassJson(const ofJson & json) {
	try {

		if (!json.contains("name")) {
			ofLogError("HourGlassManager") << "❌ Missing 'name' field in hourglass JSON";
			return false;
		}
		std::string name = json["name"];

		if (!json.contains("upLedId")) {
			ofLogError("HourGlassManager") << "❌ Missing 'upLedId' field in hourglass JSON";
			return false;
		}
		int upLedId = json["upLedId"];

		if (!json.contains("downLedId")) {
			ofLogError("HourGlassManager") << "❌ Missing 'downLedId' field in hourglass JSON";
			return false;
		}
		int downLedId = json["downLedId"];

		if (!json.contains("motorId")) {
			ofLogError("HourGlassManager") << "❌ Missing 'motorId' field in hourglass JSON";
			return false;
		}
		int motorId = json["motorId"];

		addHourGlass(name, upLedId, downLedId, motorId);

		// Setup OSC Out if configuration exists
		if (json.contains("oscOut")) {

			auto hg = getHourGlass(name);
			if (hg) {
				hg->setupOSCOutFromJson(json["oscOut"]);
				hg->enableOSCOut(true);
			}
		}

		return true;

	} catch (const std::exception & e) {
		ofLogError("HourGlassManager") << "❌ Error parsing hourglass JSON: " << e.what();
		return false;
	}
}