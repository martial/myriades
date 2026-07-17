#include "OSCController.h"
#include "OSCHelper.h"
#include "UIWrapper.h"
#include "ofMain.h"

OSCController::OSCController(HourGlassManager * manager)
	: hourglassManager(manager)
	, uiWrapper(nullptr)
	, oscEnabled(false)
	, receivePort(8000) {

	loadMotorPresets(); // Load presets on construction
}

OSCController::~OSCController() {
	shutdown();
}

void OSCController::setup(int receivePort) {
	this->receivePort = receivePort;
	receiver.setup(receivePort);

	oscEnabled = true;
}

void OSCController::update() {
	if (!oscEnabled) return;

	while (receiver.hasWaitingMessages()) {
		ofxOscMessage message;
		receiver.getNextMessage(message);
		processMessage(message);
	}
}

void OSCController::shutdown() {
	if (oscEnabled) {
		oscEnabled = false;
	}
}

void OSCController::processMessage(ofxOscMessage & message) {
	if (uiWrapper) {
		uiWrapper->notifyOSCMessageReceived();
	}

	string address = message.getAddress();
	vector<string> addressParts = ofSplitString(address, "/", true);

	if (addressParts.empty()) {
		sendError(address, "Invalid OSC address");
		return;
	}

	// Handle blackout command
	if (address == "/blackout") {
		handleGlobalBlackoutMessage(message);
		return;
	}

	// Route to appropriate handler based on address structure
	if (addressParts[0] == "hourglass") {
		if (addressParts.size() >= 2 && (addressParts[1] == "connect" || addressParts[1] == "disconnect" || addressParts[1] == "status")) {
			// Connection commands are no-ops in OSC-only mode (serial removed)
		} else if (addressParts.size() >= 3) {
			// Route to handlers based on command type - let each handler validate IDs
			if (addressParts[2] == "motor") {
				handleMotorMessage(message, addressParts);
			} else if (addressParts[2] == "led" || addressParts[2] == "pwm" || addressParts[2] == "dotstar" || addressParts[2] == "main" || addressParts[2] == "up" || addressParts[2] == "down") {
				handleLedMessage(message, addressParts);
			} else if (addressParts[2] == "connect" || addressParts[2] == "disconnect" || addressParts[2] == "status") {
				// Connection commands are no-ops in OSC-only mode (serial removed)
			} else if (addressParts[2] == "blackout") {
				// /hourglass/{id}/blackout == individual luminosity 0
				applyIndividualLuminosity(addressParts, address, 0.0f);
			} else if (addressParts[2] == "luminosity") {
				handleIndividualLuminosityMessage(message, addressParts);
			} else {
				sendError(address, "Unknown hourglass command or motor subcommand: " + addressParts[2] + (addressParts.size() >= 4 ? "/" + addressParts[3] : ""));
			}
		} else {
			sendError(address, "Incomplete hourglass address");
		}
	} else if (addressParts[0] == "system") {
		if (addressParts.size() >= 2) {
			if (addressParts[1] == "luminosity") {
				handleGlobalLuminosityMessage(message);
			} else if (addressParts[1] == "list_devices" || addressParts[1] == "emergency_stop_all") {
				handleSystemMessage(message, addressParts);
			} else if (addressParts.size() >= 3 && addressParts[1] == "motor" && addressParts[2] == "preset") {
				handleSystemMotorPresetMessage(message);
			} else if (addressParts.size() >= 5 && addressParts[1] == "motor" && addressParts[2] == "config") {
				handleSystemMotorConfigMessage(message, addressParts);
			} else if (addressParts.size() >= 4 && addressParts[1] == "motor" && addressParts[2] == "rotate") {
				handleSystemMotorRotateMessage(message, addressParts);
			} else if (addressParts.size() >= 4 && addressParts[1] == "motor" && addressParts[2] == "position") {
				handleSystemMotorPositionMessage(message, addressParts);
			} else if (addressParts.size() >= 3 && addressParts[1] == "motor" && addressParts[2] == "set_zero_all") {
				hourglassManager->setZeroAll();
			} else {
				sendError(address, "Unknown system command: " + addressParts[1] + (addressParts.size() >= 3 ? "/" + addressParts[2] : ""));
			}
		} else {
			sendError(address, "Incomplete system command.");
		}
	} else {
		sendError(address, "Unknown OSC namespace: " + addressParts[0]);
	}
}

// Shared helpers ------------------------------------------------------------

bool OSCController::lookupPreset(const std::string & presetName, const std::string & address, int & speed, int & accel) {
	auto it = motorPresets.find(presetName);
	if (it == motorPresets.end()) {
		sendError(address, "Unknown motor preset: '" + presetName + "'. Loaded presets: " + ofToString(motorPresets.size()));
		return false;
	}
	speed = it->second.first;
	accel = it->second.second;
	if (!OSCHelper::isValidMotorSpeed(speed) || !OSCHelper::isValidMotorAcceleration(accel)) {
		sendError(address, "Preset '" + presetName + "' contains invalid speed/acceleration values.");
		return false;
	}
	return true;
}

void OSCController::applyMotorSpeedAccel(HourGlass & hg, int speed, int accel) {
	hg.updatingFromOSC = true;
	hg.motorSpeed.set(speed);
	hg.motorAcceleration.set(accel);
	hg.updatingFromOSC = false;
}

// Parse degrees/speed/accel either from the address path (starting at angleIdx)
// or, when the path carries no angle, from the message arguments.
bool OSCController::parseAngleSpeedAccel(ofxOscMessage & msg, const std::vector<std::string> & addressParts, size_t angleIdx,
	const std::string & context, float & degrees, std::optional<int> & speed, std::optional<int> & accel) {
	if (addressParts.size() > angleIdx) {
		try {
			degrees = std::stof(addressParts[angleIdx]);
			if (addressParts.size() > angleIdx + 1) speed = std::stoi(addressParts[angleIdx + 1]);
			if (addressParts.size() > angleIdx + 2) accel = std::stoi(addressParts[angleIdx + 2]);
			return true;
		} catch (const std::exception &) {
			sendError(msg.getAddress(), "Invalid path format for " + context);
			return false;
		}
	}

	if (!OSCHelper::validateParameters(msg, 1, context)) return false;
	degrees = OSCHelper::getArgument<float>(msg, 0, 0.0f);
	if (msg.getNumArgs() > 1) speed = OSCHelper::getArgument<int>(msg, 1);
	if (msg.getNumArgs() > 2) accel = OSCHelper::getArgument<int>(msg, 2);
	return true;
}

// Validate a single int argument against [minValue, maxValue] and assign it to
// the given up/down LED parameters (either may be null).
void OSCController::setLedRangeParam(ofxOscMessage & msg, const std::string & context, int minValue, int maxValue, int defaultValue,
	ofParameter<int> * upParam, ofParameter<int> * downParam) {
	if (!OSCHelper::validateParameters(msg, 1, "led_" + context)) return;
	int value = OSCHelper::getArgument<int>(msg, 0, defaultValue);
	if (value < minValue || value > maxValue) {
		sendError(msg.getAddress(), "Invalid " + context + " value (" + ofToString(minValue) + "-" + ofToString(maxValue) + ")");
		return;
	}
	if (upParam) upParam->set(value);
	if (downParam) downParam->set(value);
}

// Motor ---------------------------------------------------------------------

void OSCController::handleMotorMessage(ofxOscMessage & msg, const vector<string> & addressParts) {
	string address = msg.getAddress();
	if (addressParts.size() < 4) {
		sendError(address, "Incomplete motor command");
		return;
	}

	// Motor commands don't support "all" syntax - validate single ID
	if (addressParts[1] == "all") {
		sendError(address, "Motor commands don't support 'all' syntax. Use individual hourglass IDs or system commands.");
		return;
	}

	int hourglassId = extractHourglassId(addressParts);
	if (!isValidHourglassId(hourglassId)) {
		sendError(address, "Invalid hourglass ID: " + addressParts[1]);
		return;
	}
	HourGlass * hg = getHourglassById(hourglassId);
	if (!hg) {
		sendError(address, "Hourglass not found: " + addressParts[1]);
		return;
	}
	string command = addressParts[3];

	if (command == "enable") {
		if (!OSCHelper::validateParameters(msg, 1, "motor_enable")) return;
		bool enable = OSCHelper::getArgument<bool>(msg, 0, true);
		hg->updatingFromOSC = true;
		hg->motorEnabled.set(enable);
		hg->updatingFromOSC = false;

	} else if (command == "emergency_stop") {
		hg->emergencyStop();

	} else if (command == "set_zero") {
		hg->setMotorZero();

	} else if (command == "microstep") {
		if (!OSCHelper::validateParameters(msg, 1, "motor_microstep")) return;
		int microstep = OSCHelper::getArgument<int>(msg, 0, 16);
		if (!OSCHelper::isValidMicrostep(microstep)) {
			sendError(address, "Invalid microstep value: " + ofToString(microstep));
			return;
		}
		hg->updatingFromOSC = true;
		hg->microstep.set(microstep);
		hg->updatingFromOSC = false;
		hg->applyMotorParameters();
	} else if (command == "speed") {
		if (!OSCHelper::validateParameters(msg, 1, "motor_speed")) return;
		int speed_val = OSCHelper::getArgument<int>(msg, 0, 100);
		if (!OSCHelper::isValidMotorSpeed(speed_val)) {
			sendError(address, "Invalid motor speed: " + ofToString(speed_val));
			return;
		}
		hg->updatingFromOSC = true;
		hg->motorSpeed.set(speed_val);
		hg->updatingFromOSC = false;
	} else if (command == "acceleration") {
		if (!OSCHelper::validateParameters(msg, 1, "motor_acceleration")) return;
		int accel_val = OSCHelper::getArgument<int>(msg, 0, 128);
		if (!OSCHelper::isValidMotorAcceleration(accel_val)) {
			OSCHelper::logError("motor_acceleration", address, "Invalid motor acceleration: " + ofToString(accel_val) + " (expected 0-255)");
			return;
		}
		hg->updatingFromOSC = true;
		hg->motorAcceleration.set(accel_val);
		hg->updatingFromOSC = false;
	} else if (command == "move_relative") {
		if (!OSCHelper::validateParameters(msg, 1, "motor_move_relative")) return;
		int steps = OSCHelper::getArgument<int>(msg, 0, 0);
		std::optional<int> speed_opt = msg.getNumArgs() > 1 ? std::optional<int>(OSCHelper::getArgument<int>(msg, 1)) : std::nullopt;
		std::optional<int> accel_opt = msg.getNumArgs() > 2 ? std::optional<int>(OSCHelper::getArgument<int>(msg, 2)) : std::nullopt;
		hg->commandRelativeMove(steps, speed_opt, accel_opt);

	} else if (command == "move_absolute") {
		if (!OSCHelper::validateParameters(msg, 1, "motor_move_absolute")) return;
		int position = OSCHelper::getArgument<int>(msg, 0, 0);
		std::optional<int> speed_opt = msg.getNumArgs() > 1 ? std::optional<int>(OSCHelper::getArgument<int>(msg, 1)) : std::nullopt;
		std::optional<int> accel_opt = msg.getNumArgs() > 2 ? std::optional<int>(OSCHelper::getArgument<int>(msg, 2)) : std::nullopt;
		hg->commandAbsoluteMove(position, speed_opt, accel_opt);

	} else if (command == "rotate" || command == "position") {
		float degrees_val = 0.0f;
		std::optional<int> speed_opt = std::nullopt;
		std::optional<int> accel_opt = std::nullopt;
		if (!parseAngleSpeedAccel(msg, addressParts, 4, "motor_" + command, degrees_val, speed_opt, accel_opt)) return;

		if (command == "rotate") {
			hg->commandRelativeAngle(degrees_val, speed_opt, accel_opt);
			updateUIAngleParameters(degrees_val, 0);
		} else {
			hg->commandAbsoluteAngle(degrees_val, speed_opt, accel_opt);
			updateUIAngleParameters(0, degrees_val);
		}

	} else if (command == "preset") {
		// /hourglass/{id}/motor/preset "name"
		if (!OSCHelper::validateParameters(msg, 1, "motor_preset")) return;
		std::string presetName = OSCHelper::getArgument<std::string>(msg, 0, "smooth");
		int speed, accel;
		if (lookupPreset(presetName, address, speed, accel)) {
			applyMotorSpeedAccel(*hg, speed, accel);
		}

	} else if (command == "config") {
		// /hourglass/{id}/motor/config/{speed}/{accel}
		if (addressParts.size() < 6) {
			sendError(address, "Incomplete motor config command. Expected /hourglass/{id}/motor/config/{speed}/{accel}");
			return;
		}
		try {
			int speed_val = std::stoi(addressParts[4]);
			int accel_val = std::stoi(addressParts[5]);

			if (!OSCHelper::isValidMotorSpeed(speed_val) || !OSCHelper::isValidMotorAcceleration(accel_val)) {
				OSCHelper::logError("motor_config", address, "Invalid speed/acceleration for HG " + ofToString(hourglassId) + ". Speed(0-500): " + ofToString(speed_val) + ", Accel(0-255): " + ofToString(accel_val));
				return;
			}
			applyMotorSpeedAccel(*hg, speed_val, accel_val);
		} catch (const std::exception & e) {
			sendError(address, "Invalid number format for motor config speed/accel: " + std::string(e.what()));
		}

	} else {
		sendError(address, "Unknown motor command: " + command);
	}
}

// LED -----------------------------------------------------------------------

void OSCController::handleLedMessage(ofxOscMessage & msg, const vector<string> & addressParts) {
	string address = msg.getAddress();
	if (addressParts.size() < 4) {
		sendError(address, "Incomplete LED command");
		return;
	}

	std::vector<int> hourglassIds = extractHourglassIds(addressParts);
	if (hourglassIds.empty()) {
		sendError(address, "Invalid hourglass target: " + addressParts[1] + " (use: 1, 1-3, or all)");
		return;
	}

	string target = addressParts[2];
	string command = addressParts[3];

	// Apply to all targeted hourglasses
	bool firstHourglass = true;
	for (int hourglassId : hourglassIds) {
		HourGlass * hg = getHourglassById(hourglassId);
		if (!hg) continue;

		if (target == "pwm") {
			handlePWMMessageForHourglass(msg, hg, addressParts);
		} else if (target == "main") {
			handleMainLedMessageForHourglass(msg, hg, addressParts);
		} else if (target == "up" || target == "down") {
			handleIndividualLedMessageForHourglass(msg, hg, target, command);
		} else if (target == "led") {
			handleAllLedMessageForHourglass(msg, hg, command, addressParts);
		} else if (firstHourglass) {
			// Only send error once, not per hourglass
			sendError(address, "Unknown LED target: " + target);
		}
		firstHourglass = false;
	}
}

void OSCController::handleIndividualLedMessageForHourglass(ofxOscMessage & msg, HourGlass * hg, const string & target, const string & command) {
	string address = msg.getAddress();
	bool isUp = (target == "up");

	hg->updatingFromOSC = true;

	if (command == "rgb") {
		if (OSCHelper::validateParameters(msg, 3, "led_rgb")) {
			int r = OSCHelper::getArgument<int>(msg, 0, 0), g = OSCHelper::getArgument<int>(msg, 1, 0), b = OSCHelper::getArgument<int>(msg, 2, 0);
			if (!OSCHelper::isValidColorValue(r) || !OSCHelper::isValidColorValue(g) || !OSCHelper::isValidColorValue(b)) {
				sendError(address, "Invalid RGB values (0-255)");
			} else {
				(isUp ? hg->upLedColor : hg->downLedColor).set(ofColor(r, g, b));
			}
		}
	} else if (command == "brightness") {
		if (OSCHelper::validateParameters(msg, 1, "led_brightness")) {
			int brightness = OSCHelper::getArgument<int>(msg, 0, 0);
			if (!OSCHelper::isValidColorValue(brightness)) {
				sendError(address, "Invalid brightness value (0-255)");
			} else {
				(isUp ? hg->upLedColor : hg->downLedColor).set(ofColor(brightness, brightness, brightness));
			}
		}
	} else if (command == "blend") {
		setLedRangeParam(msg, "blend", 0, 768, 0, isUp ? &hg->upLedBlend : &hg->downLedBlend, nullptr);
	} else if (command == "origin") {
		setLedRangeParam(msg, "origin", 0, 360, 0, isUp ? &hg->upLedOrigin : &hg->downLedOrigin, nullptr);
	} else if (command == "arc") {
		setLedRangeParam(msg, "arc", 0, 360, 360, isUp ? &hg->upLedArc : &hg->downLedArc, nullptr);
	} else {
		sendError(address, "Unknown " + target + " LED command: " + command);
	}

	hg->updatingFromOSC = false;
}

void OSCController::handleAllLedMessageForHourglass(ofxOscMessage & msg, HourGlass * hg, const string & command, const vector<string> & addressParts) {
	string address = msg.getAddress();

	hg->updatingFromOSC = true;

	if (command == "all" && addressParts.size() >= 5) {
		const string & subCommand = addressParts[4];
		if (subCommand == "rgb") {
			if (msg.getNumArgs() == 3) {
				int r, g, b;
				if (msg.getArgType(0) == OFXOSC_TYPE_FLOAT) {
					r = static_cast<uint8_t>(ofClamp(OSCHelper::getArgument<float>(msg, 0), 0.0f, 255.0f));
					g = static_cast<uint8_t>(ofClamp(OSCHelper::getArgument<float>(msg, 1), 0.0f, 255.0f));
					b = static_cast<uint8_t>(ofClamp(OSCHelper::getArgument<float>(msg, 2), 0.0f, 255.0f));
				} else {
					r = static_cast<uint8_t>(ofClamp(OSCHelper::getArgument<int>(msg, 0), 0, 255));
					g = static_cast<uint8_t>(ofClamp(OSCHelper::getArgument<int>(msg, 1), 0, 255));
					b = static_cast<uint8_t>(ofClamp(OSCHelper::getArgument<int>(msg, 2), 0, 255));
				}
				hg->setAllLEDs(r, g, b);
			} else if (msg.getNumArgs() == 1 && msg.getArgType(0) == OFXOSC_TYPE_RGBA_COLOR) {
				ofColor color = msg.getArgAsRgbaColor(0);
				hg->setAllLEDs(color.r, color.g, color.b);
			} else {
				sendError(address, "Invalid RGB format. Expected 3 numbers or RGBA color type");
			}
		} else if (subCommand == "blend") {
			setLedRangeParam(msg, "blend", 0, 768, 0, &hg->upLedBlend, &hg->downLedBlend);
		} else if (subCommand == "origin") {
			setLedRangeParam(msg, "origin", 0, 360, 0, &hg->upLedOrigin, &hg->downLedOrigin);
		} else if (subCommand == "arc") {
			setLedRangeParam(msg, "arc", 0, 360, 360, &hg->upLedArc, &hg->downLedArc);
		} else {
			sendError(address, "Unknown 'all' LED command: " + subCommand);
		}
	} else if (command == "all") {
		sendError(address, "Unknown 'all' LED command:  (expected rgb/blend/origin/arc)");
	} else {
		sendError(address, "Unknown LED command: " + command);
	}

	hg->updatingFromOSC = false;
}

void OSCController::handlePWMMessageForHourglass(ofxOscMessage & msg, HourGlass * hg, const vector<string> & addressParts) {
	string address = msg.getAddress();
	if (addressParts.size() < 4) {
		sendError(address, "Incomplete PWM command");
		return;
	}
	string target = addressParts[3];
	if (target != "up" && target != "down" && target != "all") {
		sendError(address, "Invalid PWM target");
		return;
	}
	if (!OSCHelper::validateParameters(msg, 1, "pwm")) return;
	int pwmValue = OSCHelper::getArgument<int>(msg, 0, 0);
	if (!OSCHelper::isValidPWMValue(pwmValue)) {
		sendError(address, "Invalid PWM value (0-255)");
		return;
	}

	hg->updatingFromOSC = true;
	if (target == "up" || target == "all") hg->upPwm.set(pwmValue);
	if (target == "down" || target == "all") hg->downPwm.set(pwmValue);
	hg->updatingFromOSC = false;
}

void OSCController::handleMainLedMessageForHourglass(ofxOscMessage & msg, HourGlass * hg, const vector<string> & addressParts) {
	string address = msg.getAddress();
	if (addressParts.size() < 4) {
		sendError(address, "Incomplete Main LED command");
		return;
	}
	string target = addressParts[3];
	if (target != "up" && target != "down" && target != "all") {
		sendError(address, "Invalid Main LED target");
		return;
	}
	if (!OSCHelper::validateParameters(msg, 1, "main_led")) return;
	int ledValue = OSCHelper::getArgument<int>(msg, 0, 0);
	if (!OSCHelper::isValidColorValue(ledValue)) {
		sendError(address, "Invalid Main LED value (0-255)");
		return;
	}

	hg->updatingFromOSC = true;
	if (target == "up" || target == "all") hg->upMainLed.set(ledValue);
	if (target == "down" || target == "all") hg->downMainLed.set(ledValue);
	hg->updatingFromOSC = false;
}

// System --------------------------------------------------------------------

void OSCController::handleSystemMessage(ofxOscMessage & msg, const vector<string> & addressParts) {
	string address = msg.getAddress();
	if (addressParts.size() < 2) {
		sendError(address, "Incomplete system command");
		return;
	}
	string command = addressParts[1];
	if (command == "list_devices") {
		auto devices = hourglassManager->getAvailableSerialPorts();

	} else if (command == "emergency_stop_all") {
		hourglassManager->emergencyStopAll();

	} else {
		sendError(address, "Unknown system command: " + command);
	}
}

void OSCController::handleGlobalBlackoutMessage(ofxOscMessage & msg) {
	LedMagnetController::setGlobalLuminosity(0.0f);
	if (uiWrapper) {
		uiWrapper->updateGlobalLuminositySlider(0.0f);
	}
	hourglassManager->refreshAllLedStates();
}

void OSCController::handleGlobalLuminosityMessage(ofxOscMessage & msg) {
	if (!OSCHelper::validateParameters(msg, 1, "system_luminosity")) return;

	float luminosity = OSCHelper::getArgument<float>(msg, 0, 1.0f);
	luminosity = ofClamp(luminosity, 0.0f, 1.0f);

	LedMagnetController::setGlobalLuminosity(luminosity);

	if (uiWrapper) {
		uiWrapper->updateGlobalLuminositySlider(luminosity);
	}
	hourglassManager->refreshAllLedStates();
}

void OSCController::applyIndividualLuminosity(const std::vector<std::string> & addressParts, const std::string & address, float value) {
	if (addressParts.size() < 3) {
		OSCHelper::logError("IndividualLuminosity", address, "Incomplete address for individual luminosity/blackout.");
		return;
	}

	// Handle "all" syntax for individual luminosity
	if (addressParts[1] == "all") {
		hourglassManager->forEachHourGlass([value](HourGlass & hg) {
			hg.individualLuminosity.set(value);
		});

		if (uiWrapper) {
			uiWrapper->updateCurrentIndividualLuminositySlider(value);
		}

		hourglassManager->refreshAllLedStates();
		return;
	}

	// Handle individual hourglass
	int hourglassId = extractHourglassId(addressParts);
	if (!isValidHourglassId(hourglassId)) {
		OSCHelper::logError("IndividualLuminosity", address, "Invalid hourglass ID: " + addressParts[1]);
		return;
	}

	HourGlass * hg = getHourglassById(hourglassId);
	if (!hg) {
		OSCHelper::logError("IndividualLuminosity", address, "Hourglass not found: " + addressParts[1]);
		return;
	}

	hg->individualLuminosity.set(value);

	if (uiWrapper && hourglassId == (uiWrapper->getCurrentHourGlass() + 1)) {
		uiWrapper->updateCurrentIndividualLuminositySlider(value);
	}

	// Only this hourglass's effective output changed - refresh only its caches
	hg->refreshLedState();
}

void OSCController::handleIndividualLuminosityMessage(ofxOscMessage & msg, const vector<string> & addressParts) {
	if (!OSCHelper::validateParameters(msg, 1, "individual_luminosity")) return;
	float luminosityValue = OSCHelper::getArgument<float>(msg, 0, 1.0f);
	luminosityValue = ofClamp(luminosityValue, 0.0f, 1.0f);

	applyIndividualLuminosity(addressParts, msg.getAddress(), luminosityValue);
}

void OSCController::handleSystemMotorPresetMessage(ofxOscMessage & msg) {
	string address = msg.getAddress();
	if (!OSCHelper::validateParameters(msg, 1, "system_motor_preset")) return;
	std::string presetName = OSCHelper::getArgument<std::string>(msg, 0, "smooth");

	int speed, accel;
	if (lookupPreset(presetName, address, speed, accel)) {
		hourglassManager->forEachHourGlass([speed, accel](HourGlass & hg) {
			applyMotorSpeedAccel(hg, speed, accel);
		});
	}
}

void OSCController::handleSystemMotorConfigMessage(ofxOscMessage & msg, const std::vector<std::string> & addressParts) {
	string address = msg.getAddress();
	if (addressParts.size() < 5) {
		sendError(address, "Incomplete system motor config command. Expected /system/motor/config/{speed}/{accel}");
		return;
	}
	try {
		int speed_val = std::stoi(addressParts[3]);
		int accel_val = std::stoi(addressParts[4]);

		if (!OSCHelper::isValidMotorSpeed(speed_val) || !OSCHelper::isValidMotorAcceleration(accel_val)) {
			OSCHelper::logError("system_motor_config", address, "Invalid speed/acceleration. Speed(0-500): " + ofToString(speed_val) + ", Accel(0-255): " + ofToString(accel_val));
			return;
		}

		hourglassManager->forEachHourGlass([speed_val, accel_val](HourGlass & hg) {
			applyMotorSpeedAccel(hg, speed_val, accel_val);
		});
	} catch (const std::exception & e) {
		sendError(address, "Invalid number format for system motor config speed/accel: " + std::string(e.what()));
	}
}

void OSCController::handleSystemMotorRotateMessage(ofxOscMessage & msg, const std::vector<std::string> & addressParts) {
	float degrees = 0.0f;
	std::optional<int> speed_opt = std::nullopt;
	std::optional<int> accel_opt = std::nullopt;
	if (!parseAngleSpeedAccel(msg, addressParts, 3, "system_motor_rotate", degrees, speed_opt, accel_opt)) return;

	hourglassManager->forEachHourGlass([&](HourGlass & hg) {
		hg.commandRelativeAngle(degrees, speed_opt, accel_opt);
	});
}

void OSCController::handleSystemMotorPositionMessage(ofxOscMessage & msg, const std::vector<std::string> & addressParts) {
	float degrees = 0.0f;
	std::optional<int> speed_opt = std::nullopt;
	std::optional<int> accel_opt = std::nullopt;
	if (!parseAngleSpeedAccel(msg, addressParts, 3, "system_motor_position", degrees, speed_opt, accel_opt)) return;

	hourglassManager->forEachHourGlass([&](HourGlass & hg) {
		hg.commandAbsoluteAngle(degrees, speed_opt, accel_opt);
	});
}

// Utilities -----------------------------------------------------------------

void OSCController::sendError(const string & originalAddress, const string & errorMessage) {
	OSCHelper::logError("OSCController", originalAddress, errorMessage);
}

int OSCController::extractHourglassId(const vector<string> & addressParts) {
	if (addressParts.size() < 2) return -1;
	try {
		return std::stoi(addressParts[1]);
	} catch (const std::exception &) {
		return -1;
	}
}

std::vector<int> OSCController::extractHourglassIds(const vector<string> & addressParts) {
	std::vector<int> ids;
	if (addressParts.size() < 2) return ids;

	std::string target = addressParts[1];

	if (target == "all") {
		for (int i = 1; i <= (int)hourglassManager->getHourGlassCount(); i++) {
			ids.push_back(i);
		}
		return ids;
	}

	size_t commaPos = target.find(',');
	if (commaPos != std::string::npos) {
		std::string remaining = target;
		while (!remaining.empty()) {
			size_t nextComma = remaining.find(',');
			std::string idStr = (nextComma != std::string::npos) ? remaining.substr(0, nextComma) : remaining;

			try {
				int id = std::stoi(idStr);
				if (isValidHourglassId(id)) {
					ids.push_back(id);
				}
			} catch (const std::exception &) {
				// Invalid ID in comma list, skip this one
			}

			if (nextComma == std::string::npos) break;
			remaining = remaining.substr(nextComma + 1);
		}
		return ids;
	}

	size_t dashPos = target.find('-');
	if (dashPos != std::string::npos) {
		try {
			int start = std::stoi(target.substr(0, dashPos));
			int end = std::stoi(target.substr(dashPos + 1));
			if (start <= end && isValidHourglassId(start) && isValidHourglassId(end)) {
				for (int i = start; i <= end; i++) {
					ids.push_back(i);
				}
			}
		} catch (const std::exception &) {
			// Invalid range format, return empty
		}
		return ids;
	}

	try {
		int id = std::stoi(target);
		if (isValidHourglassId(id)) {
			ids.push_back(id);
		}
	} catch (const std::exception &) {
		// Invalid single ID
	}

	return ids;
}

HourGlass * OSCController::getHourglassById(int id) {
	if (!isValidHourglassId(id)) return nullptr;
	return hourglassManager->getHourGlass(id - 1);
}

bool OSCController::isValidHourglassId(int id) {
	return id >= 1 && id <= (int)hourglassManager->getHourGlassCount();
}

void OSCController::updateUIAngleParameters(float relativeAngle, float absoluteAngle) {
	if (uiWrapper) uiWrapper->updatePositionParameters(relativeAngle, absoluteAngle);
}

void OSCController::loadMotorPresets(const std::string & filename) {
	static const std::map<std::string, std::pair<int, int>> kDefaultPresets = {
		{ "slow", { 50, 50 } },
		{ "smooth", { 150, 100 } },
		{ "medium", { 200, 150 } },
		{ "fast", { 400, 200 } }
	};

	motorPresets.clear();

	ofJson presetsJson;
	try {
		presetsJson = ofLoadJson(filename);
	} catch (const std::exception & e) {
		ofLogError("OSCController") << "Failed to load motor presets from " << filename << ": " << e.what();
	}

	if (presetsJson.is_null() || !presetsJson.count("presets") || !presetsJson["presets"].is_array()) {
		ofLogWarning("OSCController") << "No valid motor presets in " << filename << ". Using hardcoded defaults.";
		motorPresets = kDefaultPresets;
		return;
	}

	for (const auto & item : presetsJson["presets"]) {
		if (item.count("name") && item["name"].is_string() && item.count("speed") && item["speed"].is_number_integer() && item.count("acceleration") && item["acceleration"].is_number_integer()) {

			std::string name = item["name"].get<std::string>();
			int speed = item["speed"].get<int>();
			int acceleration = item["acceleration"].get<int>();
			motorPresets[name] = { speed, acceleration };

		} else {
			ofLogWarning("OSCController") << "Skipping invalid preset item in " << filename;
		}
	}
	if (motorPresets.empty()) {
		ofLogWarning("OSCController") << "No valid motor presets loaded from " << filename << ". Using hardcoded defaults.";
		motorPresets = kDefaultPresets;
	}
}
