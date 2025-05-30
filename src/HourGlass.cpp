#include "HourGlass.h"

// Define the static const member for LED command interval (e.g., 50ms for 20 FPS)
const float HourGlass::MIN_LED_COMMAND_INTERVAL_MS = 50.0f;

// Constructor matches HourGlass.h (name only)
HourGlass::HourGlass(const std::string & name)
	: name(name)
	, serialPortName("") // Initialize serialPortName
	, baudRate(0)
	, upLedId(0) // Default initialize IDs
	, downLedId(0)
	, motorId(0)
	, connected(false)
	, updatingFromOSC(false)
	, lastLedCommandSendTime(0.0f)
	, motorAcceleration("motorAcceleration", 128, 0, 255)
	, individualLuminosity("individualLuminosity", 1.0f, 0.0f, 1.0f)
	, upLedBlend("upLedBlend", 0, 0, 768)
	, upLedOrigin("upLedOrigin", 0, 0, 360)
	, upLedArc("upLedArc", 360, 0, 360)
	, downLedBlend("downLedBlend", 0, 0, 768)
	, downLedOrigin("downLedOrigin", 0, 0, 360)
	, downLedArc("downLedArc", 360, 0, 360)
	, upEffectsManager()
	, downEffectsManager() {
	ofLogNotice("HourGlass") << "🏗️ Constructing HourGlass: " << name;

	// Add parameters to the group
	params.add(motorEnabled);
	params.add(microstep);
	params.add(motorSpeed);
	params.add(motorAcceleration);
	params.add(gearRatio);
	params.add(calibrationFactor);
	params.add(upLedColor);
	params.add(downLedColor);
	params.add(upMainLed);
	params.add(downMainLed);
	params.add(upPwm);
	params.add(downPwm);
	params.add(individualLuminosity);
	params.add(upLedBlend);
	params.add(upLedOrigin);
	params.add(upLedArc);
	params.add(downLedBlend);
	params.add(downLedOrigin);
	params.add(downLedArc);
}

HourGlass::~HourGlass() {
	disconnect();
}

void HourGlass::configure(const std::string & serialPort, int baudRate,
	int upLedId, int downLedId, int motorId) {
	this->serialPortName = serialPort;
	this->baudRate = baudRate;
	this->upLedId = upLedId;
	this->downLedId = downLedId;
	this->motorId = motorId;

	ofLogNotice("HourGlass") << "Configured " << name
							 << " - Port: " << serialPort
							 << " - UpLED:" << upLedId
							 << " - DownLED:" << downLedId
							 << " - Motor:" << motorId;
}

bool HourGlass::connect() {
	if (connected) {
		ofLogWarning("HourGlass") << name << " already connected";
		return true;
	}

	if (serialPortName.empty()) {
		ofLogError("HourGlass") << name << " - No serial port configured";
		return false;
	}

	// Get shared serial port
	auto & portManager = SerialPortManager::getInstance();
	sharedSerialPort = portManager.getPort(serialPortName, baudRate);

	if (!sharedSerialPort) {
		ofLogError("HourGlass") << name << " - Failed to get serial port: " << serialPortName;
		return false;
	}

	setupControllers();
	connected = true;

	ofLogNotice("HourGlass") << name << " connected to " << serialPortName;
	return true;
}

bool HourGlass::isConnected() const {
	return connected && sharedSerialPort && sharedSerialPort->isInitialized();
}

void HourGlass::disconnect() {
	if (connected) {
		// Controllers will automatically release the shared port
		if (upLedMagnet) upLedMagnet->disconnect();
		if (downLedMagnet) downLedMagnet->disconnect();
		if (motor) motor->disconnect();

		sharedSerialPort.reset();
		connected = false;

		ofLogNotice("HourGlass") << name << " disconnected";
	}
}

void HourGlass::setupControllers() {
	// Setup Up LED/Magnet Controller
	upLedMagnet = std::make_unique<LedMagnetController>(sharedSerialPort);
	upLedMagnet->setId(upLedId);

	// Setup Down LED/Magnet Controller
	downLedMagnet = std::make_unique<LedMagnetController>(sharedSerialPort);
	downLedMagnet->setId(downLedId);

	// Setup Motor Controller
	motor = std::make_unique<MotorController>(sharedSerialPort);
	motor->setId(motorId);
}

// Convenience methods
void HourGlass::enableMotor() {
	if (isConnected() && motor) {
		motor->enable();
		ofLogNotice("HourGlass") << name << " - Motor enabled";
	}
}

void HourGlass::disableMotor() {
	if (isConnected() && motor) {
		motor->disable();
		ofLogNotice("HourGlass") << name << " - Motor disabled";
	}
}

void HourGlass::emergencyStop() {
	if (isConnected() && motor) {
		motor->emergencyStop();
		ofLogNotice("HourGlass") << name << " - Emergency stop";
	}
}

void HourGlass::setAllLEDs(uint8_t r, uint8_t g, uint8_t b) {
	// Only update parameters, let OSCController::processLastCommands or UI direct calls handle sending
	// Throttling is removed here as direct sending is removed.
	// The responsibility for throttling and actual hardware send is now with:
	//  - UIWrapper listeners (for direct UI interaction)
	//  - OSCController::processLastCommands (for state synchronization from parameters)

	// bool wasUpdatingFromOSC = updatingFromOSC; // This flag is tricky here.
	// If OSC calls this, updatingFromOSC should be true when params are set.
	// If UI calls this (e.g. a preset button that uses setAllLEDs), it might be false.
	// For now, let the caller manage the updatingFromOSC flag if necessary before calling this.

	// It's crucial that if OSC is the origin of this call, 'updatingFromOSC' is true
	// BEFORE ofParameters are set, so listeners in UIWrapper can ignore these changes if needed.
	// OSCController::handleLedMessage for "/led/all/rgb" does set hg->updatingFromOSC = true;

	upLedColor.set(ofColor(r, g, b));
	downLedColor.set(ofColor(r, g, b));
	// ofLogNotice("HourGlass") << name << " - Parameters set by setAllLEDs: R" << (int)r << " G" << (int)g << " B" << (int)b;
}

void HourGlass::applyMotorParameters() {
	if (!isConnected() || !motor) return;

	// Apply persistent settings
	motor->setMicrostep(microstep.get()); // This is fine to call regularly

	if (motorEnabled.get()) {
		motor->enable(); // Idempotent or controller handles repeated calls
	} else {
		motor->disable(); // Idempotent
	}

	// Determine speed and acceleration to use for pending commands
	// Use pending values if provided, otherwise fallback to ofParameters
	int currentSpeed = pendingMoveSpeed.value_or(motorSpeed.get());
	int currentAccel = pendingMoveAccel.value_or(motorAcceleration.get());

	// Execute pending commands
	if (executeRelativeMove) {
		ofLogNotice("HourGlass::applyMotorParams") << getName() << " - Executing relative move: " << targetRelativeSteps;
		motor->moveRelative(currentSpeed, currentAccel, targetRelativeSteps);
		executeRelativeMove = false;
	}
	if (executeAbsoluteMove) {
		ofLogNotice("HourGlass::applyMotorParams") << getName() << " - Executing absolute move to: " << targetAbsolutePosition;
		motor->moveAbsolute(currentSpeed, currentAccel, targetAbsolutePosition);
		executeAbsoluteMove = false;
	}
	if (executeRelativeAngle) {
		ofLogNotice("HourGlass::applyMotorParams") << getName() << " - Executing relative angle: " << targetRelativeDegrees;
		motor->moveRelativeAngle(currentSpeed, currentAccel, targetRelativeDegrees, gearRatio.get(), calibrationFactor.get());
		executeRelativeAngle = false;
	}
	if (executeAbsoluteAngle) {
		ofLogNotice("HourGlass::applyMotorParams") << getName() << " - Executing absolute angle to: " << targetAbsoluteDegrees;
		motor->moveAbsoluteAngle(currentSpeed, currentAccel, targetAbsoluteDegrees, gearRatio.get(), calibrationFactor.get());
		executeAbsoluteAngle = false;
	}

	// Reset pending speed/accel after commands are processed for this frame
	pendingMoveSpeed = std::nullopt;
	pendingMoveAccel = std::nullopt;
}

void HourGlass::updateEffects(float deltaTime) {
	// ofLogVerbose("HourGlass::updateEffects") << getName() << " - deltaTime: " << deltaTime; // COMMENTED BACK
	upEffectsManager.update(deltaTime);
	downEffectsManager.update(deltaTime);
}

void HourGlass::addUpEffect(std::unique_ptr<Effect> effect) {
	if (effect) {
		upEffectsManager.addEffect(std::move(effect));
	}
}

void HourGlass::addDownEffect(std::unique_ptr<Effect> effect) {
	if (effect) {
		downEffectsManager.addEffect(std::move(effect));
	}
}

void HourGlass::clearUpEffects() {
	upEffectsManager.clearEffects();
}

void HourGlass::clearDownEffects() {
	downEffectsManager.clearEffects();
}

// Modified applyLedParameters
void HourGlass::applyLedParameters() {
	if (!connected || !upLedMagnet || !downLedMagnet) return;

	float currentTime = ofGetElapsedTimeMillis();
	if (currentTime - lastLedCommandSendTime < MIN_LED_COMMAND_INTERVAL_MS) {
		// ofLogVerbose("HourGlass::applyLedParameters") << getName() << " - Throttled"; // Keep this commented
		//return;
	}
	// ofLogNotice("HourGlass::applyLedParameters") << getName() << " - Applying LED parameters with effects."; // Keep this one for now, or comment if too noisy

	float dt = ofGetLastFrameTime();

	// --- UP LED Controller ---
	EffectParameters upParams;
	// ... (population of upParams) ...
	upParams.color = upLedColor.get();
	upParams.mainLedValue = upMainLed.get();
	upParams.blend = upLedBlend.get();
	upParams.origin = upLedOrigin.get();
	upParams.arc = upLedArc.get();
	upParams.effectLuminosityMultiplier = 1.0f;
	upParams.deltaTime = dt;

	// ofLogVerbose("HourGlass::applyLedParameters") << getName() << " - UP Before effects - Arc: " << upParams.arc << " LumM: " << upParams.effectLuminosityMultiplier; // COMMENTED BACK
	upEffectsManager.processEffects(upParams);
	// ofLogVerbose("HourGlass::applyLedParameters") << getName() << " - UP After effects - Arc: " << upParams.arc << " LumM: " << upParams.effectLuminosityMultiplier; // COMMENTED BACK

	float finalUpIndividualLuminosity = individualLuminosity.get() * upParams.effectLuminosityMultiplier;

	// UNIFIED COMMAND: Send all LED parameters in one consistent format
	upLedMagnet->sendAllLEDParameters(
		upParams.color.r, upParams.color.g, upParams.color.b,
		upParams.blend, upParams.origin, upParams.arc,
		static_cast<uint8_t>(upParams.mainLedValue),
		static_cast<uint8_t>(upPwm.get()),
		finalUpIndividualLuminosity);

	// --- DOWN LED Controller ---
	EffectParameters downParams;
	// ... (population of downParams) ...
	downParams.color = downLedColor.get();
	downParams.mainLedValue = downMainLed.get();
	downParams.blend = downLedBlend.get();
	downParams.origin = downLedOrigin.get();
	downParams.arc = downLedArc.get();
	downParams.effectLuminosityMultiplier = 1.0f;
	downParams.deltaTime = dt;

	// ofLogVerbose("HourGlass::applyLedParameters") << getName() << " - DOWN Before effects - Arc: " << downParams.arc << " LumM: " << downParams.effectLuminosityMultiplier; // COMMENTED BACK
	downEffectsManager.processEffects(downParams);
	// ofLogVerbose("HourGlass::applyLedParameters") << getName() << " - DOWN After effects - Arc: " << downParams.arc << " LumM: " << downParams.effectLuminosityMultiplier; // COMMENTED BACK

	float finalDownIndividualLuminosity = individualLuminosity.get() * downParams.effectLuminosityMultiplier;

	// UNIFIED COMMAND: Send all LED parameters in one consistent format
	downLedMagnet->sendAllLEDParameters(
		downParams.color.r, downParams.color.g, downParams.color.b,
		downParams.blend, downParams.origin, downParams.arc,
		static_cast<uint8_t>(downParams.mainLedValue),
		static_cast<uint8_t>(downPwm.get()),
		finalDownIndividualLuminosity);

	lastLedCommandSendTime = currentTime;
}

void HourGlass::commandRelativeMove(int steps, std::optional<int> speed, std::optional<int> accel) {
	targetRelativeSteps = steps;
	pendingMoveSpeed = speed;
	pendingMoveAccel = accel;
	executeRelativeMove = true;
	ofLogNotice("HourGlass::command") << getName() << " - Relative move commanded: " << steps << " steps.";
}

void HourGlass::commandAbsoluteMove(int position, std::optional<int> speed, std::optional<int> accel) {
	targetAbsolutePosition = position;
	pendingMoveSpeed = speed;
	pendingMoveAccel = accel;
	executeAbsoluteMove = true;
	ofLogNotice("HourGlass::command") << getName() << " - Absolute move commanded to: " << position;
}

void HourGlass::commandRelativeAngle(float degrees, std::optional<int> speed, std::optional<int> accel) {
	targetRelativeDegrees = degrees;
	pendingMoveSpeed = speed;
	pendingMoveAccel = accel;
	executeRelativeAngle = true;
	ofLogNotice("HourGlass::command") << getName() << " - Relative angle commanded: " << degrees << " deg.";
}

void HourGlass::commandAbsoluteAngle(float degrees, std::optional<int> speed, std::optional<int> accel) {
	targetAbsoluteDegrees = degrees;
	pendingMoveSpeed = speed;
	pendingMoveAccel = accel;
	executeAbsoluteAngle = true;
	ofLogNotice("HourGlass::command") << getName() << " - Absolute angle commanded to: " << degrees << " deg.";
}