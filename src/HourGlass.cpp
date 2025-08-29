#include "HourGlass.h"
#include "ofMain.h"
#include <algorithm> // For std::max
#include <optional>
#include <string>
#include <vector>

// Define the static const member for LED command interval (e.g., 50ms for 20 FPS)
const float HourGlass::MIN_LED_COMMAND_INTERVAL_MS = 50.0f;

// --- Helper function for text bounding box (user provided, adapted) ---
static ofRectangle customGetBitmapStringBoundingBox(const std::string & text) {
	std::vector<std::string> lines = ofSplitString(text, "\n");
	int maxLineLength = 0;
	for (size_t i = 0; i < lines.size(); i++) {
		const std::string & line(lines[i]);
		int currentLineLength = 0;
		for (size_t j = 0; j < line.size(); j++) {
			if (line[j] == '\t') {
				currentLineLength += 8 - (currentLineLength % 8);
			} else {
				currentLineLength++;
			}
		}
		maxLineLength = std::max(maxLineLength, currentLineLength);
	}

	int fontSize = 8; // Fixed assumption for OF default bitmap font
	float leading = 1.7f;
	int height = static_cast<int>(lines.size() * fontSize * leading - 1);
	int width = maxLineLength * fontSize;
	return ofRectangle(0, 0, width, height);
}
// --- End helper function ---

// Constructor matches HourGlass.h (name only)
HourGlass::HourGlass(const std::string & name)
	: name(name)
	, serialPortName("") // Initialize serialPortName
	, baudRate(0)
	, upLedId(0) // Default initialize IDs
	, downLedId(0)
	, motorId(0)
	, updatingFromOSC(false) // Initialized before 'connected' to match typical declaration order implied by warning
	, connected(false)
	, lastLedCommandSendTime(0.0f)
	, upEffectsManager()
	, downEffectsManager()
	, oscOutController(nullptr) {

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
}

bool HourGlass::connect() {
	if (connected) {
		ofLogWarning("HourGlass") << name << " already connected";
		return true;
	}

	// Create LED controllers even without serial connection for OSC operation
	if (!upLedMagnet || !downLedMagnet) {
		setupControllers();
	}

	if (serialPortName.empty()) {
		ofLogNotice("HourGlass") << name << " - No serial port configured, operating in OSC-only mode";
		connected = true; // Allow OSC operation
		return true;
	}

	ofLogWarning("HourGlass") << name << " - Serial port specified but serial is disabled, continuing in OSC-only mode";

	// Re-setup controllers without serial connection
	setupControllers();
	connected = true;

	return true;
}

bool HourGlass::isConnected() const {
	// Always return true for OSC testing - serial connection not required
	return true;
	// return connected && sharedSerialPort && sharedSerialPort->isInitialized();
}

void HourGlass::disconnect() {
	if (connected) {
		// Controllers will automatically release the shared port
		if (upLedMagnet) upLedMagnet->disconnect();
		if (downLedMagnet) downLedMagnet->disconnect();
		if (motor) motor->disconnect();

		connected = false;
	}
}

void HourGlass::setupControllers() {
	// Setup Up LED/Magnet Controller (OSC-only mode)
	upLedMagnet.reset(new LedMagnetController());
	upLedMagnet->setId(upLedId);

	// Setup Down LED/Magnet Controller (OSC-only mode)
	downLedMagnet.reset(new LedMagnetController());
	downLedMagnet->setId(downLedId);

	// Setup Motor Controller (OSC-only mode, no serial)
	motor.reset(new MotorController()); // No serial port needed
	motor->setId(motorId);
}

// OSC Out configuration methods
void HourGlass::setupOSCOut(const std::string & configPath) {
	if (!oscOutController) {
		oscOutController.reset(new OSCOutController());
	}

	if (!configPath.empty()) {
		oscOutController->loadConfiguration(configPath);
	} else {
		// Use default config with HourGlass name suffix
		std::string defaultPath = "osc_out_config_" + name + ".json";
		oscOutController->loadConfiguration(defaultPath);
	}

	oscOutController->setup();
}

void HourGlass::setupOSCOutFromJson(const ofJson & oscConfig) {
	if (!oscOutController) {
		oscOutController.reset(new OSCOutController());
	}

	oscOutController->loadConfigurationFromJson(oscConfig);
}

void HourGlass::enableOSCOut(bool enabled) {
	if (oscOutController) {
		oscOutController->setEnabled(enabled);
	}
}

bool HourGlass::isOSCOutEnabled() const {
	return oscOutController && oscOutController->isEnabled();
}

// Convenience methods
void HourGlass::enableMotor() {
	if (motor) {
		motor->enable();
	}
}

void HourGlass::disableMotor() {
	if (motor) {
		motor->disable();
	}
}

void HourGlass::emergencyStop() {
	if (motor) {
		motor->emergencyStop();
	}

	// Send OSC message if not updating from OSC (avoid feedback loops)
	if (isOSCOutEnabled() && !updatingFromOSC) {
		oscOutController->sendMotorEmergency(motorId);
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
	// Motor parameter processing requires actual motor controller
	bool hasMotorController = (motor != nullptr);

	// Apply persistent settings (only if motor controller available)
	if (hasMotorController) {
		motor->setMicrostep(microstep.get()); // This is fine to call regularly

		if (motorEnabled.get()) {
			motor->enable(); // Idempotent or controller handles repeated calls
		} else {
			motor->disable(); // Idempotent
		}
	}

	// Send OSC message for microstep if not updating from OSC
	if (isOSCOutEnabled() && !updatingFromOSC) {
		//oscOutController->sendMotorUstep(motorId, microstep.get());
	}

	// Determine speed and acceleration to use for pending commands
	// Use pending values if provided, otherwise fallback to ofParameters
	int currentSpeed = pendingMoveSpeed.value_or(motorSpeed.get());
	int currentAccel = pendingMoveAccel.value_or(motorAcceleration.get());

	// Execute pending commands
	if (executeRelativeMove) {

		// Send to motor controller if available
		if (hasMotorController) {
			motor->moveRelative(currentSpeed, currentAccel, targetRelativeSteps);
		}

		// Send OSC message if not updating from OSC (avoid feedback loops)
		if (isOSCOutEnabled() && !updatingFromOSC) {
			// Convert steps to degrees for OSC message (simplified conversion)
			float degrees = static_cast<float>(targetRelativeSteps) / (gearRatio.get() * calibrationFactor.get());
			oscOutController->sendMotorRelative(motorId, currentSpeed, currentAccel, degrees);
		}
		executeRelativeMove = false;
	}
	if (executeAbsoluteMove) {

		// Send to motor controller if available
		if (hasMotorController) {
			motor->moveAbsolute(currentSpeed, currentAccel, targetAbsolutePosition);
		}

		// Send OSC message if not updating from OSC (avoid feedback loops)
		if (isOSCOutEnabled() && !updatingFromOSC) {
			// Convert steps to degrees for OSC message (simplified conversion)
			float degrees = static_cast<float>(targetAbsolutePosition) / (gearRatio.get() * calibrationFactor.get());
			oscOutController->sendMotorAbsolute(motorId, currentSpeed, currentAccel, degrees);
		}
		executeAbsoluteMove = false;
	}
	if (executeRelativeAngle) {

		// Send to motor controller if available
		if (hasMotorController) {
			motor->moveRelativeAngle(currentSpeed, currentAccel, targetRelativeDegrees, gearRatio.get(), calibrationFactor.get());
		}

		// Send OSC message if not updating from OSC (avoid feedback loops)
		if (isOSCOutEnabled() && !updatingFromOSC) {
			oscOutController->sendMotorRelative(motorId, currentSpeed, currentAccel, targetRelativeDegrees);
		}
		executeRelativeAngle = false;
	}
	if (executeAbsoluteAngle) {

		// Send to motor controller if available
		if (hasMotorController) {
			motor->moveAbsoluteAngle(currentSpeed, currentAccel, targetAbsoluteDegrees, gearRatio.get(), calibrationFactor.get());
		}

		// Send OSC message if not updating from OSC (avoid feedback loops)
		if (isOSCOutEnabled() && !updatingFromOSC) {
			oscOutController->sendMotorAbsolute(motorId, currentSpeed, currentAccel, targetAbsoluteDegrees);
		}
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
	// Allow LED parameter processing even without serial connection
	bool hasLedControllers = (upLedMagnet && downLedMagnet);

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
	if (hasLedControllers) {
		upLedMagnet->sendAllLEDParameters(
			upParams.color.r, upParams.color.g, upParams.color.b,
			upParams.blend, upParams.origin, upParams.arc,
			static_cast<uint8_t>(upParams.mainLedValue),
			static_cast<uint8_t>(upPwm.get()),
			finalUpIndividualLuminosity);
	}

	// Send OSC messages for UP LED only when parameters change (avoid flooding)

	bool upParamsChanged = (upParams.color != lastUpColor || upParams.origin != lastUpOrigin || upParams.arc != lastUpArc || upPwm.get() != lastUpPwm || upParams.mainLedValue != lastUpMainLed || finalUpIndividualLuminosity != lastUpLuminosity);

	if (isOSCOutEnabled() && !updatingFromOSC && upParamsChanged) {
		// RGB LED control with alpha as master brightness
		uint8_t masterAlpha = static_cast<uint8_t>(finalUpIndividualLuminosity * 255.0f);
		oscOutController->sendRGBLED("top", upParams.color.r, upParams.color.g, upParams.color.b,
			masterAlpha, upParams.origin, upParams.arc);

		// Power LED control - Main LED slider controls power LED
		oscOutController->sendPowerLED("top", static_cast<uint8_t>(upParams.mainLedValue));

		// Electromagnet control - PWM slider controls magnet
		oscOutController->sendMagnet("top", static_cast<uint8_t>(upPwm.get()));

		// Update last values
		lastUpColor = upParams.color;
		lastUpOrigin = upParams.origin;
		lastUpArc = upParams.arc;
		lastUpPwm = upPwm.get();
		lastUpMainLed = upParams.mainLedValue;
		lastUpLuminosity = finalUpIndividualLuminosity;
	}

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
	if (hasLedControllers) {
		downLedMagnet->sendAllLEDParameters(
			downParams.color.r, downParams.color.g, downParams.color.b,
			downParams.blend, downParams.origin, downParams.arc,
			static_cast<uint8_t>(downParams.mainLedValue),
			static_cast<uint8_t>(downPwm.get()),
			finalDownIndividualLuminosity);
	}

	// Send OSC messages for DOWN LED if not updating from OSC (avoid feedback loops)

	bool downParamsChanged = (downParams.color != lastDownColor || downParams.origin != lastDownOrigin || downParams.arc != lastDownArc || downPwm.get() != lastDownPwm || downParams.mainLedValue != lastDownMainLed || finalDownIndividualLuminosity != lastDownLuminosity);

	if (isOSCOutEnabled() && !updatingFromOSC && downParamsChanged) {
		// RGB LED control with alpha as master brightness
		uint8_t masterAlpha = static_cast<uint8_t>(finalDownIndividualLuminosity * 255.0f);
		oscOutController->sendRGBLED("bot", downParams.color.r, downParams.color.g, downParams.color.b,
			masterAlpha, downParams.origin, downParams.arc);

		// Power LED control - Main LED slider controls power LED
		oscOutController->sendPowerLED("bot", static_cast<uint8_t>(downParams.mainLedValue));

		// Electromagnet control - PWM slider controls magnet
		oscOutController->sendMagnet("bot", static_cast<uint8_t>(downPwm.get()));

		// Update last values for DOWN LED
		lastDownColor = downParams.color;
		lastDownOrigin = downParams.origin;
		lastDownArc = downParams.arc;
		lastDownPwm = downPwm.get();
		lastDownMainLed = downParams.mainLedValue;
		lastDownLuminosity = finalDownIndividualLuminosity;
	}

	lastLedCommandSendTime = currentTime;
}

void HourGlass::commandRelativeMove(int steps, std::optional<int> speed, std::optional<int> accel) {
	targetRelativeSteps = steps;
	pendingMoveSpeed = speed;
	pendingMoveAccel = accel;
	executeRelativeMove = true;
}

void HourGlass::commandAbsoluteMove(int position, std::optional<int> speed, std::optional<int> accel) {
	targetAbsolutePosition = position;
	pendingMoveSpeed = speed;
	pendingMoveAccel = accel;
	executeAbsoluteMove = true;
}

void HourGlass::commandRelativeAngle(float degrees, std::optional<int> speed, std::optional<int> accel) {
	targetRelativeDegrees = degrees;
	pendingMoveSpeed = speed;
	pendingMoveAccel = accel;
	executeRelativeAngle = true;
}

void HourGlass::commandAbsoluteAngle(float degrees, std::optional<int> speed, std::optional<int> accel) {
	targetAbsoluteDegrees = degrees;
	pendingMoveSpeed = speed;
	pendingMoveAccel = accel;
	executeAbsoluteAngle = true;
}

void HourGlass::setMotorZero() {
	if (motor) {
		motor->setZero();
	}

	// Send OSC message if not updating from OSC (avoid feedback loops)
	if (isOSCOutEnabled() && !updatingFromOSC) {
		oscOutController->sendMotorZero(motorId);
	}
}

// --- Start of new/updated methods for enhanced minimal view ---

// Helper function to normalize angle to 0-360 range
float HourGlass::normalizeMinimalAngle(float angle) {
	while (angle < 0)
		angle += 360.0f;
	while (angle >= 360.0f)
		angle -= 360.0f;
	return angle;
}

// Helper function to check if an angle is within a specified arc
bool HourGlass::isMinimalAngleInArc(float currentAngleDegrees, int startAngleDegrees, int arcSpanDegrees) {
	currentAngleDegrees = normalizeMinimalAngle(currentAngleDegrees);
	startAngleDegrees = static_cast<int>(normalizeMinimalAngle(static_cast<float>(startAngleDegrees)));
	arcSpanDegrees = std::max(0, std::min(arcSpanDegrees, 360));

	if (arcSpanDegrees == 360) return true;
	if (arcSpanDegrees == 0) return false;

	int endAngleDegreesCalculated = static_cast<int>(normalizeMinimalAngle(static_cast<float>(startAngleDegrees + arcSpanDegrees)));

	if (startAngleDegrees <= endAngleDegreesCalculated) {
		return currentAngleDegrees >= startAngleDegrees && currentAngleDegrees <= endAngleDegreesCalculated;
	} else {
		return currentAngleDegrees >= startAngleDegrees || currentAngleDegrees <= endAngleDegreesCalculated;
	}
}

// Helper function to determine alpha for each LED circle based on blend value
float HourGlass::getMinimalCircleAlpha(int circleIndex, int blend) {
	float normalizedBlend = ofMap(blend, 0, 768, 0.0f, 1.0f, true);
	if (normalizedBlend <= 0.5f) {
		if (circleIndex == 0) return 1.0f - (normalizedBlend * 2.0f);
		if (circleIndex == 1) return normalizedBlend * 2.0f;
		return 0.0f;
	} else {
		if (circleIndex == 0) return 0.0f;
		if (circleIndex == 1) return 1.0f - ((normalizedBlend - 0.5f) * 2.0f);
		return (normalizedBlend - 0.5f) * 2.0f;
	}
}

// Draws a single LED controller (UP or DOWN) - V8 RIGOROUS BOUNDS
ofRectangle HourGlass::drawSingleLedControllerMinimal(float x, float y, const std::string & label,
	const ofColor & color, int blend, int origin, int arc,
	float globalLum, float individualLum, bool /*isUpController*/) {
	ofPushMatrix();
	ofTranslate(x, y); // (x,y) is TOP-LEFT for this module

	float currentModuleY = 0;
	float modulePadding = 3.0f;
	float controllerVisualDiameter = MINIMAL_CIRCLE_3_RADIUS * 2.0f;
	float labelTextHeight = 8.0f;
	float barHeight = 6.0f;
	float barSpacing = 2.5f;
	float paramLabelWidth = 10.0f;
	float barVisualMaxWidth = MINIMAL_CIRCLE_3_RADIUS * 1.4f;

	// --- 1. Label ---
	ofSetColor(210, 210, 230);
	std::string finalLabel = label;
	ofRectangle labelBox = customGetBitmapStringBoundingBox(finalLabel);
	float labelDrawX = (controllerVisualDiameter - labelBox.width) / 2.0f; // Center label over circle area
	if (labelDrawX < 0) labelDrawX = 0; // Prevent negative X if label is too wide
	ofDrawBitmapString(finalLabel, labelDrawX, currentModuleY + labelTextHeight);
	currentModuleY += labelTextHeight + modulePadding;

	// --- 2. LED Circles Visual ---
	float circlesDrawY = currentModuleY;
	float circlesCenterX = controllerVisualDiameter / 2.0f;
	float circlesCenterY = circlesDrawY + MINIMAL_CIRCLE_3_RADIUS;

	ofPushMatrix();
	ofTranslate(circlesCenterX, circlesCenterY);
	// ... (Circle drawing logic - largely unchanged from V7, ensure it draws centered at 0,0 here) ...
	const float radii[] = { MINIMAL_CIRCLE_1_RADIUS, MINIMAL_CIRCLE_2_RADIUS, MINIMAL_CIRCLE_3_RADIUS };
	const int numLedsPerCircle[] = { NUM_LEDS_CIRCLE_1, NUM_LEDS_CIRCLE_2, NUM_LEDS_CIRCLE_3 };
	float finalEffectiveBrightness = globalLum * individualLum;
	for (int circleIdx = 0; circleIdx < 3; ++circleIdx) {
		float circleAlpha = getMinimalCircleAlpha(circleIdx, blend);
		if (circleAlpha < 0.01f) continue;
		float currentRadius = radii[circleIdx];
		int numLeds = numLedsPerCircle[circleIdx];
		float finalLedAlpha = circleAlpha * finalEffectiveBrightness;
		ofSetColor(color.r, color.g, color.b, static_cast<int>(finalLedAlpha * 40));
		ofNoFill();
		ofDrawCircle(0, 0, currentRadius);
		ofFill();
		for (int i = 0; i < numLeds; ++i) {
			float ledAngleDeg = ofMap(i, 0, numLeds, 0, 360);
			if (isMinimalAngleInArc(ledAngleDeg, origin, arc)) {
				float angleRad = ofDegToRad(ledAngleDeg);
				float ledX = cos(angleRad) * currentRadius;
				float ledY = sin(angleRad) * currentRadius;
				ofSetColor(color.r, color.g, color.b, static_cast<int>(finalLedAlpha * 255));
				ofDrawCircle(ledX, ledY, 2.0f);
			}
		}
	}
	float originMarkerVisRadius = MINIMAL_CIRCLE_3_RADIUS;
	float originAngleRadVis = ofDegToRad(static_cast<float>(origin) - 90.0f);
	ofSetColor(255, 255, 0, 200);
	ofDrawLine(cos(originAngleRadVis) * (originMarkerVisRadius - 4), sin(originAngleRadVis) * (originMarkerVisRadius - 4),
		cos(originAngleRadVis) * (originMarkerVisRadius + 4), sin(originAngleRadVis) * (originMarkerVisRadius + 4));
	ofPath arcPathVis;
	arcPathVis.setCircleResolution(50);
	arcPathVis.setFilled(false);
	arcPathVis.setStrokeWidth(2.5f);
	arcPathVis.setStrokeColor(ofColor(color, static_cast<int>(220 * finalEffectiveBrightness)));
	float ofPathOriginVis = static_cast<float>(origin);
	float ofPathArcVis = (arc == 0 && origin == 0) ? 359.9f : static_cast<float>(arc);
	if (abs(arc) >= 360) ofPathArcVis = 359.9f;
	arcPathVis.arc(0, 0, originMarkerVisRadius, originMarkerVisRadius, ofPathOriginVis, ofPathOriginVis + ofPathArcVis);
	arcPathVis.draw();
	ofSetColor(120, 120, 140);
	ofDrawCircle(0, 0, 1.5f);
	ofPopMatrix(); // Pop from circles center translation
	currentModuleY = circlesDrawY + controllerVisualDiameter + modulePadding;

	// --- 3. Parameter Bars ---
	float barBlockDrawX = (controllerVisualDiameter - barVisualMaxWidth) / 2.0f;
	if (barBlockDrawX < 0) barBlockDrawX = 0; // Ensure bars don't go to negative X relative to module
	float barLabelDrawX = barBlockDrawX - paramLabelWidth - 1.0f;
	if (barLabelDrawX < 0) barLabelDrawX = 0; // Ensure labels don't go to negative X either

	// Blend Bar
	ofSetColor(210, 210, 230, 190);
	ofDrawBitmapString("B", barLabelDrawX, currentModuleY + barHeight - 1);
	ofSetColor(70, 70, 100, 190);
	ofDrawRectangle(barBlockDrawX, currentModuleY, barVisualMaxWidth, barHeight);
	ofSetColor(180, 180, 220, 210);
	float blendFill = ofMap(blend, 0, 768, 0, barVisualMaxWidth, true);
	ofDrawRectangle(barBlockDrawX, currentModuleY, blendFill, barHeight);
	currentModuleY += barHeight + barSpacing;

	// Global Luminosity Bar (similar drawing for G and I)
	ofSetColor(210, 210, 230, 190);
	ofDrawBitmapString("G", barLabelDrawX, currentModuleY + barHeight - 1);
	ofSetColor(70, 70, 100, 190);
	ofDrawRectangle(barBlockDrawX, currentModuleY, barVisualMaxWidth, barHeight);
	ofSetColor(150, 220, 150, 210);
	float gLumFill = ofMap(globalLum, 0.0f, 1.0f, 0, barVisualMaxWidth, true);
	ofDrawRectangle(barBlockDrawX, currentModuleY, gLumFill, barHeight);
	currentModuleY += barHeight + barSpacing;

	ofSetColor(210, 210, 230, 190);
	ofDrawBitmapString("I", barLabelDrawX, currentModuleY + barHeight - 1);
	ofSetColor(70, 70, 100, 190);
	ofDrawRectangle(barBlockDrawX, currentModuleY, barVisualMaxWidth, barHeight);
	ofSetColor(150, 150, 220, 210);
	float iLumFill = ofMap(individualLum, 0.0f, 1.0f, 0, barVisualMaxWidth, true);
	ofDrawRectangle(barBlockDrawX, currentModuleY, iLumFill, barHeight);
	currentModuleY += barHeight; // Last bar, no extra spacing needed from currentModuleY perspective for height calculation

	float totalModuleWidth = std::max(controllerVisualDiameter, paramLabelWidth + barVisualMaxWidth + 2 * modulePadding);
	ofPopMatrix();
	return ofRectangle(x, y, totalModuleWidth, currentModuleY);
}

// Updated drawMinimal method for HourGlass - V8 RIGOROUS BOUNDS
void HourGlass::drawMinimal(float x, float y) {
	ofPushMatrix();
	ofTranslate(x, y); // x,y is top-left of this HG's box

	float padding = 8.0f; // General padding for inside the box
	float headerLineHeight = 12.0f;
	float currentY = padding;
	float currentX = padding;
	float overallMaxX = padding; // To track the widest point for box width

	// --- Header: Name and Status Dot ---
	ofSetColor(220, 220, 240);
	ofRectangle nameBounds = customGetBitmapStringBoundingBox(name);
	ofDrawBitmapString(name, currentX, currentY + headerLineHeight);
	float nameEndPostionX = currentX + nameBounds.width;

	float statusDotRadius = 3.5f;
	float statusDotX = nameEndPostionX + padding + statusDotRadius;
	if (isConnected()) {
		ofSetColor(80, 230, 80, 220);
	} else {
		ofSetColor(230, 80, 80, 220);
	}
	ofDrawCircle(statusDotX, currentY + headerLineHeight * 0.5f + statusDotRadius * 0.5f, statusDotRadius);
	overallMaxX = std::max(overallMaxX, statusDotX + statusDotRadius + padding);
	currentY += headerLineHeight + padding;

	// --- LED Controllers Row ---
	float globalSystemLuminosity = LedMagnetController::getGlobalLuminosity();
	currentX = padding; // Reset X for the controller row
	float controllerRowMaxHeight = 0;

	ofRectangle upCtrlRect = drawSingleLedControllerMinimal(currentX, currentY, "UP",
		upLedColor.get(), upLedBlend.get(), upLedOrigin.get(), upLedArc.get(),
		globalSystemLuminosity, individualLuminosity.get(), true);
	currentX += upCtrlRect.width + padding * 1.5f; // Space between controllers
	controllerRowMaxHeight = std::max(controllerRowMaxHeight, upCtrlRect.height);
	overallMaxX = std::max(overallMaxX, currentX);

	ofRectangle downCtrlRect = drawSingleLedControllerMinimal(currentX, currentY, "DOWN",
		downLedColor.get(), downLedBlend.get(), downLedOrigin.get(), downLedArc.get(),
		globalSystemLuminosity, individualLuminosity.get(), false);
	currentX += downCtrlRect.width;
	controllerRowMaxHeight = std::max(controllerRowMaxHeight, downCtrlRect.height);
	overallMaxX = std::max(overallMaxX, currentX + padding);
	currentY += controllerRowMaxHeight + padding;

	// --- Motor Status Row ---
	float motorTextY = currentY + headerLineHeight * 0.5f;
	currentX = padding;
	float motorIconX = currentX + 5.0f;
	float motorTextX = motorIconX + 10.0f;
	float motorLineHeight = headerLineHeight; // Use header line height for consistency

	if (motorEnabled.get()) {
		ofSetColor(80, 200, 80, 200);
		ofDrawRectangle(motorIconX, motorTextY, 6, 6);
		ofSetColor(210, 210, 230, 200);
		std::string motorOnText = "ON " + ofToString(motorSpeed.get());
		ofDrawBitmapString(motorOnText, motorTextX, motorTextY + motorLineHeight * 0.5f);
		overallMaxX = std::max(overallMaxX, motorTextX + customGetBitmapStringBoundingBox(motorOnText).width + padding);
	} else {
		ofSetColor(200, 80, 80, 200);
		ofDrawRectangle(motorIconX, motorTextY, 6, 6);
		ofSetColor(210, 210, 230, 200);
		std::string motorOffText = "OFF";
		ofDrawBitmapString(motorOffText, motorTextX, motorTextY + motorLineHeight * 0.5f);
		overallMaxX = std::max(overallMaxX, motorTextX + customGetBitmapStringBoundingBox(motorOffText).width + padding);
	}
	currentY += motorLineHeight + padding;

	// --- Bounding box ---
	float finalBoxWidth = overallMaxX;
	float finalBoxHeight = currentY;
	ofNoFill();
	ofSetColor(60, 65, 80, 190);
	ofDrawRectRounded(0, 0, finalBoxWidth, finalBoxHeight, 5);
	ofFill();
	ofPopMatrix();
}

// --- End of new/updated methods ---