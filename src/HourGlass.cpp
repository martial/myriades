#include "HourGlass.h"
#include "LedGeometry.h"
#include "ofMain.h"
#include <algorithm> // For std::max
#include <optional>
#include <string>
#include <vector>

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
	, updatingFromOSC(false)
	, connected(false)
	, upEffectsManager()
	, downEffectsManager()
	, oscOutController(nullptr) {
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

void HourGlass::refreshLedState() {
	if (upLedMagnet) upLedMagnet->resetLastSentValues();
	if (downLedMagnet) downLedMagnet->resetLastSentValues();
}

void HourGlass::setAllLEDs(uint8_t r, uint8_t g, uint8_t b) {
	// Only updates parameters; the actual send happens in applyLedParameters()
	// on the next frame. If OSC is the origin of this call, 'updatingFromOSC'
	// must be true BEFORE the parameters are set so UI listeners can ignore it.
	upLedColor.set(ofColor(r, g, b));
	downLedColor.set(ofColor(r, g, b));
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

	// Determine speed and acceleration to use for pending commands
	// Use pending values if provided, otherwise fallback to ofParameters
	int currentSpeed = pendingMoveSpeed.value_or(motorSpeed.get());
	int currentAccel = pendingMoveAccel.value_or(motorAcceleration.get());

	// Execute pending commands
	if (executeRelativeMove) {
		if (hasMotorController) {
			motor->moveRelative(currentSpeed, currentAccel, targetRelativeSteps);
		}

		if (isOSCOutEnabled() && !updatingFromOSC) {
			// Mirror steps as degrees using the canonical conversion so the OSC
			// message matches what an angle command for the same motion would say
			float degrees = MotorController::axisToDegrees(targetRelativeSteps, gearRatio.get(), calibrationFactor.get());
			oscOutController->sendMotorRelative(motorId, currentSpeed, currentAccel, degrees);
		}
		executeRelativeMove = false;
	}
	if (executeAbsoluteMove) {
		if (hasMotorController) {
			motor->moveAbsolute(currentSpeed, currentAccel, targetAbsolutePosition);
		}

		if (isOSCOutEnabled() && !updatingFromOSC) {
			float degrees = MotorController::axisToDegrees(targetAbsolutePosition, gearRatio.get(), calibrationFactor.get());
			oscOutController->sendMotorAbsolute(motorId, currentSpeed, currentAccel, degrees);
		}
		executeAbsoluteMove = false;
	}
	if (executeRelativeAngle) {
		if (hasMotorController) {
			motor->moveRelativeAngle(currentSpeed, currentAccel, targetRelativeDegrees, gearRatio.get(), calibrationFactor.get());
		}

		if (isOSCOutEnabled() && !updatingFromOSC) {
			oscOutController->sendMotorRelative(motorId, currentSpeed, currentAccel, targetRelativeDegrees);
		}
		executeRelativeAngle = false;
	}
	if (executeAbsoluteAngle) {
		if (hasMotorController) {
			motor->moveAbsoluteAngle(currentSpeed, currentAccel, targetAbsoluteDegrees, gearRatio.get(), calibrationFactor.get());
		}

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

// Shared UP/DOWN pipeline: effects -> controller -> change-tracked OSC out
void HourGlass::applyLedSide(LedMagnetController * controller, EffectsManager & effectsManager,
	const char * oscPosition,
	const ofParameter<ofColor> & colorParam, const ofParameter<int> & mainLedParam,
	const ofParameter<int> & blendParam, const ofParameter<int> & originParam,
	const ofParameter<int> & arcParam, const ofParameter<int> & pwmParam,
	LedSideState & lastSent, float dt) {

	EffectParameters params;
	params.color = colorParam.get();
	params.mainLedValue = mainLedParam.get();
	params.blend = blendParam.get();
	params.origin = originParam.get();
	params.arc = arcParam.get();
	params.effectLuminosityMultiplier = 1.0f;
	params.deltaTime = dt;

	effectsManager.processEffects(params);

	float finalIndividualLuminosity = individualLuminosity.get() * params.effectLuminosityMultiplier;

	// UNIFIED COMMAND: Send all LED parameters in one consistent format
	if (controller) {
		controller->sendAllLEDParameters(
			params.color.r, params.color.g, params.color.b,
			params.blend, params.origin, params.arc,
			static_cast<uint8_t>(params.mainLedValue),
			static_cast<uint8_t>(pwmParam.get()),
			finalIndividualLuminosity);
	}

	// Send OSC messages - only what actually changed
	if (isOSCOutEnabled() && !updatingFromOSC) {
		bool rgbChanged = (params.color != lastSent.color || params.origin != lastSent.origin || params.arc != lastSent.arc || finalIndividualLuminosity != lastSent.luminosity);
		bool mainLedChanged = (params.mainLedValue != lastSent.mainLed);
		bool pwmChanged = (pwmParam.get() != lastSent.pwm);

		if (rgbChanged) {
			uint8_t masterAlpha = static_cast<uint8_t>(finalIndividualLuminosity * 255.0f);
			oscOutController->sendRGBLED(oscPosition, params.color.r, params.color.g, params.color.b,
				masterAlpha, params.origin, params.arc);

			lastSent.color = params.color;
			lastSent.origin = params.origin;
			lastSent.arc = params.arc;
			lastSent.luminosity = finalIndividualLuminosity;
		}

		if (mainLedChanged) {
			oscOutController->sendPowerLED(oscPosition, static_cast<uint8_t>(params.mainLedValue));
			lastSent.mainLed = params.mainLedValue;
		}

		if (pwmChanged) {
			oscOutController->sendMagnet(oscPosition, static_cast<uint8_t>(pwmParam.get()));
			lastSent.pwm = pwmParam.get();
		}
	}
}

void HourGlass::applyLedParameters() {
	float dt = ofGetLastFrameTime();

	applyLedSide(upLedMagnet.get(), upEffectsManager, "top",
		upLedColor, upMainLed, upLedBlend, upLedOrigin, upLedArc, upPwm,
		lastUpSent, dt);

	applyLedSide(downLedMagnet.get(), downEffectsManager, "bot",
		downLedColor, downMainLed, downLedBlend, downLedOrigin, downLedArc, downPwm,
		lastDownSent, dt);
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

// --- Minimal view drawing ---

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
	const float radii[] = { MINIMAL_CIRCLE_1_RADIUS, MINIMAL_CIRCLE_2_RADIUS, MINIMAL_CIRCLE_3_RADIUS };
	const int numLedsPerCircle[] = { LedGeometry::NUM_LEDS_CIRCLE_1, LedGeometry::NUM_LEDS_CIRCLE_2, LedGeometry::NUM_LEDS_CIRCLE_3 };
	float finalEffectiveBrightness = globalLum * individualLum;
	for (int circleIdx = 0; circleIdx < 3; ++circleIdx) {
		float circleAlpha = LedGeometry::circleAlphaForBlend(circleIdx, blend);
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
			if (LedGeometry::isAngleInArc(ledAngleDeg, origin, arc)) {
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
