#include "OSCController.h"
#include "OSCHelper.h"
#include "UIWrapper.h"
#include "ofMain.h"

OSCController::OSCController(HourGlassManager * manager)
	: hourglassManager(manager)
	, uiWrapper(nullptr)
	, oscEnabled(false)
	, receivePort(8000) {
	ofLogNotice("OSCController") << "🎛️ OSC Controller initialized";
	loadMotorPresets(); // Load presets on construction
}

OSCController::~OSCController() {
	shutdown();
}

void OSCController::setup(int receivePort) {
	this->receivePort = receivePort;
	receiver.setup(receivePort);
	ofLogNotice("OSCController") << "📡 OSC Receiver listening on port " << receivePort;
	oscEnabled = true;
	ofLogNotice("OSCController") << "✅ OSC Controller setup complete";
}

void OSCController::update() {
	if (!oscEnabled) return;

	while (receiver.hasWaitingMessages()) {
		ofxOscMessage message;
		receiver.getNextMessage(message);
		processMessage(message);
	}
	processLastCommands();
}

void OSCController::shutdown() {
	if (oscEnabled) {
		oscEnabled = false;
		ofLogNotice("OSCController") << "🔌 OSC Controller shutdown";
	}
}

void OSCController::processMessage(ofxOscMessage & message) {
	if (uiWrapper) {
		uiWrapper->notifyOSCMessageReceived();
	}

	string address = message.getAddress();
	vector<string> addressParts = splitAddress(address);

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
			handleConnectionMessage(message);
		} else if (addressParts.size() >= 3) {
			// Route to handlers based on command type - let each handler validate IDs
			if (addressParts[2] == "motor") {
				handleMotorMessage(message);
			} else if (addressParts[2] == "led" || addressParts[2] == "pwm" || addressParts[2] == "dotstar" || addressParts[2] == "main" || addressParts[2] == "up" || addressParts[2] == "down") {
				handleLedMessage(message);
			} else if (addressParts[2] == "connect" || addressParts[2] == "disconnect" || addressParts[2] == "status") {
				handleConnectionMessage(message);
			} else if (addressParts[2] == "blackout") {
				// Treat /hourglass/{id}/blackout as setting individual luminosity to 0
				ofxOscMessage lumMsg; // Create a new message to pass to luminosity handler
				lumMsg.setAddress(message.getAddress()); // Keep original address for logging context if needed
				lumMsg.addFloatArg(0.0f); // Add 0.0f as the luminosity value
				handleIndividualLuminosityMessage(lumMsg);
			} else if (addressParts[2] == "luminosity") {
				handleIndividualLuminosityMessage(message);
			} else if (addressParts[2] == "motor" && addressParts.size() >= 4 && addressParts[3] == "preset") {
				handleMotorPresetMessage(message);
			} else if (addressParts[2] == "motor" && addressParts.size() >= 6 && addressParts[3] == "config") {
				handleIndividualMotorConfigMessage(message, addressParts);
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
			} else if (addressParts[1] == "list_devices") {
				handleSystemMessage(message);
			} else if (addressParts[1] == "emergency_stop_all") {
				handleSystemMessage(message);
			} else if (addressParts.size() >= 3 && addressParts[1] == "motor" && addressParts[2] == "preset") {
				handleSystemMotorPresetMessage(message);
			} else if (addressParts.size() >= 5 && addressParts[1] == "motor" && addressParts[2] == "config") {
				handleSystemMotorConfigMessage(message, addressParts);
			} else if (addressParts.size() >= 4 && addressParts[1] == "motor" && addressParts[2] == "rotate") {
				handleSystemMotorRotateMessage(message, addressParts);
			} else if (addressParts.size() >= 4 && addressParts[1] == "motor" && addressParts[2] == "position") {
				handleSystemMotorPositionMessage(message, addressParts);
			} else if (addressParts.size() >= 3 && addressParts[1] == "motor" && addressParts[2] == "set_zero_all") {
				handleSystemSetZeroAllMessage(message);
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

void OSCController::handleConnectionMessage(ofxOscMessage & msg) {
	string address = msg.getAddress();
	vector<string> addressParts = splitAddress(address);

	if (addressParts.size() == 2) {
		if (addressParts[1] == "connect") {
			hourglassManager->connectAll();
			ofLogNotice("OSCController") << "🔗 OSC: Connect all hourglasses command received";
		} else if (addressParts[1] == "disconnect") {
			hourglassManager->disconnectAll();
			ofLogNotice("OSCController") << "❌ OSC: Disconnect all hourglasses command received";
		} else if (addressParts[1] == "status") {
			ofLogNotice("OSCController") << "📊 OSC: Global status request received (no longer broadcasts)";
		}
	} else if (addressParts.size() == 3) {
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
		if (addressParts[2] == "connect") {
			bool success = hg->connect();
			ofLogNotice("OSCController") << "🔗 OSC: " << (success ? "Connected" : "Failed to connect") << " hourglass " << hourglassId;
		} else if (addressParts[2] == "disconnect") {
			hg->disconnect();
			ofLogNotice("OSCController") << "❌ OSC: Disconnected hourglass " << hourglassId;
		} else if (addressParts[2] == "status") {
			ofLogNotice("OSCController") << "📊 OSC: Status request for hourglass " << hourglassId << " (connected: " << hg->isConnected() << ") (no longer broadcasts)";
		}
	}
}

void OSCController::handleMotorMessage(ofxOscMessage & msg) {
	string address = msg.getAddress();
	vector<string> addressParts = splitAddress(address);
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
	if (!hg || !hg->isConnected()) {
		sendError(address, "Hourglass not connected: " + addressParts[1]);
		return;
	}
	MotorController * motor = hg->getMotor();
	string command = addressParts[3];

	if (command == "enable") {
		if (!OSCHelper::validateParameters(msg, 1, "motor_enable")) return;
		bool enable = OSCHelper::getArgument<bool>(msg, 0, true);
		hg->updatingFromOSC = true;
		hg->motorEnabled.set(enable);
		hg->updatingFromOSC = false;
		ofLogNotice("OSCController") << "⚡ OSC: Motor enabled state set to " << (enable ? "true" : "false") << " for hourglass " << hourglassId;
	} else if (command == "emergency_stop") {
		motor->emergencyStop();
		hg->emergencyStop();
		ofLogNotice("OSCController") << "🚨 OSC: Emergency stop for hourglass " << hourglassId;
	} else if (command == "set_zero") {
		motor->setZero();
		hg->setMotorZero();
		ofLogNotice("OSCController") << "🎯 OSC: Set zero for hourglass " << hourglassId;
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
		ofLogNotice("OSCController") << "↕️ OSC: Commanded relative move " << steps << " for HG " << hourglassId;
	} else if (command == "move_absolute") {
		if (!OSCHelper::validateParameters(msg, 1, "motor_move_absolute")) return;
		int position = OSCHelper::getArgument<int>(msg, 0, 0);
		std::optional<int> speed_opt = msg.getNumArgs() > 1 ? std::optional<int>(OSCHelper::getArgument<int>(msg, 1)) : std::nullopt;
		std::optional<int> accel_opt = msg.getNumArgs() > 2 ? std::optional<int>(OSCHelper::getArgument<int>(msg, 2)) : std::nullopt;
		hg->commandAbsoluteMove(position, speed_opt, accel_opt);
		ofLogNotice("OSCController") << "📍 OSC: Commanded absolute move to " << position << " for HG " << hourglassId;
	} else if (command == "rotate") {
		float degrees_val;
		std::optional<int> speed_opt = std::nullopt;
		std::optional<int> accel_opt = std::nullopt;
		if (addressParts.size() >= 5) {
			try {
				degrees_val = std::stof(addressParts[4]);
				if (addressParts.size() >= 6) speed_opt = std::stoi(addressParts[5]);
				if (addressParts.size() >= 7) accel_opt = std::stoi(addressParts[6]);
			} catch (const std::exception &) {
				sendError(address, "Invalid path format for rotate");
				return;
			}
		} else {
			if (!OSCHelper::validateParameters(msg, 1, "motor_rotate")) return;
			degrees_val = OSCHelper::getArgument<float>(msg, 0, 0.0f);
			if (msg.getNumArgs() > 1) speed_opt = OSCHelper::getArgument<int>(msg, 1);
			if (msg.getNumArgs() > 2) accel_opt = OSCHelper::getArgument<int>(msg, 2);
		}
		hg->commandRelativeAngle(degrees_val, speed_opt, accel_opt);
		updateUIAngleParameters(degrees_val, 0);
		ofLogNotice("OSCController") << "🔄 OSC: Commanded relative angle " << degrees_val << " for HG " << hourglassId;
	} else if (command == "position") {
		float degrees_val;
		std::optional<int> speed_opt = std::nullopt;
		std::optional<int> accel_opt = std::nullopt;
		if (addressParts.size() >= 5) {
			try {
				degrees_val = std::stof(addressParts[4]);
				if (addressParts.size() >= 6) speed_opt = std::stoi(addressParts[5]);
				if (addressParts.size() >= 7) accel_opt = std::stoi(addressParts[6]);
			} catch (const std::exception &) {
				sendError(address, "Invalid path format for position");
				return;
			}
		} else {
			if (!OSCHelper::validateParameters(msg, 1, "motor_position")) return;
			degrees_val = OSCHelper::getArgument<float>(msg, 0, 0.0f);
			if (msg.getNumArgs() > 1) speed_opt = OSCHelper::getArgument<int>(msg, 1);
			if (msg.getNumArgs() > 2) accel_opt = OSCHelper::getArgument<int>(msg, 2);
		}
		hg->commandAbsoluteAngle(degrees_val, speed_opt, accel_opt);
		updateUIAngleParameters(0, degrees_val);
		ofLogNotice("OSCController") << "🎯 OSC: Commanded absolute angle to " << degrees_val << " for HG " << hourglassId;
	} else {
		sendError(address, "Unknown motor command: " + command);
	}
}

void OSCController::handleLedMessage(ofxOscMessage & msg) {
	string address = msg.getAddress();
	vector<string> addressParts = splitAddress(address);
	if (addressParts.size() < 4) {
		sendError(address, "Incomplete LED command");
		return;
	}

	// Use new multi-targeting function
	std::vector<int> hourglassIds = extractHourglassIds(addressParts);
	if (hourglassIds.empty()) {
		sendError(address, "Invalid hourglass target: " + addressParts[1] + " (use: 1, 1-3, or all)");
		return;
	}

	std::vector<HourGlass *> hourglasses = getHourglassesByIds(hourglassIds);
	if (hourglasses.empty()) {
		sendError(address, "No valid hourglasses found for target: " + addressParts[1]);
		return;
	}

	string target = addressParts[2];
	string command = addressParts[3];

	// Apply to all targeted hourglasses
	for (HourGlass * hg : hourglasses) {
		if (!hg || !hg->isConnected()) continue;

		int hourglassId = -1;
		for (size_t i = 0; i < hourglassManager->getHourGlassCount(); i++) {
			if (hourglassManager->getHourGlass(i) == hg) {
				hourglassId = i + 1;
				break;
			}
		}
		if (hourglassId == -1) continue;

		if (target == "pwm") {
			handlePWMMessageForHourglass(msg, hg, hourglassId, addressParts);
			continue;
		}
		if (target == "main") {
			handleMainLedMessageForHourglass(msg, hg, hourglassId, addressParts);
			continue;
		}

		if (target == "up" || target == "down") {
			handleIndividualLedMessageForHourglass(msg, hg, hourglassId, target, command, addressParts);
		} else if (target == "led") {
			handleAllLedMessageForHourglass(msg, hg, hourglassId, command, addressParts);
		} else {
			// Only send error once, not per hourglass
			if (hg == hourglasses[0]) {
				sendError(address, "Unknown LED target: " + target);
			}
		}
	}

	// Log success for multi-targeting
	if (hourglassIds.size() > 1) {
		std::string targetStr = addressParts[1];
		ofLogNotice("OSCController") << "🎨 OSC: LED command applied to " << hourglasses.size() << " hourglasses (target: " << targetStr << ")";
	}
}

void OSCController::handleIndividualLedMessageForHourglass(ofxOscMessage & msg, HourGlass * hg, int hourglassId, const string & target, const string & command, const vector<string> & addressParts) {
	string address = msg.getAddress();
	bool shouldUpdateUI = uiWrapper && hourglassId == (uiWrapper->getCurrentHourGlass() + 1);

	hg->updatingFromOSC = true;

	if (command == "rgb") {
		if (!OSCHelper::validateParameters(msg, 3, "led_rgb")) {
			hg->updatingFromOSC = false;
			return;
		}
		int r = OSCHelper::getArgument<int>(msg, 0, 0), g = OSCHelper::getArgument<int>(msg, 1, 0), b = OSCHelper::getArgument<int>(msg, 2, 0);
		if (!OSCHelper::isValidColorValue(r) || !OSCHelper::isValidColorValue(g) || !OSCHelper::isValidColorValue(b)) {
			sendError(address, "Invalid RGB values (0-255)");
			hg->updatingFromOSC = false;
			return;
		}
		if (target == "up")
			hg->upLedColor.set(ofColor(r, g, b));
		else
			hg->downLedColor.set(ofColor(r, g, b));
	} else if (command == "brightness") {
		if (!OSCHelper::validateParameters(msg, 1, "led_brightness")) {
			hg->updatingFromOSC = false;
			return;
		}
		int brightness = OSCHelper::getArgument<int>(msg, 0, 0);
		if (!OSCHelper::isValidColorValue(brightness)) {
			sendError(address, "Invalid brightness value (0-255)");
			hg->updatingFromOSC = false;
			return;
		}
		if (target == "up")
			hg->upLedColor.set(ofColor(brightness, brightness, brightness));
		else
			hg->downLedColor.set(ofColor(brightness, brightness, brightness));

	} else if (command == "blend") {
		if (!OSCHelper::validateParameters(msg, 1, "led_blend")) {
			hg->updatingFromOSC = false;
			return;
		}
		int blend = OSCHelper::getArgument<int>(msg, 0, 0);
		if (blend < 0 || blend > 768) {
			sendError(address, "Invalid blend value (0-768)");
			hg->updatingFromOSC = false;
			return;
		}

		if (target == "up") {
			hg->upLedBlend.set(blend);
		} else {
			hg->downLedBlend.set(blend);
		}

		if (shouldUpdateUI) {
			if (target == "up") {
				uiWrapper->updateUpLedBlendFromOSC(blend);
			} else {
				uiWrapper->updateDownLedBlendFromOSC(blend);
			}
		}
	} else if (command == "origin") {
		if (!OSCHelper::validateParameters(msg, 1, "led_origin")) {
			hg->updatingFromOSC = false;
			return;
		}
		int origin = OSCHelper::getArgument<int>(msg, 0, 0);
		if (origin < 0 || origin > 360) {
			sendError(address, "Invalid origin value (0-360)");
			hg->updatingFromOSC = false;
			return;
		}

		if (target == "up") {
			hg->upLedOrigin.set(origin);
		} else {
			hg->downLedOrigin.set(origin);
		}

		if (shouldUpdateUI) {
			if (target == "up") {
				uiWrapper->updateUpLedOriginFromOSC(origin);
			} else {
				uiWrapper->updateDownLedOriginFromOSC(origin);
			}
		}
	} else if (command == "arc") {
		if (!OSCHelper::validateParameters(msg, 1, "led_arc")) {
			hg->updatingFromOSC = false;
			return;
		}
		int arc = OSCHelper::getArgument<int>(msg, 0, 360);
		if (arc < 0 || arc > 360) {
			sendError(address, "Invalid arc value (0-360)");
			hg->updatingFromOSC = false;
			return;
		}

		if (target == "up") {
			hg->upLedArc.set(arc);
		} else {
			hg->downLedArc.set(arc);
		}

		if (shouldUpdateUI) {
			if (target == "up") {
				uiWrapper->updateUpLedArcFromOSC(arc);
			} else {
				uiWrapper->updateDownLedArcFromOSC(arc);
			}
		}
	} else {
		sendError(address, "Unknown " + target + " LED command: " + command);
	}
	hg->updatingFromOSC = false;
}

void OSCController::handleAllLedMessageForHourglass(ofxOscMessage & msg, HourGlass * hg, int hourglassId, const string & command, const vector<string> & addressParts) {
	string address = msg.getAddress();
	bool shouldUpdateUI = uiWrapper && hourglassId == (uiWrapper->getCurrentHourGlass() + 1);

	hg->updatingFromOSC = true;

	if (command == "all") {
		if (addressParts.size() >= 5 && addressParts[4] == "rgb") {
			if (msg.getNumArgs() == 3) {
				int r, g, b;
				if (msg.getArgType(0) == OFXOSC_TYPE_FLOAT) {
					r = static_cast<uint8_t>(OSCHelper::clamp(OSCHelper::getArgument<float>(msg, 0), 0.0f, 255.0f));
					g = static_cast<uint8_t>(OSCHelper::clamp(OSCHelper::getArgument<float>(msg, 1), 0.0f, 255.0f));
					b = static_cast<uint8_t>(OSCHelper::clamp(OSCHelper::getArgument<float>(msg, 2), 0.0f, 255.0f));
				} else {
					r = static_cast<uint8_t>(OSCHelper::clamp(OSCHelper::getArgument<int>(msg, 0), 0, 255));
					g = static_cast<uint8_t>(OSCHelper::clamp(OSCHelper::getArgument<int>(msg, 1), 0, 255));
					b = static_cast<uint8_t>(OSCHelper::clamp(OSCHelper::getArgument<int>(msg, 2), 0, 255));
				}
				hg->setAllLEDs(r, g, b);
			} else if (msg.getNumArgs() == 1 && msg.getArgType(0) == OFXOSC_TYPE_RGBA_COLOR) {
				ofColor color = msg.getArgAsRgbaColor(0);
				hg->setAllLEDs(color.r, color.g, color.b);
			} else {
				sendError(address, "Invalid RGB format. Expected 3 numbers or RGBA color type");
			}
		} else if (addressParts.size() >= 5 && addressParts[4] == "blend") {
			if (!OSCHelper::validateParameters(msg, 1, "led_all_blend")) {
				hg->updatingFromOSC = false;
				return;
			}
			int blend = OSCHelper::getArgument<int>(msg, 0, 0);
			if (blend < 0 || blend > 768) {
				sendError(address, "Invalid blend value (0-768)");
				hg->updatingFromOSC = false;
				return;
			}

			// CRITICAL FIX: Update HourGlass parameters so hardware gets updated
			hg->upLedBlend.set(blend);
			hg->downLedBlend.set(blend);

			if (shouldUpdateUI) {
				uiWrapper->updateUpLedBlendFromOSC(blend);
				uiWrapper->updateDownLedBlendFromOSC(blend);
			}
		} else if (addressParts.size() >= 5 && addressParts[4] == "origin") {
			if (!OSCHelper::validateParameters(msg, 1, "led_all_origin")) {
				hg->updatingFromOSC = false;
				return;
			}
			int origin = OSCHelper::getArgument<int>(msg, 0, 0);
			if (origin < 0 || origin > 360) {
				sendError(address, "Invalid origin value (0-360)");
				hg->updatingFromOSC = false;
				return;
			}

			// CRITICAL FIX: Update HourGlass parameters so hardware gets updated
			hg->upLedOrigin.set(origin);
			hg->downLedOrigin.set(origin);

			if (shouldUpdateUI) {
				uiWrapper->updateUpLedOriginFromOSC(origin);
				uiWrapper->updateDownLedOriginFromOSC(origin);
			}
		} else if (addressParts.size() >= 5 && addressParts[4] == "arc") {
			if (!OSCHelper::validateParameters(msg, 1, "led_all_arc")) {
				hg->updatingFromOSC = false;
				return;
			}
			int arc = OSCHelper::getArgument<int>(msg, 0, 360);
			if (arc < 0 || arc > 360) {
				sendError(address, "Invalid arc value (0-360)");
				hg->updatingFromOSC = false;
				return;
			}

			// CRITICAL FIX: Update HourGlass parameters so hardware gets updated
			hg->upLedArc.set(arc);
			hg->downLedArc.set(arc);

			if (shouldUpdateUI) {
				uiWrapper->updateUpLedArcFromOSC(arc);
				uiWrapper->updateDownLedArcFromOSC(arc);
			}
		} else {
			sendError(address, "Unknown 'all' LED command: " + (addressParts.size() > 4 ? addressParts[4] : " (expected rgb/blend/origin/arc)"));
		}
	} else {
		sendError(address, "Unknown LED command: " + command);
	}
	hg->updatingFromOSC = false;
}

void OSCController::handlePWMMessageForHourglass(ofxOscMessage & msg, HourGlass * hg, int hourglassId, const vector<string> & addressParts) {
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

void OSCController::handleMainLedMessageForHourglass(ofxOscMessage & msg, HourGlass * hg, int hourglassId, const vector<string> & addressParts) {
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

void OSCController::handleSystemMessage(ofxOscMessage & msg) {
	string address = msg.getAddress();
	vector<string> addressParts = splitAddress(address);
	if (addressParts.size() < 2) {
		sendError(address, "Incomplete system command");
		return;
	}
	string command = addressParts[1];
	if (command == "list_devices") {
		auto devices = hourglassManager->getAvailableSerialPorts();
		ofLogNotice("OSCController") << "📋 OSC: List devices command received. Devices available: " << devices.size();
		for (const auto & device : devices) {
			ofLogNotice("OSCController") << "  - " << device;
		}
	} else if (command == "emergency_stop_all") {
		for (size_t i = 0; i < hourglassManager->getHourGlassCount(); i++) {
			auto * hg = hourglassManager->getHourGlass(i);
			if (hg && hg->isConnected()) hg->emergencyStop();
		}
		ofLogNotice("OSCController") << "🚨 OSC: Emergency stop ALL hourglasses command received";
	} else {
		sendError(address, "Unknown system command: " + command);
	}
}

void OSCController::handleGlobalBlackoutMessage(ofxOscMessage & msg) {
	ofLogNotice("OSCController") << "⚫ OSC: GLOBAL Blackout command received. Setting global luminosity to 0.";
	LedMagnetController::setGlobalLuminosity(0.0f);
	if (uiWrapper) {
		uiWrapper->updateGlobalLuminositySlider(0.0f);
	}
	forceRefreshAllHardwareStates();
}

void OSCController::handleGlobalLuminosityMessage(ofxOscMessage & msg) {
	string address = msg.getAddress();
	if (!OSCHelper::validateParameters(msg, 1, "system_luminosity")) return;

	float luminosity = OSCHelper::getArgument<float>(msg, 0, 1.0f);
	luminosity = OSCHelper::clamp(luminosity, 0.0f, 1.0f);

	LedMagnetController::setGlobalLuminosity(luminosity);
	ofLogNotice("OSCController") << "💡 OSC: Global luminosity set to " << luminosity;
	if (uiWrapper) {
		uiWrapper->updateGlobalLuminositySlider(luminosity);
	}
	forceRefreshAllHardwareStates();
}

void OSCController::handleIndividualLuminosityMessage(ofxOscMessage & msg) {
	string address = msg.getAddress();
	vector<string> addressParts = splitAddress(address);

	if (addressParts.size() < 3) {
		OSCHelper::logError("IndividualLuminosity", address, "Incomplete address for individual luminosity/blackout.");
		return;
	}

	if (!OSCHelper::validateParameters(msg, 1, "individual_luminosity")) return;
	float luminosityValue = OSCHelper::getArgument<float>(msg, 0, 1.0f);
	luminosityValue = OSCHelper::clamp(luminosityValue, 0.0f, 1.0f);

	// Handle "all" syntax for individual luminosity
	if (addressParts[1] == "all") {
		ofLogNotice("OSCController") << "💡 OSC: Setting individual luminosity to " << luminosityValue << " for ALL hourglasses";

		for (size_t i = 0; i < hourglassManager->getHourGlassCount(); i++) {
			HourGlass * hg = hourglassManager->getHourGlass(i);
			if (hg) {
				hg->individualLuminosity.set(luminosityValue);
			}
		}

		// Update UI if current hourglass is being affected
		if (uiWrapper) {
			uiWrapper->updateCurrentIndividualLuminositySlider(luminosityValue);
		}

		forceRefreshAllHardwareStates();
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

	hg->individualLuminosity.set(luminosityValue);

	if (uiWrapper && hourglassId == (uiWrapper->getCurrentHourGlass() + 1)) {
		uiWrapper->updateCurrentIndividualLuminositySlider(luminosityValue);
	}

	forceRefreshAllHardwareStates();
}

void OSCController::handleMotorPresetMessage(ofxOscMessage & msg) {
	string address = msg.getAddress();
	vector<string> addressParts = splitAddress(address);

	if (addressParts.size() < 4) {
		sendError(address, "Incomplete motor preset command.");
		return;
	}

	int hourglassId = extractHourglassId(addressParts);
	if (!isValidHourglassId(hourglassId)) {
		sendError(address, "Invalid hourglass ID for motor preset: " + addressParts[1]);
		return;
	}

	HourGlass * hg = getHourglassById(hourglassId);
	if (!hg || !hg->isConnected()) {
		sendError(address, "Hourglass not connected for motor preset: " + addressParts[1]);
		return;
	}

	if (!OSCHelper::validateParameters(msg, 1, "motor_preset")) return;
	std::string presetName = OSCHelper::getArgument<std::string>(msg, 0, "smooth");

	auto it = motorPresets.find(presetName);
	if (it != motorPresets.end()) {
		int newSpeed = it->second.first;
		int newAccel = it->second.second;

		if (!OSCHelper::isValidMotorSpeed(newSpeed) || !OSCHelper::isValidMotorAcceleration(newAccel)) {
			sendError(address, "Preset '" + presetName + "' contains invalid speed/acceleration values.");
			return;
		}

		hg->updatingFromOSC = true;
		hg->motorSpeed.set(newSpeed);
		hg->motorAcceleration.set(newAccel);
		hg->updatingFromOSC = false;
		ofLogNotice("OSCController") << "🏃🚀 OSC: Motor preset '" << presetName
									 << "' applied to HG " << hourglassId
									 << " (Speed: " << newSpeed << ", Accel: " << newAccel << ")";
	} else {
		sendError(address, "Unknown motor preset: '" + presetName + "'. Loaded presets: " + ofToString(motorPresets.size()));
	}
}

void OSCController::handleSystemMotorPresetMessage(ofxOscMessage & msg) {
	string address = msg.getAddress();
	if (!OSCHelper::validateParameters(msg, 1, "system_motor_preset")) return;
	std::string presetName = OSCHelper::getArgument<std::string>(msg, 0, "smooth");

	auto it = motorPresets.find(presetName);
	if (it != motorPresets.end()) {
		int newSpeed = it->second.first;
		int newAccel = it->second.second;

		if (!OSCHelper::isValidMotorSpeed(newSpeed) || !OSCHelper::isValidMotorAcceleration(newAccel)) {
			OSCHelper::logError("system_motor_preset", address, "Preset '" + presetName + "' contains invalid speed/acceleration values.");
			return;
		}

		ofLogNotice("OSCController") << "🏃🚀 OSC: System motor preset '" << presetName << "' applying to ALL hourglasses (Speed: " << newSpeed << ", Accel: " << newAccel << ")";
		for (size_t i = 0; i < hourglassManager->getHourGlassCount(); ++i) {
			HourGlass * hg = hourglassManager->getHourGlass(i);
			if (hg && hg->isConnected()) {
				hg->updatingFromOSC = true;
				hg->motorSpeed.set(newSpeed);
				hg->motorAcceleration.set(newAccel);
				hg->updatingFromOSC = false;
			}
		}
	} else {
		sendError(address, "Unknown system motor preset: '" + presetName + "'. Loaded presets: " + ofToString(motorPresets.size()));
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
		ofLogNotice("OSCController") << "⚙️ OSC: System motor config applying to ALL hourglasses (Speed: " << speed_val << ", Accel: " << accel_val << ")";
		for (size_t i = 0; i < hourglassManager->getHourGlassCount(); ++i) {
			HourGlass * hg = hourglassManager->getHourGlass(i);
			if (hg) { // No need to check isConnected here, just set params
				hg->updatingFromOSC = true;
				hg->motorSpeed.set(speed_val);
				hg->motorAcceleration.set(accel_val);
				hg->updatingFromOSC = false;
			}
		}
	} catch (const std::exception & e) {
		sendError(address, "Invalid number format for system motor config speed/accel: " + std::string(e.what()));
	}
}

void OSCController::handleIndividualMotorConfigMessage(ofxOscMessage & msg, const std::vector<std::string> & addressParts) {
	string address = msg.getAddress();
	if (addressParts.size() < 6) {
		sendError(address, "Incomplete individual motor config command.");
		return;
	}

	int hourglassId = extractHourglassId(addressParts);
	if (!isValidHourglassId(hourglassId)) {
		sendError(address, "Invalid hourglass ID for motor config: " + addressParts[1]);
		return;
	}
	HourGlass * hg = getHourglassById(hourglassId);
	if (!hg || !hg->isConnected()) {
		sendError(address, "Hourglass not connected for motor config: " + addressParts[1]);
		return;
	}

	try {
		int speed_val = std::stoi(addressParts[4]);
		int accel_val = std::stoi(addressParts[5]);

		if (!OSCHelper::isValidMotorSpeed(speed_val) || !OSCHelper::isValidMotorAcceleration(accel_val)) {
			OSCHelper::logError("individual_motor_config", address, "Invalid speed/acceleration for HG " + ofToString(hourglassId) + ". Speed(0-500): " + ofToString(speed_val) + ", Accel(0-255): " + ofToString(accel_val));
			return;
		}

		hg->updatingFromOSC = true;
		hg->motorSpeed.set(speed_val);
		hg->motorAcceleration.set(accel_val);
		hg->updatingFromOSC = false;
	} catch (const std::exception & e) {
		sendError(address, "Invalid number format for individual motor config speed/accel: " + std::string(e.what()));
	}
}

void OSCController::handleSystemMotorRotateMessage(ofxOscMessage & msg, const std::vector<std::string> & addressParts) {
	string address = msg.getAddress();
	if (addressParts.size() < 4) {
		sendError(address, "Incomplete system motor rotate. Expected /system/motor/rotate/{angle}/{speed?}/{accel?}");
		return;
	}
	try {
		float degrees = std::stof(addressParts[3]);
		std::optional<int> speed_opt = (addressParts.size() > 4) ? std::optional<int>(std::stoi(addressParts[4])) : std::nullopt;
		std::optional<int> accel_opt = (addressParts.size() > 5) ? std::optional<int>(std::stoi(addressParts[5])) : std::nullopt;

		ofLogNotice("OSCController") << "🔄 OSC: System motor rotate " << degrees << "° applying to ALL HGs.";
		for (size_t i = 0; i < hourglassManager->getHourGlassCount(); ++i) {
			HourGlass * hg = hourglassManager->getHourGlass(i);
			if (hg && hg->isConnected()) { // Check isConnected before commanding a move
				// Here, we use the provided speed/accel if available, otherwise HourGlass::applyMotorParameters will use its own defaults
				hg->commandRelativeAngle(degrees, speed_opt, accel_opt);
			}
		}
	} catch (const std::exception & e) {
		sendError(address, "Invalid number format for system motor rotate parameters: " + std::string(e.what()));
	}
}

void OSCController::handleSystemMotorPositionMessage(ofxOscMessage & msg, const std::vector<std::string> & addressParts) {
	string address = msg.getAddress();
	if (addressParts.size() < 4) {
		sendError(address, "Incomplete system motor position. Expected /system/motor/position/{angle}/{speed?}/{accel?}");
		return;
	}
	try {
		float degrees = std::stof(addressParts[3]);
		std::optional<int> speed_opt = (addressParts.size() > 4) ? std::optional<int>(std::stoi(addressParts[4])) : std::nullopt;
		std::optional<int> accel_opt = (addressParts.size() > 5) ? std::optional<int>(std::stoi(addressParts[5])) : std::nullopt;

		ofLogNotice("OSCController") << "🎯 OSC: System motor position to " << degrees << "° applying to ALL HGs.";
		for (size_t i = 0; i < hourglassManager->getHourGlassCount(); ++i) {
			HourGlass * hg = hourglassManager->getHourGlass(i);
			if (hg && hg->isConnected()) { // Check isConnected
				hg->commandAbsoluteAngle(degrees, speed_opt, accel_opt);
			}
		}
	} catch (const std::exception & e) {
		sendError(address, "Invalid number format for system motor position parameters: " + std::string(e.what()));
	}
}

void OSCController::handleSystemSetZeroAllMessage(ofxOscMessage & msg) {
	ofLogNotice("OSCController") << "🎯 OSC: Set Zero ALL Motors command received";
	for (size_t i = 0; i < hourglassManager->getHourGlassCount(); ++i) {
		HourGlass * hg = hourglassManager->getHourGlass(i);
		if (hg && hg->isConnected()) {
			hg->setMotorZero();
		}
	}
}

void OSCController::forceRefreshAllHardwareStates() {
	ofLogNotice("OSCController") << "Forcing refresh of all hardware states (excluding PWM).";
	for (size_t i = 0; i < hourglassManager->getHourGlassCount(); ++i) {
		HourGlass * hg = hourglassManager->getHourGlass(i);
		if (hg) {
			LedMagnetController * upMagnet = hg->getUpLedMagnet();
			if (upMagnet) {
				upMagnet->rgbInitialized = false;
				upMagnet->mainLedInitialized = false;
				// upMagnet->pwmInitialized = false; // PWM is not affected by global luminosity, so don't force resend unless its value actually changed.
			}
			LedMagnetController * downMagnet = hg->getDownLedMagnet();
			if (downMagnet) {
				downMagnet->rgbInitialized = false;
				downMagnet->mainLedInitialized = false;
				// downMagnet->pwmInitialized = false; // PWM is not affected by global luminosity
			}
		}
	}
}

void OSCController::sendError(const string & originalAddress, const string & errorMessage) {
	OSCHelper::logError("OSCController", originalAddress, errorMessage);
}

vector<string> OSCController::splitAddress(const string & address) {
	vector<string> parts;
	string addr = address;
	if (!addr.empty() && addr[0] == '/') addr = addr.substr(1);
	size_t pos = 0;
	while ((pos = addr.find('/')) != string::npos) {
		parts.push_back(addr.substr(0, pos));
		addr.erase(0, pos + 1);
	}
	if (!addr.empty()) parts.push_back(addr);
	return parts;
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
				if (id >= 1 && id <= (int)hourglassManager->getHourGlassCount()) {
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
			if (start <= end && start >= 1 && end <= (int)hourglassManager->getHourGlassCount()) {
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
		if (id >= 1 && id <= (int)hourglassManager->getHourGlassCount()) {
			ids.push_back(id);
		}
	} catch (const std::exception &) {
		// Invalid single ID
	}

	return ids;
}

std::vector<HourGlass *> OSCController::getHourglassesByIds(const std::vector<int> & ids) {
	std::vector<HourGlass *> hourglasses;
	for (int id : ids) {
		HourGlass * hg = getHourglassById(id);
		if (hg) {
			hourglasses.push_back(hg);
		}
	}
	return hourglasses;
}

HourGlass * OSCController::getHourglassById(int id) {
	if (id < 1 || id > (int)hourglassManager->getHourGlassCount()) return nullptr;
	return hourglassManager->getHourGlass(id - 1);
}

bool OSCController::isValidHourglassId(int id) {
	return id >= 1 && id <= (int)hourglassManager->getHourGlassCount();
}

void OSCController::logOSCMessage(const ofxOscMessage & msg, const string & action) {
	string args = "";
	for (int i = 0; i < msg.getNumArgs(); i++) {
		if (i > 0) args += " ";
		if (msg.getArgType(i) == OFXOSC_TYPE_INT32)
			args += ofToString(msg.getArgAsInt(i));
		else if (msg.getArgType(i) == OFXOSC_TYPE_FLOAT)
			args += ofToString(msg.getArgAsFloat(i));
		else if (msg.getArgType(i) == OFXOSC_TYPE_STRING)
			args += "\"" + msg.getArgAsString(i) + "\"";
		else if (msg.getArgType(i) == OFXOSC_TYPE_TRUE || msg.getArgType(i) == OFXOSC_TYPE_FALSE)
			args += msg.getArgAsBool(i) ? "true" : "false";
	}
	ofLogNotice("OSCController") << "OSC " << action << ": " << msg.getAddress() << (args.empty() ? "" : " [" + args + "]");
}

void OSCController::updateUIAngleParameters(float relativeAngle, float absoluteAngle) {
	if (uiWrapper) uiWrapper->updatePositionParameters(relativeAngle, absoluteAngle);
}

void OSCController::processLastCommands() {
	// The entire loop and logic for sending LED commands based on lastSentValues
	// and comparing with hg->parameters or uiWrapper->parameters is now handled by
	// HourGlass::applyLedParameters(), which is called every frame via UIWrapper::update().

	// Therefore, the LED-related command sending logic in this function should be removed.

	// If processLastCommands() had other responsibilities (e.g., for motor commands or other state sync)
	// those would remain. For now, assuming it was primarily for LEDs based on its content.

	// If there are no other tasks for processLastCommands, it can be left empty or eventually removed.
	// For safety, let's leave it empty for now.
}

void OSCController::loadMotorPresets(const std::string & filename) {
	ofJson presetsJson;
	try {
		presetsJson = ofLoadJson(filename);
	} catch (const std::exception & e) {
		ofLogError("OSCController") << "Failed to load motor presets from " << filename << ": " << e.what();
		// Fallback to hardcoded defaults if file loading fails
		motorPresets["slow"] = { 50, 50 };
		motorPresets["smooth"] = { 150, 100 };
		motorPresets["medium"] = { 200, 150 };
		motorPresets["fast"] = { 400, 200 };
		ofLogWarning("OSCController") << "Using hardcoded default motor presets.";
		return;
	}

	if (presetsJson.is_null() || !presetsJson.count("presets") || !presetsJson["presets"].is_array()) {
		ofLogError("OSCController") << "Invalid format in motor_presets.json. Expected 'presets' array.";
		// Fallback to hardcoded defaults
		motorPresets["slow"] = { 50, 50 };
		motorPresets["smooth"] = { 150, 100 };
		motorPresets["medium"] = { 200, 150 };
		motorPresets["fast"] = { 400, 200 };
		ofLogWarning("OSCController") << "Using hardcoded default motor presets due to invalid file format.";
		return;
	}

	for (const auto & item : presetsJson["presets"]) {
		if (item.count("name") && item["name"].is_string() && item.count("speed") && item["speed"].is_number_integer() && item.count("acceleration") && item["acceleration"].is_number_integer()) {

			std::string name = item["name"].get<std::string>();
			int speed = item["speed"].get<int>();
			int acceleration = item["acceleration"].get<int>();
			motorPresets[name] = { speed, acceleration };
			ofLogNotice("OSCController") << "Loaded motor preset: " << name << " (Speed: " << speed << ", Accel: " << acceleration << ")";
		} else {
			ofLogWarning("OSCController") << "Skipping invalid preset item in " << filename;
		}
	}
	if (motorPresets.empty()) {
		ofLogWarning("OSCController") << "No valid motor presets loaded from " << filename << ". Using hardcoded defaults.";
		motorPresets["slow"] = { 50, 50 };
		motorPresets["smooth"] = { 150, 100 };
		motorPresets["medium"] = { 200, 150 };
		motorPresets["fast"] = { 400, 200 };
	}
}