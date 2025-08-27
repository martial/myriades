#include "HourGlassManager.h"
#include "SerialPortManager.h"

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

	ofLogNotice("HourGlassManager") << "🔍 Loading configuration from: " << configFile;

	ofFile file(configFile);
	if (!file.exists()) {
		ofLogWarning("HourGlassManager") << "Config file not found: " << configFile << " - Creating default";
		createDefaultConfiguration();
		return saveConfiguration(configFile);
	}

	try {
		ofJson json = ofLoadJson(configFile);
		ofLogNotice("HourGlassManager") << "📄 JSON file loaded successfully";

		if (!json.contains("hourglasses")) {
			ofLogError("HourGlassManager") << "Invalid config file: missing 'hourglasses' array";
			return false;
		}

		// Load shared serial port configuration
		if (json.contains("serialPort")) {
			sharedSerialPort = json["serialPort"];
			ofLogNotice("HourGlassManager") << "📡 Serial Port from JSON: " << sharedSerialPort;
		}
		if (json.contains("baudRate")) {
			sharedBaudRate = json["baudRate"];
			ofLogNotice("HourGlassManager") << "⚡ Baud Rate from JSON: " << sharedBaudRate;
		}

		// Clear existing hourglasses
		disconnectAll();
		hourglasses.clear();

		// Load each hourglass
		ofLogNotice("HourGlassManager") << "🔧 Parsing hourglasses from JSON...";
		for (const auto & hourglassJson : json["hourglasses"]) {
			if (!parseHourGlassJson(hourglassJson)) {
				ofLogError("HourGlassManager") << "Failed to parse hourglass configuration";
				return false;
			}
		}

		ofLogNotice("HourGlassManager") << "✅ Successfully loaded " << hourglasses.size() << " hourglasses from " << configFile;
		return true;

	} catch (const std::exception & e) {
		ofLogError("HourGlassManager") << "❌ Error loading config: " << e.what();
		ofLogWarning("HourGlassManager") << "🔄 Falling back to default configuration";
		createDefaultConfiguration();
		return false;
	}
}

bool HourGlassManager::saveConfiguration(const std::string & configFile) {
	ofLogNotice("HourGlassManager") << "💾 Saving configuration to: " << configFile;

	try {
		ofJson json;
		json["serialPort"] = sharedSerialPort;
		json["baudRate"] = sharedBaudRate;
		json["hourglasses"] = ofJson::array();

		for (const auto & hourglass : hourglasses) {
			json["hourglasses"].push_back(createHourGlassJson(*hourglass));
		}

		ofSaveJson(configFile, json);
		ofLogNotice("HourGlassManager") << "✅ Configuration saved to " << configFile;
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

	ofLogNotice("HourGlassManager") << "🔧 Creating default HourGlass configuration...";
	ofLogNotice("HourGlassManager") << "📡 Shared Serial Port: " << sharedSerialPort << " @ " << sharedBaudRate << " baud";

	// Add default hourglasses
	addHourGlass("HourGlass1", 11, 12, 1);
	addHourGlass("HourGlass2", 21, 22, 2);

	ofLogNotice("HourGlassManager") << "🎯 Default configuration complete: " << hourglasses.size() << " hourglasses created";
}

void HourGlassManager::addHourGlass(const std::string & name, int upLedId, int downLedId, int motorId) {
	auto hourglass = std::unique_ptr<HourGlass>(new HourGlass(name));
	hourglass->configure(sharedSerialPort, sharedBaudRate, upLedId, downLedId, motorId);
	hourglasses.push_back(std::move(hourglass));

	ofLogNotice("HourGlassManager") << "✅ Created HourGlass: " << name
									<< " | Port: " << sharedSerialPort
									<< " | Baud: " << sharedBaudRate
									<< " | UpLED: " << upLedId
									<< " | DownLED: " << downLedId
									<< " | Motor: " << motorId;
}

bool HourGlassManager::removeHourGlass(const std::string & name) {
	auto it = std::find_if(hourglasses.begin(), hourglasses.end(),
		[&name](const std::unique_ptr<HourGlass> & hg) {
			return hg->getName() == name;
		});

	if (it != hourglasses.end()) {
		(*it)->disconnect();
		hourglasses.erase(it);
		ofLogNotice("HourGlassManager") << "Removed hourglass: " << name;
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

	ofLogNotice("HourGlassManager") << "Connect all: " << (allConnected ? "SUCCESS" : "PARTIAL");
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
	ofLogNotice("HourGlassManager") << "Disconnected all hourglasses";
}

void HourGlassManager::disconnectHourGlass(const std::string & name) {
	auto * hourglass = getHourGlass(name);
	if (hourglass) {
		hourglass->disconnect();
		ofLogNotice("HourGlassManager") << "Disconnected hourglass: " << name;
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
	auto & portManager = SerialPortManager::getInstance();
	return portManager.getAvailablePorts();
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
				for (const auto& dest : destinations) {
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
		ofLogNotice("HourGlassManager") << "🔍 Parsing hourglass JSON entry...";

		if (!json.contains("name")) {
			ofLogError("HourGlassManager") << "❌ Missing 'name' field in hourglass JSON";
			return false;
		}
		std::string name = json["name"];
		ofLogNotice("HourGlassManager") << "📝 Found name: " << name;

		if (!json.contains("upLedId")) {
			ofLogError("HourGlassManager") << "❌ Missing 'upLedId' field in hourglass JSON";
			return false;
		}
		int upLedId = json["upLedId"];
		ofLogNotice("HourGlassManager") << "⬆️ Found upLedId: " << upLedId;

		if (!json.contains("downLedId")) {
			ofLogError("HourGlassManager") << "❌ Missing 'downLedId' field in hourglass JSON";
			return false;
		}
		int downLedId = json["downLedId"];
		ofLogNotice("HourGlassManager") << "⬇️ Found downLedId: " << downLedId;

		if (!json.contains("motorId")) {
			ofLogError("HourGlassManager") << "❌ Missing 'motorId' field in hourglass JSON";
			return false;
		}
		int motorId = json["motorId"];
		ofLogNotice("HourGlassManager") << "🔧 Found motorId: " << motorId;

		ofLogNotice("HourGlassManager") << "🎯 Creating HourGlass from JSON: " << name
										<< " (UpLED:" << upLedId << ", DownLED:" << downLedId << ", Motor:" << motorId << ")";

		addHourGlass(name, upLedId, downLedId, motorId);
		
		// Setup OSC Out if configuration exists
		if (json.contains("oscOut")) {
			ofLogNotice("HourGlassManager") << "📡 Setting up OSC Out for: " << name;
			auto hg = getHourGlass(name);
			if (hg) {
				hg->setupOSCOutFromJson(json["oscOut"]);
				hg->enableOSCOut(true);
				ofLogNotice("HourGlassManager") << "✅ OSC Out configured for: " << name;
			}
		}
		
		return true;

	} catch (const std::exception & e) {
		ofLogError("HourGlassManager") << "❌ Error parsing hourglass JSON: " << e.what();
		return false;
	}
}