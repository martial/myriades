#include "OSCController.h"
#include "OSCHelper.h"
#include "UIWrapper.h"

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
			try {
				std::stoi(addressParts[1]); // Validate ID
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
			} catch (const std::exception &) {
				sendError(address, "Invalid hourglass ID: " + addressParts[1]);
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
		if (enable)
			hg->enableMotor();
		else
			hg->disableMotor();
		hg->updatingFromOSC = true;
		hg->motorEnabled.set(enable);
		hg->updatingFromOSC = false;
		ofLogNotice("OSCController") << "⚡ OSC: Motor " << (enable ? "enabled" : "disabled") << " for hourglass " << hourglassId;
	} else if (command == "emergency_stop") {
		motor->emergencyStop();
		hg->emergencyStop();
		ofLogNotice("OSCController") << "🚨 OSC: Emergency stop for hourglass " << hourglassId;
	} else if (command == "set_zero") {
		motor->setZero();
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
		// ofLogNotice("OSCController") << "🔧 OSC: Set microstep to " << microstep << " for hourglass " << hourglassId;
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
		// ofLogNotice("OSCController") << "🏃 OSC: Set default motor speed to " << speed_val << " for hourglass " << hourglassId;
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
		// ofLogNotice("OSCController") << "🚀 OSC: Set default acceleration to " << accel_val << " (0-255) for hourglass " << hourglassId;
	} else if (command == "move_relative") {
		handleMotorRelativeMessage(msg, hg, addressParts);
	} else if (command == "move_absolute") {
		handleMotorAbsoluteMessage(msg, hg, addressParts);
	} else if (command == "rotate") {
		float degrees;
		int rotate_speed;
		int rotate_accel; // Stays int
		if (addressParts.size() >= 5) {
			try {
				degrees = std::stof(addressParts[4]);
				rotate_speed = (addressParts.size() >= 6) ? std::stoi(addressParts[5]) : hg->motorSpeed.get();
				rotate_accel = (addressParts.size() >= 7) ? std::stoi(addressParts[6]) : hg->motorAcceleration.get();
			} catch (const std::exception &) {
				sendError(address, "Invalid path format for rotate");
				return;
			}
		} else {
			if (!OSCHelper::validateParameters(msg, 1, "motor_rotate")) return;
			degrees = OSCHelper::getArgument<float>(msg, 0, 0.0f);
			rotate_speed = OSCHelper::getArgument<int>(msg, 1, hg->motorSpeed.get());
			rotate_accel = OSCHelper::getArgument<int>(msg, 2, hg->motorAcceleration.get());
		}
		if (!OSCHelper::isValidAngle(degrees) || !OSCHelper::isValidMotorSpeed(rotate_speed) || !OSCHelper::isValidMotorAcceleration(rotate_accel)) { // Validates 0-255 for accel
			OSCHelper::logError("motor_rotate", address, "Invalid rotation parameters. Speed: " + ofToString(rotate_speed) + " Accel (0-255): " + ofToString(rotate_accel));
			return;
		}
		bool speedChanged = (rotate_speed != hg->motorSpeed.get());
		bool accelChanged = (rotate_accel != hg->motorAcceleration.get());
		if (speedChanged || accelChanged) {
			hg->updatingFromOSC = true;
			if (speedChanged) {
				hg->motorSpeed.set(rotate_speed);
				ofLogNotice("OSCController") << "🏃 OSC: Updated UI speed to " << rotate_speed;
			}
			if (accelChanged) {
				hg->motorAcceleration.set(rotate_accel);
				ofLogNotice("OSCController") << "🚀 OSC: Updated UI acceleration to " << rotate_accel;
			}
			hg->updatingFromOSC = false;
		}
		motor->moveRelativeAngle(rotate_speed, rotate_accel, degrees, hg->gearRatio.get(), hg->calibrationFactor.get());
		updateUIAngleParameters(degrees, 0);
	} else if (command == "position") {
		float degrees;
		int pos_speed;
		int pos_accel; // Stays int
		if (addressParts.size() >= 5) {
			try {
				degrees = std::stof(addressParts[4]);
				pos_speed = (addressParts.size() >= 6) ? std::stoi(addressParts[5]) : hg->motorSpeed.get();
				pos_accel = (addressParts.size() >= 7) ? std::stoi(addressParts[6]) : hg->motorAcceleration.get();
			} catch (const std::exception &) {
				sendError(address, "Invalid path format for position");
				return;
			}
		} else {
			if (!OSCHelper::validateParameters(msg, 1, "motor_position")) return;
			degrees = OSCHelper::getArgument<float>(msg, 0, 0.0f);
			pos_speed = OSCHelper::getArgument<int>(msg, 1, hg->motorSpeed.get());
			pos_accel = OSCHelper::getArgument<int>(msg, 2, hg->motorAcceleration.get());
		}
		if (!OSCHelper::isValidAngle(degrees) || !OSCHelper::isValidMotorSpeed(pos_speed) || !OSCHelper::isValidMotorAcceleration(pos_accel)) { // Validates 0-255 for accel
			OSCHelper::logError("motor_position", address, "Invalid position parameters. Speed: " + ofToString(pos_speed) + " Accel (0-255): " + ofToString(pos_accel));
			return;
		}
		bool speedChanged = (pos_speed != hg->motorSpeed.get());
		bool accelChanged = (pos_accel != hg->motorAcceleration.get());
		if (speedChanged || accelChanged) {
			hg->updatingFromOSC = true;
			if (speedChanged) {
				hg->motorSpeed.set(pos_speed);
				ofLogNotice("OSCController") << "🏃 OSC: Updated UI speed to " << pos_speed;
			}
			if (accelChanged) {
				hg->motorAcceleration.set(pos_accel);
				ofLogNotice("OSCController") << "🚀 OSC: Updated UI acceleration to " << pos_accel;
			}
			hg->updatingFromOSC = false;
		}
		motor->moveAbsoluteAngle(pos_speed, pos_accel, degrees, hg->gearRatio.get(), hg->calibrationFactor.get());
		updateUIAngleParameters(0, degrees);
		ofLogNotice("OSCController") << "🎯 OSC: Position to " << degrees << "° (absolute) at speed " << pos_speed << ", accel " << pos_accel << " for hourglass " << hourglassId;
	} else {
		sendError(address, "Unknown motor command: " + command);
	}
}

void OSCController::handleMotorRelativeMessage(ofxOscMessage & msg, HourGlass * hg, const std::vector<std::string> & addressParts) {
	string address = msg.getAddress();
	if (!OSCHelper::validateParameters(msg, 1, "motor_move_relative")) return;
	int steps = OSCHelper::getArgument<int>(msg, 0, 0);
	int speed = OSCHelper::getArgument<int>(msg, 1, hg->motorSpeed.get());
	float accel = OSCHelper::getArgument<float>(msg, 2, hg->motorAcceleration.get());
	if (!OSCHelper::isValidMotorSpeed(speed) || !OSCHelper::isValidMotorAcceleration(accel)) {
		sendError(address, "Invalid motor parameters");
		return;
	}
	hg->getMotor()->moveRelative(speed, accel, steps);
	ofLogNotice("OSCController") << "↕️ OSC: Move relative " << steps << " steps for hourglass " << extractHourglassId(addressParts);
}

void OSCController::handleMotorAbsoluteMessage(ofxOscMessage & msg, HourGlass * hg, const std::vector<std::string> & addressParts) {
	string address = msg.getAddress();
	if (!OSCHelper::validateParameters(msg, 1, "motor_move_absolute")) return;
	int position = OSCHelper::getArgument<int>(msg, 0, 0);
	int speed = OSCHelper::getArgument<int>(msg, 1, hg->motorSpeed.get());
	float accel = OSCHelper::getArgument<float>(msg, 2, hg->motorAcceleration.get());
	if (!OSCHelper::isValidMotorSpeed(speed) || !OSCHelper::isValidMotorAcceleration(accel)) {
		sendError(address, "Invalid motor parameters");
		return;
	}
	hg->getMotor()->moveAbsolute(speed, accel, position);
	ofLogNotice("OSCController") << "📍 OSC: Move absolute to " << position << " for hourglass " << extractHourglassId(addressParts);
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

	if (command == "rgb") {
		if (!OSCHelper::validateParameters(msg, 3, "led_rgb")) return;
		int r = OSCHelper::getArgument<int>(msg, 0, 0), g = OSCHelper::getArgument<int>(msg, 1, 0), b = OSCHelper::getArgument<int>(msg, 2, 0);
		if (!OSCHelper::isValidColorValue(r) || !OSCHelper::isValidColorValue(g) || !OSCHelper::isValidColorValue(b)) {
			sendError(address, "Invalid RGB values (0-255)");
			return;
		}
		hg->updatingFromOSC = true;
		if (target == "up")
			hg->upLedColor.set(ofColor(r, g, b));
		else
			hg->downLedColor.set(ofColor(r, g, b));
		hg->updatingFromOSC = false;
	} else if (command == "brightness") {
		if (!OSCHelper::validateParameters(msg, 1, "led_brightness")) return;
		int brightness = OSCHelper::getArgument<int>(msg, 0, 0);
		if (!OSCHelper::isValidColorValue(brightness)) {
			sendError(address, "Invalid brightness value (0-255)");
			return;
		}
		hg->updatingFromOSC = true;
		if (target == "up")
			hg->upLedColor.set(ofColor(brightness, brightness, brightness));
		else
			hg->downLedColor.set(ofColor(brightness, brightness, brightness));
		hg->updatingFromOSC = false;
	} else if (command == "blend") {
		if (!OSCHelper::validateParameters(msg, 1, "led_blend")) return;
		int blend = OSCHelper::getArgument<int>(msg, 0, 0);
		if (blend < 0 || blend > 768) {
			sendError(address, "Invalid blend value (0-768)");
			return;
		}
		if (uiWrapper && hourglassId == (uiWrapper->getCurrentHourGlass() + 1)) {
			hg->updatingFromOSC = true;
			if (target == "up")
				uiWrapper->upLedBlendParam.set(blend);
			else
				uiWrapper->downLedBlendParam.set(blend);
			hg->updatingFromOSC = false;
		}
		LastSentValues & lastSent = lastSentValues[hourglassId];
		if (target == "up")
			lastSent.upBlend = blend;
		else
			lastSent.downBlend = blend;
		lastSent.initialized = false;
	} else if (command == "origin") {
		if (!OSCHelper::validateParameters(msg, 1, "led_origin")) return;
		int origin = OSCHelper::getArgument<int>(msg, 0, 0);
		if (origin < 0 || origin > 360) {
			sendError(address, "Invalid origin value (0-360)");
			return;
		}
		if (uiWrapper && hourglassId == (uiWrapper->getCurrentHourGlass() + 1)) {
			hg->updatingFromOSC = true;
			if (target == "up")
				uiWrapper->upLedOriginParam.set(origin);
			else
				uiWrapper->downLedOriginParam.set(origin);
			hg->updatingFromOSC = false;
		}
		LastSentValues & lastSent = lastSentValues[hourglassId];
		if (target == "up")
			lastSent.upOrigin = origin;
		else
			lastSent.downOrigin = origin;
		lastSent.initialized = false;
	} else if (command == "arc") {
		if (!OSCHelper::validateParameters(msg, 1, "led_arc")) return;
		int arc = OSCHelper::getArgument<int>(msg, 0, 360);
		if (arc < 0 || arc > 360) {
			sendError(address, "Invalid arc value (0-360)");
			return;
		}
		if (uiWrapper && hourglassId == (uiWrapper->getCurrentHourGlass() + 1)) {
			hg->updatingFromOSC = true;
			if (target == "up")
				uiWrapper->upLedArcParam.set(arc);
			else
				uiWrapper->downLedArcParam.set(arc);
			hg->updatingFromOSC = false;
		}
		LastSentValues & lastSent = lastSentValues[hourglassId];
		if (target == "up")
			lastSent.upArc = arc;
		else
			lastSent.downArc = arc;
		lastSent.initialized = false;
	} else {
		sendError(address, "Unknown " + target + " LED command: " + command);
	}
}

void OSCController::handleAllLedMessageForHourglass(ofxOscMessage & msg, HourGlass * hg, int hourglassId, const string & command, const vector<string> & addressParts) {
	string address = msg.getAddress();

	if (command == "all") {
		if (addressParts.size() == 5 && addressParts[4] == "rgb") {
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
				hg->updatingFromOSC = true;
				hg->setAllLEDs(r, g, b);
				hg->updatingFromOSC = false;
			} else if (msg.getNumArgs() == 1 && msg.getArgType(0) == OFXOSC_TYPE_RGBA_COLOR) {
				ofColor color = msg.getArgAsRgbaColor(0);
				hg->updatingFromOSC = true;
				hg->setAllLEDs(color.r, color.g, color.b);
				hg->updatingFromOSC = false;
			} else {
				sendError(address, "Invalid RGB format. Expected 3 numbers or RGBA color type");
			}
		} else if (addressParts.size() == 5 && addressParts[4] == "blend") {
			if (!OSCHelper::validateParameters(msg, 1, "led_all_blend")) return;
			int blend = OSCHelper::getArgument<int>(msg, 0, 0);
			if (blend < 0 || blend > 768) {
				sendError(address, "Invalid blend value (0-768)");
				return;
			}
			if (uiWrapper && hourglassId == (uiWrapper->getCurrentHourGlass() + 1)) {
				hg->updatingFromOSC = true;
				uiWrapper->upLedBlendParam.set(blend);
				uiWrapper->downLedBlendParam.set(blend);
				hg->updatingFromOSC = false;
			}
			LastSentValues & lastSent = lastSentValues[hourglassId];
			lastSent.upBlend = blend;
			lastSent.downBlend = blend;
			lastSent.initialized = false;
		} else if (addressParts.size() == 5 && addressParts[4] == "origin") {
			if (!OSCHelper::validateParameters(msg, 1, "led_all_origin")) return;
			int origin = OSCHelper::getArgument<int>(msg, 0, 0);
			if (origin < 0 || origin > 360) {
				sendError(address, "Invalid origin value (0-360)");
				return;
			}
			if (uiWrapper && hourglassId == (uiWrapper->getCurrentHourGlass() + 1)) {
				hg->updatingFromOSC = true;
				uiWrapper->upLedOriginParam.set(origin);
				uiWrapper->downLedOriginParam.set(origin);
				hg->updatingFromOSC = false;
			}
			LastSentValues & lastSent = lastSentValues[hourglassId];
			lastSent.upOrigin = origin;
			lastSent.downOrigin = origin;
			lastSent.initialized = false;
		} else if (addressParts.size() == 5 && addressParts[4] == "arc") {
			if (!OSCHelper::validateParameters(msg, 1, "led_all_arc")) return;
			int arc = OSCHelper::getArgument<int>(msg, 0, 360);
			if (arc < 0 || arc > 360) {
				sendError(address, "Invalid arc value (0-360)");
				return;
			}
			if (uiWrapper && hourglassId == (uiWrapper->getCurrentHourGlass() + 1)) {
				hg->updatingFromOSC = true;
				uiWrapper->upLedArcParam.set(arc);
				uiWrapper->downLedArcParam.set(arc);
				hg->updatingFromOSC = false;
			}
			LastSentValues & lastSent = lastSentValues[hourglassId];
			lastSent.upArc = arc;
			lastSent.downArc = arc;
			lastSent.initialized = false;
		} else {
			sendError(address, "Unknown 'all' LED command: " + (addressParts.size() > 4 ? addressParts[4] : " (expected rgb/blend/origin/arc)"));
		}
	} else {
		sendError(address, "Unknown LED command: " + command);
	}
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
	luminosity = OSCHelper::clamp(luminosity, 0.0f, 1.0f); // Ensure it's within bounds

	LedMagnetController::setGlobalLuminosity(luminosity);
	ofLogNotice("OSCController") << "💡 OSC: Global luminosity set to " << luminosity;
	if (uiWrapper) {
		uiWrapper->updateGlobalLuminositySlider(luminosity);
	}
	forceRefreshAllHardwareStates(); // Force re-send of all values
}

void OSCController::handleIndividualLuminosityMessage(ofxOscMessage & msg) {
	string address = msg.getAddress();
	vector<string> addressParts = splitAddress(address);

	if (addressParts.size() < 3) { // Expect /hourglass/{id}/luminosity or transformed /hourglass/{id}/blackout
		OSCHelper::logError("IndividualLuminosity", address, "Incomplete address for individual luminosity/blackout.");
		return;
	}

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

	// Argument 0 should be the luminosity value (float)
	if (!OSCHelper::validateParameters(msg, 1, "individual_luminosity")) return;
	float luminosityValue = OSCHelper::getArgument<float>(msg, 0, 1.0f); // Default to 1.0 if not provided (should be caught by validateParameters)
	luminosityValue = OSCHelper::clamp(luminosityValue, 0.0f, 1.0f);

	hg->individualLuminosity.set(luminosityValue);

	// Update UI slider if this is the currently selected hourglass in the UI
	if (uiWrapper && hourglassId == (uiWrapper->getCurrentHourGlass() + 1)) {
		uiWrapper->updateCurrentIndividualLuminositySlider(luminosityValue);
	}

	forceRefreshAllHardwareStates(); // Force refresh as this specific HG's output will change
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

		// Validate preset values against allowed ranges (optional, but good practice)
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
	// Expected format: /system/motor/preset [preset_name_string]
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
	// Expected: /system/motor/config/{speed}/{accel}
	// addressParts[0] = system, [1] = motor, [2] = config, [3] = speed, [4] = accel
	if (addressParts.size() < 5) {
		sendError(address, "Incomplete system motor config command.");
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
			if (hg && hg->isConnected()) {
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
	// Expected: /hourglass/{id}/motor/config/{speed}/{accel}
	// addressParts[0]=hourglass, [1]=id, [2]=motor, [3]=config, [4]=speed, [5]=accel
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
	// Expected: /system/motor/rotate/{angle_degrees}/{speed?}/{acceleration?}
	// addressParts[0]=system, [1]=motor, [2]=rotate, [3]=angle, [4]=speed(opt), [5]=accel(opt)
	if (addressParts.size() < 4) { // Need at least angle
		sendError(address, "Incomplete system motor rotate command. Expected /system/motor/rotate/{angle}/{speed?}/{accel?}");
		return;
	}

	try {
		float degrees = std::stof(addressParts[3]);

		for (size_t i = 0; i < hourglassManager->getHourGlassCount(); ++i) {
			HourGlass * hg = hourglassManager->getHourGlass(i);
			if (hg && hg->isConnected() && hg->getMotor()) {
				int speed_val = (addressParts.size() > 4) ? std::stoi(addressParts[4]) : hg->motorSpeed.get();
				int accel_val = (addressParts.size() > 5) ? std::stoi(addressParts[5]) : hg->motorAcceleration.get();

				if (!OSCHelper::isValidAngle(degrees) || !OSCHelper::isValidMotorSpeed(speed_val) || !OSCHelper::isValidMotorAcceleration(accel_val)) {
					OSCHelper::logError("system_motor_rotate", address, "Skipping HG " + ofToString(i + 1) + ": Invalid rotate params. Angle:" + ofToString(degrees) + " Speed:" + ofToString(speed_val) + " Accel:" + ofToString(accel_val));
					continue;
				}
				hg->getMotor()->moveRelativeAngle(speed_val, accel_val, degrees, hg->gearRatio.get(), hg->calibrationFactor.get());
			}
		}
	} catch (const std::exception & e) {
		sendError(address, "Invalid number format for system motor rotate parameters: " + std::string(e.what()));
	}
}

void OSCController::handleSystemMotorPositionMessage(ofxOscMessage & msg, const std::vector<std::string> & addressParts) {
	string address = msg.getAddress();
	// Expected: /system/motor/position/{angle_degrees}/{speed?}/{acceleration?}
	if (addressParts.size() < 4) { // Need at least angle
		sendError(address, "Incomplete system motor position command. Expected /system/motor/position/{angle}/{speed?}/{accel?}");
		return;
	}

	try {
		float degrees = std::stof(addressParts[3]);
		ofLogNotice("OSCController") << "🎯 OSC: System motor position to " << degrees << "° applying to ALL HGs.";

		for (size_t i = 0; i < hourglassManager->getHourGlassCount(); ++i) {
			HourGlass * hg = hourglassManager->getHourGlass(i);
			if (hg && hg->isConnected() && hg->getMotor()) {
				int speed_val = (addressParts.size() > 4) ? std::stoi(addressParts[4]) : hg->motorSpeed.get();
				int accel_val = (addressParts.size() > 5) ? std::stoi(addressParts[5]) : hg->motorAcceleration.get();

				if (!OSCHelper::isValidAngle(degrees) || !OSCHelper::isValidMotorSpeed(speed_val) || !OSCHelper::isValidMotorAcceleration(accel_val)) {
					OSCHelper::logError("system_motor_position", address, "Skipping HG " + ofToString(i + 1) + ": Invalid position params. Angle:" + ofToString(degrees) + " Speed:" + ofToString(speed_val) + " Accel:" + ofToString(accel_val));
					continue;
				}
				hg->getMotor()->moveAbsoluteAngle(speed_val, accel_val, degrees, hg->gearRatio.get(), hg->calibrationFactor.get());
			}
		}
	} catch (const std::exception & e) {
		sendError(address, "Invalid number format for system motor position parameters: " + std::string(e.what()));
	}
}

void OSCController::forceRefreshAllHardwareStates() {
	for (auto & pair : lastSentValues) {
		pair.second.initialized = false;
	}
	// The next call to processLastCommands() will now re-evaluate and send all states.
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

// New function to extract multiple hourglass IDs
std::vector<int> OSCController::extractHourglassIds(const vector<string> & addressParts) {
	std::vector<int> ids;
	if (addressParts.size() < 2) return ids;

	std::string target = addressParts[1];

	// Handle "all" keyword
	if (target == "all") {
		for (int i = 1; i <= (int)hourglassManager->getHourGlassCount(); i++) {
			ids.push_back(i);
		}
		return ids;
	}

	// Handle comma-separated list like "1,3,5"
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

	// Handle range syntax like "1-3"
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

	// Handle single ID
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

// Helper function to get multiple hourglasses by IDs
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
	for (size_t i = 0; i < hourglassManager->getHourGlassCount(); i++) {
		HourGlass * hg = hourglassManager->getHourGlass(i);
		if (!hg || !hg->isConnected()) continue;
		int hourglassId = i + 1;
		LastSentValues & lastSent = lastSentValues[hourglassId];

		float indivLum = hg->individualLuminosity.get();

		// Determine the correct effect parameters to use
		int upBlend, upOrigin, upArc;
		int downBlend, downOrigin, downArc;

		if (uiWrapper && hourglassId == (uiWrapper->getCurrentHourGlass() + 1)) {
			// If this hourglass is active in the UI, use the UI's current slider values
			upBlend = uiWrapper->upLedBlendParam.get();
			upOrigin = uiWrapper->upLedOriginParam.get();
			upArc = uiWrapper->upLedArcParam.get();
			downBlend = uiWrapper->downLedBlendParam.get();
			downOrigin = uiWrapper->downLedOriginParam.get();
			downArc = uiWrapper->downLedArcParam.get();
		} else {
			// Otherwise, use the last known sent values for this specific hourglass
			// These are initialized to defaults (0,0,360) if never changed via UI for this HG
			upBlend = lastSent.upBlend;
			upOrigin = lastSent.upOrigin;
			upArc = lastSent.upArc;
			downBlend = lastSent.downBlend;
			downOrigin = lastSent.downOrigin;
			downArc = lastSent.downArc;
		}

		ofColor currentUpColor = hg->upLedColor.get();
		if (!lastSent.initialized || currentUpColor != lastSent.upLedColor || indivLum != lastSent.individualLuminosity || upBlend != lastSent.upBlend || upOrigin != lastSent.upOrigin || upArc != lastSent.upArc) {
			hg->getUpLedMagnet()->sendLED(currentUpColor.r, currentUpColor.g, currentUpColor.b, upBlend, upOrigin, upArc, indivLum);
			lastSent.upLedColor = currentUpColor;
			lastSent.individualLuminosity = indivLum;
			lastSent.upBlend = upBlend;
			lastSent.upOrigin = upOrigin;
			lastSent.upArc = upArc;
		}
		ofColor currentDownColor = hg->downLedColor.get();
		if (!lastSent.initialized || currentDownColor != lastSent.downLedColor || indivLum != lastSent.individualLuminosity || downBlend != lastSent.downBlend || downOrigin != lastSent.downOrigin || downArc != lastSent.downArc) {
			hg->getDownLedMagnet()->sendLED(currentDownColor.r, currentDownColor.g, currentDownColor.b, downBlend, downOrigin, downArc, indivLum);
			lastSent.downLedColor = currentDownColor;
			lastSent.individualLuminosity = indivLum; // Also update for down, in case up wasn't sent
			lastSent.downBlend = downBlend;
			lastSent.downOrigin = downOrigin;
			lastSent.downArc = downArc;
		}

		int currentUpMainLed = hg->upMainLed.get();
		if (!lastSent.initialized || currentUpMainLed != lastSent.upMainLed || indivLum != lastSent.individualLuminosity) {
			hg->getUpLedMagnet()->sendLED(static_cast<uint8_t>(currentUpMainLed), indivLum);
			lastSent.upMainLed = currentUpMainLed;
			lastSent.individualLuminosity = indivLum; // Ensure indivLum is stored if only main LED changes
		}
		int currentDownMainLed = hg->downMainLed.get();
		if (!lastSent.initialized || currentDownMainLed != lastSent.downMainLed || indivLum != lastSent.individualLuminosity) {
			hg->getDownLedMagnet()->sendLED(static_cast<uint8_t>(currentDownMainLed), indivLum);
			lastSent.downMainLed = currentDownMainLed;
			lastSent.individualLuminosity = indivLum; // Ensure indivLum is stored
		}

		// PWM is not affected by luminosity
		int currentUpPwm = hg->upPwm.get();
		if (!lastSent.initialized || currentUpPwm != lastSent.upPwm) {
			hg->getUpLedMagnet()->sendPWM(static_cast<uint8_t>(currentUpPwm));
			lastSent.upPwm = currentUpPwm;
		}
		int currentDownPwm = hg->downPwm.get();
		if (!lastSent.initialized || currentDownPwm != lastSent.downPwm) {
			hg->getDownLedMagnet()->sendPWM(static_cast<uint8_t>(currentDownPwm));
			lastSent.downPwm = currentDownPwm;
		}

		lastSent.initialized = true;
	}
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