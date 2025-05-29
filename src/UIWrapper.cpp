#include "UIWrapper.h"
#include "OSCController.h"
#include "SerialPortManager.h"

// OSC activity tracking constants
const float UIWrapper::OSC_ACTIVITY_FADE_TIME = 1.5f; // Dot visible for 1.5 seconds

UIWrapper::UIWrapper()
	: hourglassManager(nullptr)
	, oscControllerInstance(nullptr)
	, currentHourGlass(0)
	, lastColorUpdateTime(0)
	, isUpdatingFromEffects(false)
	, lastOSCMessageTime(0) {
	lastUpColor = ofColor::black;
	lastDownColor = ofColor::black;
}

UIWrapper::~UIWrapper() {
	saveSettings();
}

void UIWrapper::setup(HourGlassManager * manager, OSCController * oscCtrl) {
	this->hourglassManager = manager;
	this->oscControllerInstance = oscCtrl;

	currentHourGlass = 0;

	// Setup all panels
	setupPanels();

	// Setup all listeners
	setupListeners();

	// Sync selector with current selection
	hourglassSelectorParam = currentHourGlass + 1;

	// Load saved settings and configurations
	loadSettings();

	ofLogNotice("UIWrapper") << "Setup complete with " << hourglassManager->getHourGlassCount() << " hourglasses";
}

void UIWrapper::update() {
	// Update OSC parameter sync (disabled)
	// oscSync.update();

	// Update serial statistics
	SerialPortManager::getInstance().updateStats();

	// Debug: Check parameter values
	static int debugFrameCount = 0;
	debugFrameCount++;

	// Remove the debug logging section completely
}

void UIWrapper::draw() {
	// Draw all horizontal panels
	settingsPanel.draw();
	motorPanel.draw();
	ledUpPanel.draw();
	ledDownPanel.draw();
	luminosityPanel.draw();
	// globalSettingsPanel.draw(); // Removed: Will be part of settingsPanel

	// Draw enhanced status panel
	drawStatus();
}

void UIWrapper::setupPanels() {
	// Calculate panel dimensions for 5 horizontal panels - made even more compact
	int panelWidth = 225;
	int panelHeight = 300; // Reduced from 350 to 300 for even more status space
	int panelSpacing = 15;
	int startX = 15;
	int startY = 20;

	// === PANEL 1: SETTINGS & ACTIONS (Left) ===
	settingsPanel.setup("SETTINGS & ACTIONS", "settings_actions.xml", startX, startY);
	settingsPanel.setDefaultBackgroundColor(ofColor(20, 20, 20, 180));
	settingsPanel.setDefaultFillColor(ofColor(60, 60, 60, 160));
	settingsPanel.setDefaultHeaderBackgroundColor(ofColor(40, 40, 40, 200));
	settingsPanel.setDefaultTextColor(ofColor(255, 255, 255));

	// --- Settings Section ---
	hourglassSelectorParam.set("Select HourGlass", 1, 1, hourglassManager->getHourGlassCount() > 0 ? hourglassManager->getHourGlassCount() : 2);
	connectBtnParam.set("Connect All");
	disconnectBtnParam.set("Disconnect All");

	settingsPanel.add(hourglassSelectorParam);
	connectBtn.setup(connectBtnParam.getName());
	disconnectBtn.setup(disconnectBtnParam.getName());
	settingsPanel.add(&connectBtn);
	settingsPanel.add(&disconnectBtn);

	// Global Luminosity (affects ALL modules)
	globalLuminosityParam.set("Global Luminosity", LedMagnetController::getGlobalLuminosity(), 0.0f, 1.0f);
	globalLuminositySlider.setup(globalLuminosityParam);
	settingsPanel.add(&globalLuminositySlider);

	// --- Actions Section ---
	allOffBtnParam.set("ALL OFF");
	ledsOffBtnParam.set("LEDs Off");

	allOffBtn.setup(allOffBtnParam.getName());
	ledsOffBtn.setup(ledsOffBtnParam.getName());
	settingsPanel.add(&allOffBtn);
	settingsPanel.add(&ledsOffBtn);

	// === PANEL 2: MOTOR (Center-Left) ===
	motorPanel.setup("MOTOR", "motor.xml", startX + (panelWidth + panelSpacing) * 1, startY);
	motorPanel.setDefaultBackgroundColor(ofColor(20, 20, 20, 180));
	motorPanel.setDefaultFillColor(ofColor(60, 60, 60, 160));
	motorPanel.setDefaultHeaderBackgroundColor(ofColor(40, 40, 40, 200));
	motorPanel.setDefaultTextColor(ofColor(255, 255, 255));

	emergencyStopBtnParam.set("Emergency Stop");
	setZeroBtnParam.set("Set Zero");
	relativePositionParam.set("Relative Pos", 0, -10000, 10000);
	absolutePositionParam.set("Absolute Pos", 0, -10000, 10000);
	moveRelativeBtnParam.set("Move Relative");
	moveAbsoluteBtnParam.set("Move Absolute");

	// Add angle-based parameters
	relativeAngleParam.set("Relative Angle (°)", 0, -36000, 36000);
	absoluteAngleParam.set("Absolute Angle (°)", 0, -36000, 36000);
	moveRelativeAngleBtnParam.set("Move Relative Angle");
	moveAbsoluteAngleBtnParam.set("Move Absolute Angle");

	// Add shared parameters from first hourglass
	if (hourglassManager->getHourGlassCount() > 0) {
		auto * hg = hourglassManager->getHourGlass(0);
		if (hg) {
			motorPanel.add(hg->motorEnabled);
			motorPanel.add(hg->microstep);
			motorPanel.add(hg->motorSpeed);
			motorPanel.add(hg->motorAcceleration);

			gearRatioInput.setup(hg->gearRatio);
			calibrationFactorInput.setup(hg->calibrationFactor);
			motorPanel.add(&gearRatioInput);
			motorPanel.add(&calibrationFactorInput);
		}
	}

	motorPanel.add(relativePositionParam);
	motorPanel.add(absolutePositionParam);

	relativeAngleInput.setup(relativeAngleParam);
	absoluteAngleInput.setup(absoluteAngleParam);
	motorPanel.add(&relativeAngleInput);
	motorPanel.add(&absoluteAngleInput);

	emergencyStopBtn.setup(emergencyStopBtnParam.getName());
	setZeroBtn.setup(setZeroBtnParam.getName());
	moveRelativeBtn.setup(moveRelativeBtnParam.getName());
	moveAbsoluteBtn.setup(moveAbsoluteBtnParam.getName());
	moveRelativeAngleBtn.setup(moveRelativeAngleBtnParam.getName());
	moveAbsoluteAngleBtn.setup(moveAbsoluteAngleBtnParam.getName());
	motorPanel.add(&emergencyStopBtn);
	motorPanel.add(&setZeroBtn);
	motorPanel.add(&moveRelativeBtn);
	motorPanel.add(&moveAbsoluteBtn);
	motorPanel.add(&moveRelativeAngleBtn);
	motorPanel.add(&moveAbsoluteAngleBtn);

	// === PANEL 3: UP LED CONTROLLER (Center) ===
	ledUpPanel.setup("UP LED CONTROLLER", "led_up.xml", startX + (panelWidth + panelSpacing) * 2, startY);
	ledUpPanel.setDefaultBackgroundColor(ofColor(20, 20, 20, 180));
	ledUpPanel.setDefaultFillColor(ofColor(60, 60, 60, 160));
	ledUpPanel.setDefaultHeaderBackgroundColor(ofColor(40, 40, 40, 200));
	ledUpPanel.setDefaultTextColor(ofColor(255, 255, 255));

	// Up LED Controller - RGB + Main LED + PWM
	if (hourglassManager->getHourGlassCount() > 0) {
		auto * hg = hourglassManager->getHourGlass(0);
		if (hg) {
			ledUpPanel.add(hg->upLedColor.set("RGB Color", ofColor::black));
			ledUpPanel.add(hg->upMainLed.set("Main LED", 0, 0, 255));
			ledUpPanel.add(hg->upPwm.set("PWM", 0, 0, 255));
		}
	}

	// Up LED Effect Parameters
	upLedBlendParam.set("Blend", 0, 0, 768);
	upLedOriginParam.set("Origin (°)", 0, 0, 360);
	upLedArcParam.set("Arc (°)", 360, 0, 360);
	ledUpPanel.add(upLedBlendParam);
	ledUpPanel.add(upLedOriginParam);
	ledUpPanel.add(upLedArcParam);

	// Expand color picker by default
	ledUpPanel.getGroup("RGB Color").maximize();

	// === PANEL 4: DOWN LED CONTROLLER (Center-Right) ===
	ledDownPanel.setup("DOWN LED CONTROLLER", "led_down.xml", startX + (panelWidth + panelSpacing) * 3, startY);
	ledDownPanel.setDefaultBackgroundColor(ofColor(20, 20, 20, 180));
	ledDownPanel.setDefaultFillColor(ofColor(60, 60, 60, 160));
	ledDownPanel.setDefaultHeaderBackgroundColor(ofColor(40, 40, 40, 200));
	ledDownPanel.setDefaultTextColor(ofColor(255, 255, 255));

	// Down LED Controller - RGB + Main LED + PWM
	if (hourglassManager->getHourGlassCount() > 0) {
		auto * hg = hourglassManager->getHourGlass(0);
		if (hg) {
			ledDownPanel.add(hg->downLedColor.set("RGB Color", ofColor::black));
			ledDownPanel.add(hg->downMainLed.set("Main LED", 0, 0, 255));
			ledDownPanel.add(hg->downPwm.set("PWM", 0, 0, 255));
		}
	}

	// Down LED Effect Parameters
	downLedBlendParam.set("Blend", 0, 0, 768);
	downLedOriginParam.set("Origin (°)", 0, 0, 360);
	downLedArcParam.set("Arc (°)", 360, 0, 360);
	ledDownPanel.add(downLedBlendParam);
	ledDownPanel.add(downLedOriginParam);
	ledDownPanel.add(downLedArcParam);

	// Expand color picker by default
	ledDownPanel.getGroup("RGB Color").maximize();

	// === PANEL 5: MODULE LUMINOSITY (Right) ===
	luminosityPanel.setup("MODULE LUMINOSITY", "luminosity.xml", startX + (panelWidth + panelSpacing) * 4, startY);
	luminosityPanel.setDefaultBackgroundColor(ofColor(20, 20, 20, 180));
	luminosityPanel.setDefaultFillColor(ofColor(60, 60, 60, 160));
	luminosityPanel.setDefaultHeaderBackgroundColor(ofColor(40, 40, 40, 200));
	luminosityPanel.setDefaultTextColor(ofColor(255, 255, 255));

	// Sync Controls for LED Controllers
	syncColorsParam.set("Sync Controllers", false);
	luminosityPanel.add(syncColorsParam);

	// Individual Luminosity for current module (shared between Up/Down controllers)
	currentHgIndividualLuminosityParam.set("Module Individual Luminosity", 1.0f, 0.0f, 1.0f);
	if (hourglassManager->getHourGlassCount() > 0) {
		auto * hg = hourglassManager->getHourGlass(0);
		if (hg) {
			currentHgIndividualLuminosityParam.set(hg->individualLuminosity.get());
		}
	}
	currentHgIndividualLuminositySlider.setup(currentHgIndividualLuminosityParam);
	luminosityPanel.add(&currentHgIndividualLuminositySlider);
}

void UIWrapper::setupListeners() {
	// Settings listeners
	hourglassSelectorParam.addListener(this, &UIWrapper::hourglassSelectorChanged);
	connectBtn.addListener(this, &UIWrapper::onConnectPressed);
	disconnectBtn.addListener(this, &UIWrapper::onDisconnectPressed);

	// Motor listeners
	emergencyStopBtn.addListener(this, &UIWrapper::onEmergencyStopPressed);
	setZeroBtn.addListener(this, &UIWrapper::onSetZeroPressed);
	moveRelativeBtn.addListener(this, &UIWrapper::onMoveRelativePressed);
	moveAbsoluteBtn.addListener(this, &UIWrapper::onMoveAbsolutePressed);
	moveRelativeAngleBtn.addListener(this, &UIWrapper::onMoveRelativeAnglePressed);
	moveAbsoluteAngleBtn.addListener(this, &UIWrapper::onMoveAbsoluteAnglePressed);

	// LED listeners
	syncColorsParam.addListener(this, &UIWrapper::syncColorsChanged);

	// Add listener for individual luminosity
	currentHgIndividualLuminosityParam.addListener(this, &UIWrapper::onIndividualLuminosityChanged);

	// Add listeners for new LED effect parameters
	upLedBlendParam.addListener(this, &UIWrapper::onUpLedBlendChanged);
	upLedOriginParam.addListener(this, &UIWrapper::onUpLedOriginChanged);
	upLedArcParam.addListener(this, &UIWrapper::onUpLedArcChanged);
	downLedBlendParam.addListener(this, &UIWrapper::onDownLedBlendChanged);
	downLedOriginParam.addListener(this, &UIWrapper::onDownLedOriginChanged);
	downLedArcParam.addListener(this, &UIWrapper::onDownLedArcChanged);

	// Setup OSC parameter sync for the first hourglass
	if (hourglassManager->getHourGlassCount() > 0) {
		auto * hg = hourglassManager->getHourGlass(0);
		if (hg) {
			// Disable ofxOscParameterSync for now, use OSCController for everything
			// oscSync.setup(hg->getParameterGroup(), 9000, "localhost", 9001);
			ofLogNotice("UIWrapper") << "Using OSCController only - all OSC on port 8000";

			// Log simple OSC addresses for RGB control
			ofLogNotice("OSC Addresses") << "RGB Color control addresses (port 8000):";
			ofLogNotice("OSC Addresses") << "  /rgb [r] [g] [b]           - Set both LEDs";
			ofLogNotice("OSC Addresses") << "  /up/rgb [r] [g] [b]        - Set upper LED";
			ofLogNotice("OSC Addresses") << "  /down/rgb [r] [g] [b]      - Set lower LED";

			// Add listeners for motor parameters to apply changes to hardware
			hg->motorEnabled.addListener(this, &UIWrapper::onMotorEnabledChanged);
			hg->microstep.addListener(this, &UIWrapper::onMicrostepChanged);
			hg->motorSpeed.addListener(this, &UIWrapper::onMotorSpeedChanged);
			hg->motorAcceleration.addListener(this, &UIWrapper::onMotorAccelerationChanged);
			hg->gearRatio.addListener(this, &UIWrapper::onGearRatioChanged);
			hg->calibrationFactor.addListener(this, &UIWrapper::onCalibrationFactorChanged);

			// Add listeners for LED parameters to apply changes to hardware
			hg->upLedColor.addListener(this, &UIWrapper::onUpLedColorChanged);
			hg->downLedColor.addListener(this, &UIWrapper::onDownLedColorChanged);
			hg->upMainLed.addListener(this, &UIWrapper::onUpMainLedChanged);
			hg->downMainLed.addListener(this, &UIWrapper::onDownMainLedChanged);
			hg->upPwm.addListener(this, &UIWrapper::onUpPwmChanged);
			hg->downPwm.addListener(this, &UIWrapper::onDownPwmChanged);
		}
	}

	// Quick action listeners
	allOffBtn.addListener(this, &UIWrapper::onAllOffPressed);
	ledsOffBtn.addListener(this, &UIWrapper::onLedsOffPressed);

	// Global Luminosity Listener
	globalLuminosityParam.addListener(this, &UIWrapper::onGlobalLuminosityChanged);
}

void UIWrapper::drawStatus() {
	// Position lower on screen to use more bottom space
	int panelWidth = 225;
	int panelSpacing = 15;
	int startX = 15;
	int statusY = 420; // Moved down from 390 to use more bottom space
	int statusWidth = (panelWidth * 5) + (panelSpacing * 4); // Span all 5 panels

	// Split into two sections: left for HG info, right for serial stats
	int leftSectionWidth = statusWidth * 0.65; // 65% for HG info
	int rightSectionWidth = statusWidth * 0.35; // 35% for serial stats

	int textX = startX + 15;
	int rightTextX = startX + leftSectionWidth + 20;
	int textY = statusY + 25;
	int rightTextY = statusY + 25;
	int lineHeight = 18; // Slightly reduced for more compact layout

	// Calculate content height dynamically
	int leftContentLines = 10; // HG status (5) + keyboard shortcuts (5) - increased to fit content
	int rightContentLines = 9; // Serial stats (8) + OSC activity (1) - increased to fit content
	int maxContentLines = std::max(leftContentLines, rightContentLines);
	int statusHeight = 35 + (maxContentLines * lineHeight * 2); // Increased padding from 25 to 35

	// Draw panel background - sized to fit content
	ofSetColor(40, 40, 45, 180); // Dark semi-transparent
	ofDrawRectRounded(startX, statusY, statusWidth - startX, statusHeight, 6);

	// Draw vertical separator between sections

	statusY += 50;
	ofSetColor(60, 60, 60);
	ofDrawLine(startX + leftSectionWidth + 10, statusY + 10, startX + leftSectionWidth + 10, statusY + statusHeight - 10);

	// === LEFT SECTION: HOURGLASS STATUS ===
	ofSetColor(255); // White text
	ofDrawBitmapString("HOURGLASS STATUS", textX, textY);
	textY += lineHeight * 1.2;

	// Current HourGlass Info
	auto * hg = hourglassManager->getHourGlass(currentHourGlass);
	if (hg) {
		ofSetColor(220, 220, 220); // Light gray for info text
		ofDrawBitmapString("HG " + ofToString(currentHourGlass + 1) + ": " + hg->getName(), textX, textY += lineHeight);
		ofDrawBitmapString(std::string("Port: ") + (hg->isConnected() ? hg->getSerialPort() : "N/A"), textX, textY += lineHeight);
		ofDrawBitmapString(std::string("Status: ") + (hg->isConnected() ? "Connected" : "Disconnected"), textX, textY += lineHeight);
		ofSetColor(hg->motorEnabled.get() ? ofColor::green : ofColor::red);
		ofDrawBitmapString(std::string("Motor: ") + (hg->motorEnabled.get() ? "Enabled" : "Disabled"), textX, textY += lineHeight);

		// Display LED colors and luminosity info in columns
		ofColor upColor = hg->upLedColor.get();
		ofColor downColor = hg->downLedColor.get();

		// Left column - Up LED
		ofSetColor(220, 220, 220);
		ofDrawBitmapString("Up RGB: ", textX, textY += lineHeight);
		ofSetColor(upColor);
		ofDrawRectangle(textX + 70, textY - lineHeight * 0.7, 15, 15);

		// Right column - Down LED
		ofSetColor(220, 220, 220);
		ofDrawBitmapString("Down RGB: ", textX + 200, textY);
		ofSetColor(downColor);
		ofDrawRectangle(textX + 280, textY - lineHeight * 0.7, 15, 15);

		// Next line - Main LEDs
		ofSetColor(220, 220, 220);
		ofDrawBitmapString("Up Main: " + ofToString(hg->upMainLed.get()), textX, textY += lineHeight);
		ofDrawBitmapString("Down Main: " + ofToString(hg->downMainLed.get()), textX + 200, textY);

		// Next line - Luminosity info
		ofDrawBitmapString("Global Lum: " + ofToString(LedMagnetController::getGlobalLuminosity(), 2), textX, textY += lineHeight);
		ofDrawBitmapString("Indiv Lum: " + ofToString(hg->individualLuminosity.get(), 2), textX + 200, textY);

		textY += lineHeight; // Extra space before next section
	} else {
		ofSetColor(ofColor::orangeRed);
		ofDrawBitmapString("No HourGlass selected or available.", textX, textY += lineHeight);
		textY += lineHeight;
	}

	// Draw separator
	ofSetColor(100);
	ofDrawLine(textX, textY + lineHeight * 0.25, textX + leftSectionWidth - 30, textY + lineHeight * 0.25);

	// Keyboard Shortcuts in left section
	ofSetColor(220, 220, 220);
	ofDrawBitmapString("Keyboard Shortcuts:", textX, textY += lineHeight * 1.5);

	// 3 columns of shortcuts for the wider left section
	std::vector<std::string> shortcuts1 = {
		"1-2: Select HG",
		"c: Connect",
		"x: Disconnect",
		"e: Enable",
		"q: Disable"
	};

	std::vector<std::string> shortcuts2 = {
		"z: Zero",
		"s: Stop",
		"u/d: Up/Down",
		"<->: ±45°",
		"^v: ±180°"
	};

	std::vector<std::string> shortcuts3 = {
		"r: Red",
		"g: Green",
		"b: Blue",
		"w: White",
		"o: Off"
	};

	int shortcutY = textY + lineHeight;
	for (size_t i = 0; i < std::max({ shortcuts1.size(), shortcuts2.size(), shortcuts3.size() }); i++) {
		if (i < shortcuts1.size()) ofDrawBitmapString(shortcuts1[i], textX, shortcutY);
		if (i < shortcuts2.size()) ofDrawBitmapString(shortcuts2[i], textX + 140, shortcutY);
		if (i < shortcuts3.size()) ofDrawBitmapString(shortcuts3[i], textX + 280, shortcutY);
		shortcutY += lineHeight;
	}

	// === RIGHT SECTION: SERIAL STATISTICS ===
	ofSetColor(255); // White text
	ofDrawBitmapString("SERIAL STATISTICS", rightTextX, rightTextY);
	rightTextY += lineHeight * 1.2;

	// Serial Statistics
	auto serialStats = SerialPortManager::getInstance().getStats();

	// Instant values (current frame/second)
	ofSetColor(180, 180, 180);
	ofDrawBitmapString("Instant:", rightTextX, rightTextY += lineHeight);
	ofDrawBitmapString("C/s: " + ofToString(serialStats.avgCallsPerSecond, 1), rightTextX, rightTextY += lineHeight);
	ofDrawBitmapString("B/s: " + ofToString(serialStats.avgBytesPerSecond, 0), rightTextX + 80, rightTextY);

	// 1-second average
	ofSetColor(220, 220, 220);
	ofDrawBitmapString("1s avg:", rightTextX, rightTextY += lineHeight);
	ofDrawBitmapString("C: " + ofToString(serialStats.avgCallsPerSecond_1s, 1), rightTextX, rightTextY += lineHeight);
	ofDrawBitmapString("B: " + ofToString(serialStats.avgBytesPerSecond_1s, 0), rightTextX + 80, rightTextY);

	// 5-second average
	ofDrawBitmapString("5s avg:", rightTextX, rightTextY += lineHeight);
	ofDrawBitmapString("C: " + ofToString(serialStats.avgCallsPerSecond_5s, 1), rightTextX, rightTextY += lineHeight);
	ofDrawBitmapString("B: " + ofToString(serialStats.avgBytesPerSecond_5s, 0), rightTextX + 80, rightTextY);

	// 60-frame average
	ofDrawBitmapString("60f avg:", rightTextX, rightTextY += lineHeight);
	ofDrawBitmapString("C: " + ofToString(serialStats.avgCallsPerFrame_60f, 1), rightTextX, rightTextY += lineHeight);
	ofDrawBitmapString("B: " + ofToString(serialStats.avgBytesPerFrame_60f, 1), rightTextX + 80, rightTextY);

	// Totals
	ofSetColor(160, 160, 160);
	ofDrawBitmapString("Total:", rightTextX, rightTextY += lineHeight);
	ofDrawBitmapString(ofToString(serialStats.totalCalls) + " calls", rightTextX, rightTextY += lineHeight);
	ofDrawBitmapString(ofToString(serialStats.totalBytes) + " bytes", rightTextX, rightTextY += lineHeight);

	// Draw separator
	ofSetColor(100);
	ofDrawLine(rightTextX + 10, rightTextY - lineHeight * 0.25, rightTextX + rightSectionWidth - 10, rightTextY - lineHeight * 0.25);

	// OSC Activity
	ofSetColor(220, 220, 220);
	ofDrawBitmapString("OSC Activity:", rightTextX, rightTextY += lineHeight * 1.5);
	float timeSinceLastOSC = ofGetElapsedTimef() - lastOSCMessageTime;
	if (timeSinceLastOSC < OSC_ACTIVITY_FADE_TIME) {
		float alpha = ofMap(timeSinceLastOSC, 0, OSC_ACTIVITY_FADE_TIME, 255, 0);
		ofSetColor(0, 255, 0, alpha); // Fading green
		ofDrawCircle(rightTextX + 120, rightTextY - lineHeight * 0.3, 5); // Green dot
	} else {
		ofSetColor(100); // Dimmed if no recent activity
		ofDrawCircle(rightTextX + 120, rightTextY - lineHeight * 0.3, 5); // Dim dot
	}
	rightTextY += lineHeight;
}

// Keyboard handling
void UIWrapper::handleKeyPressed(int key) {
	handleHourGlassSelection(key);
	handleConnectionCommands(key);
	handleMotorCommands(key);
	handleLEDCommands(key);
}

void UIWrapper::handleHourGlassSelection(int key) {
	if (key >= '1' && key <= '9') {
		int selection = key - '1';
		if (selection < hourglassManager->getHourGlassCount()) {
			currentHourGlass = selection;
			hourglassSelectorParam = currentHourGlass + 1; // Update GUI
			ofLogNotice("UIWrapper") << "Selected HourGlass: " << (currentHourGlass + 1);
		}
	}
}

void UIWrapper::handleConnectionCommands(int key) {
	switch (key) {
	case 'c': // Connect all
		hourglassManager->connectAll();
		ofLogNotice("UIWrapper") << "Connecting all hourglasses";
		break;
	case 'x': // Disconnect all
		hourglassManager->disconnectAll();
		ofLogNotice("UIWrapper") << "Disconnected all hourglasses";
		break;
	}
}

void UIWrapper::handleMotorCommands(int key) {
	auto * hg = hourglassManager->getHourGlass(currentHourGlass);
	if (!hg || !hg->isConnected()) return;
	float gearRatio = hg->gearRatio.get(); // Get current gear ratio from shared params

	switch (key) {
	case 'e': // Enable motor
		hg->motorEnabled.set(true);
		hg->applyMotorParameters();
		break;
	case 'q': // Disable motor
		hg->motorEnabled.set(false);
		hg->applyMotorParameters();
		break;
	case 's': // Emergency stop
		hg->emergencyStop();
		hg->motorEnabled.set(false);
		break;
	case 'z': // Set zero for current hourglass motor
		hg->getMotor()->setZero();
		ofLogNotice("UIWrapper") << "Set zero for " << hg->getName();
		break;
	case 'u': // Move motor up
		hg->getMotor()->moveRelative(hg->motorSpeed.get(), hg->motorAcceleration.get(), 1000);
		ofLogNotice("UIWrapper") << "Move motor up for " << hg->getName();
		break;
	case 'd': // Move motor down
		hg->getMotor()->moveRelative(hg->motorSpeed.get(), hg->motorAcceleration.get(), -1000);
		ofLogNotice("UIWrapper") << "Move motor down for " << hg->getName();
		break;
	case OF_KEY_LEFT: // Rotate 45 degrees counter-clockwise
		hg->getMotor()->moveRelativeAngle(hg->motorSpeed.get(), hg->motorAcceleration.get(), -45, gearRatio, hg->calibrationFactor.get());
		ofLogNotice("UIWrapper") << "Rotate 45° CCW for " << hg->getName();
		break;
	case OF_KEY_RIGHT: // Rotate 45 degrees clockwise
		hg->getMotor()->moveRelativeAngle(hg->motorSpeed.get(), hg->motorAcceleration.get(), 45, gearRatio, hg->calibrationFactor.get());
		ofLogNotice("UIWrapper") << "Rotate 45° CW for " << hg->getName();
		break;
	case OF_KEY_UP: // Rotate 180 degrees
		hg->getMotor()->moveRelativeAngle(hg->motorSpeed.get(), hg->motorAcceleration.get(), 180, gearRatio, hg->calibrationFactor.get());
		ofLogNotice("UIWrapper") << "Rotate 180° for " << hg->getName();
		break;
	case OF_KEY_DOWN: // Rotate -180 degrees
		hg->getMotor()->moveRelativeAngle(hg->motorSpeed.get(), hg->motorAcceleration.get(), -180, gearRatio, hg->calibrationFactor.get());
		ofLogNotice("UIWrapper") << "Rotate -180° for " << hg->getName();
		break;
	}
}

void UIWrapper::handleLEDCommands(int key) {
	auto * hg = hourglassManager->getHourGlass(currentHourGlass);
	if (!hg || !hg->isConnected()) return;

	float indivLum = hg->individualLuminosity.get();

	switch (key) {
	case 'o': // LEDs off
		hg->getUpLedMagnet()->sendLED(0, 0, 0, upLedBlendParam.get(), upLedOriginParam.get(), upLedArcParam.get(), indivLum);
		hg->getDownLedMagnet()->sendLED(0, 0, 0, downLedBlendParam.get(), downLedOriginParam.get(), downLedArcParam.get(), indivLum);
		setColorPreset(ofColor::black);
		break;
	}

	// Handle save shortcut separately to check for modifier keys
	if (key == 's' && (ofGetKeyPressed(OF_KEY_COMMAND) || ofGetKeyPressed(OF_KEY_CONTROL))) {
		saveSettings();
	}
}

// GUI Event Handlers
void UIWrapper::hourglassSelectorChanged(int & selection) {
	currentHourGlass = selection - 1;
	ofLogNotice("UIWrapper") << "Selected HourGlass: " << selection;

	// Update the individual luminosity slider to reflect the newly selected hourglass
	auto * hg = hourglassManager->getHourGlass(currentHourGlass);
	if (hg) {
		// Temporarily remove listener to prevent feedback loop during set
		currentHgIndividualLuminosityParam.removeListener(this, &UIWrapper::onIndividualLuminosityChanged);
		currentHgIndividualLuminosityParam.set(hg->individualLuminosity.get());
		currentHgIndividualLuminosityParam.addListener(this, &UIWrapper::onIndividualLuminosityChanged);
	} else {
		// Optionally disable or set to a default if no valid HG is selected
		currentHgIndividualLuminosityParam.removeListener(this, &UIWrapper::onIndividualLuminosityChanged);
		currentHgIndividualLuminosityParam.set(1.0f); // Default to full if HG invalid
		currentHgIndividualLuminosityParam.addListener(this, &UIWrapper::onIndividualLuminosityChanged);
	}
}

void UIWrapper::onConnectPressed() {
	hourglassManager->connectAll();
	ofLogNotice("UIWrapper") << "Connect All pressed";
}

void UIWrapper::onDisconnectPressed() {
	hourglassManager->disconnectAll();
	ofLogNotice("UIWrapper") << "Disconnect All pressed";
}

void UIWrapper::onEmergencyStopPressed() {
	auto * hg = hourglassManager->getHourGlass(currentHourGlass);
	if (hg && hg->isConnected()) {
		hg->emergencyStop();
		hg->motorEnabled.set(false);
		ofLogNotice("UIWrapper") << "Emergency Stop pressed";
	}
}

void UIWrapper::onSetZeroPressed() {
	auto * hg = hourglassManager->getHourGlass(currentHourGlass);
	if (hg && hg->isConnected()) {
		hg->getMotor()->setZero();
		ofLogNotice("UIWrapper") << "Set Zero for " << hg->getName();
	}
}

void UIWrapper::onMoveRelativePressed() {
	auto * hg = hourglassManager->getHourGlass(currentHourGlass);
	if (hg && hg->isConnected()) {
		hg->getMotor()->moveRelative(hg->motorSpeed.get(), hg->motorAcceleration.get(), relativePositionParam.get());
		ofLogNotice("UIWrapper") << "Move Relative: " << relativePositionParam.get() << " at speed " << hg->motorSpeed.get();
	}
}

void UIWrapper::onMoveAbsolutePressed() {
	auto * hg = hourglassManager->getHourGlass(currentHourGlass);
	if (hg && hg->isConnected()) {
		hg->getMotor()->moveAbsolute(hg->motorSpeed.get(), hg->motorAcceleration.get(), absolutePositionParam.get());
		ofLogNotice("UIWrapper") << "Move Absolute: " << absolutePositionParam.get() << " at speed " << hg->motorSpeed.get();
	}
}

void UIWrapper::onMoveRelativeAnglePressed() {
	auto * hg = hourglassManager->getHourGlass(currentHourGlass);
	if (hg && hg->isConnected()) {
		int angle = relativeAngleParam.get();
		hg->getMotor()->moveRelativeAngle(hg->motorSpeed.get(), hg->motorAcceleration.get(), static_cast<float>(angle), hg->gearRatio.get(), hg->calibrationFactor.get());
		ofLogNotice("UIWrapper") << "Move Relative Angle: " << angle << "° at speed " << hg->motorSpeed.get();
	}
}

void UIWrapper::onMoveAbsoluteAnglePressed() {
	auto * hg = hourglassManager->getHourGlass(currentHourGlass);
	if (hg && hg->isConnected()) {
		int angle = absoluteAngleParam.get();
		hg->getMotor()->moveAbsoluteAngle(hg->motorSpeed.get(), hg->motorAcceleration.get(), static_cast<float>(angle), hg->gearRatio.get(), hg->calibrationFactor.get());
		ofLogNotice("UIWrapper") << "Move Absolute Angle: " << angle << "° at speed " << hg->motorSpeed.get();
	}
}

void UIWrapper::syncColorsChanged(bool & enabled) {
	ofLogNotice("UIWrapper") << "Sync Colors: " << (enabled ? "ON" : "OFF");
}

void UIWrapper::onAllOffPressed() {
	// Prevent multiple triggers
	static float lastPressTime = 0;
	float currentTime = ofGetElapsedTimef();
	if (currentTime - lastPressTime < 0.5f) { // Debounce: 500ms
		return;
	}
	lastPressTime = currentTime;

	auto * hg = hourglassManager->getHourGlass(currentHourGlass);
	if (hg && hg->isConnected()) {
		// Set flag to prevent feedback
		isUpdatingFromEffects = true;

		float indivLum = hg->individualLuminosity.get();

		// Turn off RGB LEDs with effect parameters and individual luminosity
		hg->getUpLedMagnet()->sendLED(0, 0, 0, upLedBlendParam.get(), upLedOriginParam.get(), upLedArcParam.get(), indivLum);
		hg->getDownLedMagnet()->sendLED(0, 0, 0, downLedBlendParam.get(), downLedOriginParam.get(), downLedArcParam.get(), indivLum);

		// Turn off main LEDs with individual luminosity
		hg->getUpLedMagnet()->sendLED(0, indivLum);
		hg->getDownLedMagnet()->sendLED(0, indivLum);

		// Turn off PWM
		hg->getUpLedMagnet()->sendPWM(0);
		hg->getDownLedMagnet()->sendPWM(0);

		ofLogNotice("UIWrapper") << "ALL OFF - RGB LEDs, Main LEDs, and PWM all set to 0 with effect parameters and indivLum=" << indivLum;

		// Update shared parameters to reflect the changes
		hg->upLedColor.set(ofColor::black);
		hg->downLedColor.set(ofColor::black);
		hg->upMainLed.set(0);
		hg->downMainLed.set(0);
		hg->upPwm.set(0);
		hg->downPwm.set(0);

		// Update last colors
		lastUpColor = ofColor::black;
		lastDownColor = ofColor::black;

		// Clear flag
		isUpdatingFromEffects = false;
	}
}

void UIWrapper::onLedsOffPressed() {
	auto * hg = hourglassManager->getHourGlass(currentHourGlass);
	if (hg && hg->isConnected()) {
		float indivLum = hg->individualLuminosity.get();

		if (syncColorsParam.get()) {
			hg->getUpLedMagnet()->sendLED(0, 0, 0, upLedBlendParam.get(), upLedOriginParam.get(), upLedArcParam.get(), indivLum);
			hg->getDownLedMagnet()->sendLED(0, 0, 0, downLedBlendParam.get(), downLedOriginParam.get(), downLedArcParam.get(), indivLum);
			ofLogNotice("UIWrapper") << "LEDs Off - BOTH LEDs with effect parameters and indivLum=" << indivLum;
		} else {
			hg->getUpLedMagnet()->sendLED(0, 0, 0, upLedBlendParam.get(), upLedOriginParam.get(), upLedArcParam.get(), indivLum);
			ofLogNotice("UIWrapper") << "LEDs Off - UP LED only with effect parameters and indivLum=" << indivLum;
		}
		if (hg) {
			hg->upLedColor.set(ofColor::black);
			hg->downLedColor.set(ofColor::black);
		}
	}
}

void UIWrapper::setColorPreset(const ofColor & color) {
	auto * hg = hourglassManager->getHourGlass(currentHourGlass);
	if (hg && hg->isConnected()) {
		isUpdatingFromEffects = true;
		hg->upLedColor.set(color);
		hg->downLedColor.set(color);
		isUpdatingFromEffects = false;
	}
}

// Motor parameter listeners - apply changes to hardware when sliders change
void UIWrapper::onMotorEnabledChanged(bool & enabled) {
	auto * hg = hourglassManager->getHourGlass(currentHourGlass);
	if (hg && hg->isConnected() && !hg->updatingFromOSC) {
		// Apply motor enable/disable to hardware
		if (enabled) {
			hg->enableMotor();
			ofLogNotice("UIWrapper") << "⚡ UI: Motor enabled for hourglass " << (currentHourGlass + 1);
		} else {
			hg->disableMotor();
			ofLogNotice("UIWrapper") << "⚡ UI: Motor disabled for hourglass " << (currentHourGlass + 1);
		}
	}
}

void UIWrapper::onMicrostepChanged(int & value) {
	auto * hg = hourglassManager->getHourGlass(currentHourGlass);
	if (hg && hg->isConnected() && !hg->updatingFromOSC) {
		// Apply microstep change to hardware
		hg->applyMotorParameters();
		ofLogNotice("UIWrapper") << "🔧 UI: Microstep changed to " << value << " for hourglass " << (currentHourGlass + 1);
	}
}

void UIWrapper::onMotorSpeedChanged(int & value) {
	// Motor speed is just a parameter for the next movement command
	// No immediate hardware command needed
	ofLogNotice("UIWrapper") << "UI: Motor speed parameter set to " << value << " for hourglass " << (currentHourGlass + 1);
}

void UIWrapper::onMotorAccelerationChanged(int & value) {
	// Motor acceleration is just a parameter for the next movement command
	// No immediate hardware command needed, but HourGlass might apply it if motor is enabled.
	auto * hg = hourglassManager->getHourGlass(currentHourGlass);
	if (hg && hg->isConnected() && !hg->updatingFromOSC) {
		hg->applyMotorParameters(); // This will now use the int acceleration value
	}
	ofLogNotice("UIWrapper") << "UI: Motor acceleration parameter set to " << value << " for hourglass " << (currentHourGlass + 1);
}

void UIWrapper::onGearRatioChanged(float & value) {
	// Gear ratio doesn't need immediate hardware application
	// It's used in calculations for motor movements
}

void UIWrapper::onCalibrationFactorChanged(float & value) {
	// Calibration factor doesn't need immediate hardware application
	// It's used in calculations for motor movements
}

// LED parameter listeners - apply changes to hardware when sliders change
void UIWrapper::onUpLedColorChanged(ofColor & color) {
	if (isUpdatingFromEffects) return; // Skip if update is from effects/presets
	auto * hg = hourglassManager->getHourGlass(currentHourGlass);
	if (hg && hg->isConnected() && !hg->updatingFromOSC) {
		// Apply color change to hardware with current LED effect parameters and individual luminosity
		hg->getUpLedMagnet()->sendLED(color.r, color.g, color.b, upLedBlendParam.get(), upLedOriginParam.get(), upLedArcParam.get(), hg->individualLuminosity.get());

		// If sync is enabled, also update down LED
		if (syncColorsParam.get()) {
			hg->updatingFromOSC = true; // Prevent feedback
			hg->downLedColor.set(color);
			hg->updatingFromOSC = false;
			hg->getDownLedMagnet()->sendLED(color.r, color.g, color.b, downLedBlendParam.get(), downLedOriginParam.get(), downLedArcParam.get(), hg->individualLuminosity.get());
		}
	}
}

void UIWrapper::onDownLedColorChanged(ofColor & color) {
	if (isUpdatingFromEffects) return; // Skip if update is from effects/presets
	auto * hg = hourglassManager->getHourGlass(currentHourGlass);
	if (hg && hg->isConnected() && !hg->updatingFromOSC) {
		// Apply color change to hardware with current LED effect parameters and individual luminosity
		hg->getDownLedMagnet()->sendLED(color.r, color.g, color.b, downLedBlendParam.get(), downLedOriginParam.get(), downLedArcParam.get(), hg->individualLuminosity.get());

		// If sync is enabled, also update up LED
		if (syncColorsParam.get()) {
			hg->updatingFromOSC = true; // Prevent feedback
			hg->upLedColor.set(color);
			hg->updatingFromOSC = false;
			hg->getUpLedMagnet()->sendLED(color.r, color.g, color.b, upLedBlendParam.get(), upLedOriginParam.get(), upLedArcParam.get(), hg->individualLuminosity.get());
		}
	}
}

void UIWrapper::onUpMainLedChanged(int & value) {
	auto * hg = hourglassManager->getHourGlass(currentHourGlass);
	if (hg && hg->isConnected() && !hg->updatingFromOSC) {
		// Apply main LED change to hardware with individual luminosity
		hg->getUpLedMagnet()->sendLED(static_cast<uint8_t>(value), hg->individualLuminosity.get());
	}
}

void UIWrapper::onDownMainLedChanged(int & value) {
	auto * hg = hourglassManager->getHourGlass(currentHourGlass);
	if (hg && hg->isConnected() && !hg->updatingFromOSC) {
		// Apply main LED change to hardware with individual luminosity
		hg->getDownLedMagnet()->sendLED(static_cast<uint8_t>(value), hg->individualLuminosity.get());
	}
}

void UIWrapper::onUpPwmChanged(int & value) {
	auto * hg = hourglassManager->getHourGlass(currentHourGlass);
	if (hg && hg->isConnected() && !hg->updatingFromOSC) {
		// Apply PWM change to hardware
		hg->getUpLedMagnet()->sendPWM(static_cast<uint8_t>(value));
	}
}

void UIWrapper::onDownPwmChanged(int & value) {
	auto * hg = hourglassManager->getHourGlass(currentHourGlass);
	if (hg && hg->isConnected() && !hg->updatingFromOSC) {
		// Apply PWM change to hardware
		hg->getDownLedMagnet()->sendPWM(static_cast<uint8_t>(value));
	}
}

// Position parameter synchronization for OSC commands
void UIWrapper::updatePositionParameters(float relativeAngle, float absoluteAngle) {
	// Update the UI input fields to reflect the angles used in OSC commands
	// This provides visual feedback to the user about what positions were commanded
	relativeAngleParam.set(static_cast<int>(relativeAngle));
	absoluteAngleParam.set(static_cast<int>(absoluteAngle));
}

// OSC activity tracking
void UIWrapper::notifyOSCMessageReceived() {
	lastOSCMessageTime = ofGetElapsedTimef();
}

// XML save/load for persistence
void UIWrapper::saveSettings() {
	// Save all panel settings to their respective XML files
	settingsPanel.saveToFile("settings_actions.xml");
	motorPanel.saveToFile("motor.xml");
	ledUpPanel.saveToFile("led_up.xml");
	ledDownPanel.saveToFile("led_down.xml");
	luminosityPanel.saveToFile("luminosity.xml");

	// Save HourGlass configurations (device IDs, ports, etc.)
	ofXml hourglassConfig;
	auto configurationsNode = hourglassConfig.appendChild("HourGlassConfigurations");

	for (size_t i = 0; i < hourglassManager->getHourGlassCount(); i++) {
		auto * hg = hourglassManager->getHourGlass(i);
		if (hg) {
			auto hgNode = configurationsNode.appendChild("HourGlass");
			hgNode.setAttribute("name", hg->getName());
			hgNode.setAttribute("serialPort", hg->getSerialPort());
			hgNode.setAttribute("baudRate", ofToString(hg->getBaudRate()));
			hgNode.setAttribute("upLedId", ofToString(hg->getUpLedId()));
			hgNode.setAttribute("downLedId", ofToString(hg->getDownLedId()));
			hgNode.setAttribute("motorId", ofToString(hg->getMotorId()));
		}
	}

	hourglassConfig.save("hourglass_config.xml");

	ofLogNotice("UIWrapper") << "💾 Settings and HourGlass configurations saved to XML files";
}

void UIWrapper::loadSettings() {
	// Load all panel settings from their respective XML files
	settingsPanel.loadFromFile("settings_actions.xml");
	motorPanel.loadFromFile("motor.xml");
	ledUpPanel.loadFromFile("led_up.xml");
	ledDownPanel.loadFromFile("led_down.xml");
	luminosityPanel.loadFromFile("luminosity.xml");

	// Load HourGlass configurations (device IDs, ports, etc.)
	ofXml hourglassConfig;
	if (hourglassConfig.load("hourglass_config.xml")) {
		auto configurationsNode = hourglassConfig.findFirst("HourGlassConfigurations");
		if (configurationsNode) {
			auto hourglassNodes = configurationsNode.getChildren("HourGlass");

			// Iterate over hourglass nodes
			size_t i = 0;
			for (auto & node : hourglassNodes) {
				if (i >= hourglassManager->getHourGlassCount()) break;

				auto * hg = hourglassManager->getHourGlass(i);
				if (hg) {
					std::string serialPort = node.getAttribute("serialPort").getValue();
					int baudRate = ofToInt(node.getAttribute("baudRate").getValue());
					int upLedId = ofToInt(node.getAttribute("upLedId").getValue());
					int downLedId = ofToInt(node.getAttribute("downLedId").getValue());
					int motorId = ofToInt(node.getAttribute("motorId").getValue());

					// Reconfigure the HourGlass with saved settings
					hg->configure(serialPort, baudRate, upLedId, downLedId, motorId);
				}
				i++;
			}
		}
	}

	ofLogNotice("UIWrapper") << "📂 Settings and HourGlass configurations loaded from XML files";
}

// Implement the new listener method
void UIWrapper::onGlobalLuminosityChanged(float & luminosity) {
	LedMagnetController::setGlobalLuminosity(luminosity);
	ofLogNotice("UIWrapper") << "💡 Global Luminosity changed via UI to: " << luminosity;
	if (oscControllerInstance) {
		oscControllerInstance->forceRefreshAllHardwareStates();
	}
}

void UIWrapper::onIndividualLuminosityChanged(float & luminosity) {
	auto * hg = hourglassManager->getHourGlass(currentHourGlass);
	if (hg && !hg->updatingFromOSC) { // No need to check hg->isConnected() here, as we're setting a param
		// Set the ofParameter on the HourGlass object.
		// This will trigger HourGlass's own internal listeners if it has any to apply changes.
		hg->individualLuminosity.set(luminosity);
		ofLogNotice("UIWrapper") << "💡 Individual Luminosity UI for HG " << (currentHourGlass + 1) << " changed to: " << luminosity;

		// Force OSCController to re-evaluate and send all states because effective output will change
		if (oscControllerInstance) {
			oscControllerInstance->forceRefreshAllHardwareStates();
		}
	} else if (!hg) {
		ofLogWarning("UIWrapper") << "Attempted to change individual luminosity for an invalid HourGlass selection.";
	}
}

void UIWrapper::updateGlobalLuminositySlider(float luminosity) {
	// Temporarily remove listener to prevent feedback loop if set triggers listener
	globalLuminosityParam.removeListener(this, &UIWrapper::onGlobalLuminosityChanged);
	globalLuminosityParam.set(luminosity);
	globalLuminosityParam.addListener(this, &UIWrapper::onGlobalLuminosityChanged);
	ofLogNotice("UIWrapper") << "Global luminosity slider updated to: " << luminosity;
}

void UIWrapper::updateCurrentIndividualLuminositySlider(float luminosity) {
	// Temporarily remove listener to prevent feedback loop
	currentHgIndividualLuminosityParam.removeListener(this, &UIWrapper::onIndividualLuminosityChanged);
	currentHgIndividualLuminosityParam.set(luminosity);
	currentHgIndividualLuminosityParam.addListener(this, &UIWrapper::onIndividualLuminosityChanged);
}

void UIWrapper::onUpLedBlendChanged(int & value) {
	// Debug: Check if listener is being called

	auto * hg = hourglassManager->getHourGlass(currentHourGlass);
	if (hg && hg->isConnected() && !hg->updatingFromOSC) {
		// Apply the current up LED color with the new blend parameter and individual luminosity
		ofColor upColor = hg->upLedColor.get();

		hg->getUpLedMagnet()->sendLED(upColor.r, upColor.g, upColor.b, value, upLedOriginParam.get(), upLedArcParam.get(), hg->individualLuminosity.get());

		// If sync is enabled, also update down LED blend
		if (syncColorsParam.get()) {
			downLedBlendParam.removeListener(this, &UIWrapper::onDownLedBlendChanged);
			downLedBlendParam.set(value);
			downLedBlendParam.addListener(this, &UIWrapper::onDownLedBlendChanged);

			ofColor downColor = hg->downLedColor.get();
			hg->getDownLedMagnet()->sendLED(downColor.r, downColor.g, downColor.b, value, downLedOriginParam.get(), downLedArcParam.get(), hg->individualLuminosity.get());
			ofLogNotice("UIWrapper") << "🔄 UI: Synced down LED blend to " << value;
		}
	} else {
	}
}

void UIWrapper::onUpLedOriginChanged(int & value) {
	// Debug: Check if listener is being called

	auto * hg = hourglassManager->getHourGlass(currentHourGlass);
	if (hg && hg->isConnected() && !hg->updatingFromOSC) {
		// Apply the current up LED color with the new origin parameter and individual luminosity
		ofColor upColor = hg->upLedColor.get();

		hg->getUpLedMagnet()->sendLED(upColor.r, upColor.g, upColor.b, upLedBlendParam.get(), value, upLedArcParam.get(), hg->individualLuminosity.get());

		ofLogNotice("UIWrapper") << "📍 UI: Up LED Origin changed to " << value << "° for hourglass " << (currentHourGlass + 1) << " with indivLum=" << hg->individualLuminosity.get();

		// If sync is enabled, also update down LED origin
		if (syncColorsParam.get()) {
			downLedOriginParam.removeListener(this, &UIWrapper::onDownLedOriginChanged);
			downLedOriginParam.set(value);
			downLedOriginParam.addListener(this, &UIWrapper::onDownLedOriginChanged);

			ofColor downColor = hg->downLedColor.get();
			hg->getDownLedMagnet()->sendLED(downColor.r, downColor.g, downColor.b, downLedBlendParam.get(), value, downLedArcParam.get(), hg->individualLuminosity.get());
			ofLogNotice("UIWrapper") << "🔄 UI: Synced down LED origin to " << value << "°";
		}
	} else {
	}
}

void UIWrapper::onUpLedArcChanged(int & value) {
	// Debug: Check if listener is being called

	auto * hg = hourglassManager->getHourGlass(currentHourGlass);
	if (hg && hg->isConnected() && !hg->updatingFromOSC) {
		// Apply the current up LED color with the new arc parameter and individual luminosity
		ofColor upColor = hg->upLedColor.get();

		hg->getUpLedMagnet()->sendLED(upColor.r, upColor.g, upColor.b, upLedBlendParam.get(), upLedOriginParam.get(), value, hg->individualLuminosity.get());

		ofLogNotice("UIWrapper") << "🌅 UI: Up LED Arc changed to " << value << "° for hourglass " << (currentHourGlass + 1) << " with indivLum=" << hg->individualLuminosity.get();

		// If sync is enabled, also update down LED arc
		if (syncColorsParam.get()) {
			downLedArcParam.removeListener(this, &UIWrapper::onDownLedArcChanged);
			downLedArcParam.set(value);
			downLedArcParam.addListener(this, &UIWrapper::onDownLedArcChanged);

			ofColor downColor = hg->downLedColor.get();
			hg->getDownLedMagnet()->sendLED(downColor.r, downColor.g, downColor.b, downLedBlendParam.get(), downLedOriginParam.get(), value, hg->individualLuminosity.get());
			ofLogNotice("UIWrapper") << "🔄 UI: Synced down LED arc to " << value << "°";
		}
	} else {
	}
}

void UIWrapper::onDownLedBlendChanged(int & value) {
	auto * hg = hourglassManager->getHourGlass(currentHourGlass);
	if (hg && hg->isConnected() && !hg->updatingFromOSC) {
		// Apply the current down LED color with the new blend parameter and individual luminosity
		ofColor downColor = hg->downLedColor.get();

		hg->getDownLedMagnet()->sendLED(downColor.r, downColor.g, downColor.b, value, downLedOriginParam.get(), downLedArcParam.get(), hg->individualLuminosity.get());

		ofLogNotice("UIWrapper") << "🌀 UI: Down LED Blend changed to " << value << " for hourglass " << (currentHourGlass + 1) << " with indivLum=" << hg->individualLuminosity.get();

		// If sync is enabled, also update up LED blend
		if (syncColorsParam.get()) {
			upLedBlendParam.removeListener(this, &UIWrapper::onUpLedBlendChanged);
			upLedBlendParam.set(value);
			upLedBlendParam.addListener(this, &UIWrapper::onUpLedBlendChanged);

			ofColor upColor = hg->upLedColor.get();
			hg->getUpLedMagnet()->sendLED(upColor.r, upColor.g, upColor.b, value, upLedOriginParam.get(), upLedArcParam.get(), hg->individualLuminosity.get());
			ofLogNotice("UIWrapper") << "🔄 UI: Synced up LED blend to " << value;
		}
	}
}

void UIWrapper::onDownLedOriginChanged(int & value) {
	auto * hg = hourglassManager->getHourGlass(currentHourGlass);
	if (hg && hg->isConnected() && !hg->updatingFromOSC) {
		// Apply the current down LED color with the new origin parameter and individual luminosity
		ofColor downColor = hg->downLedColor.get();

		hg->getDownLedMagnet()->sendLED(downColor.r, downColor.g, downColor.b, downLedBlendParam.get(), value, downLedArcParam.get(), hg->individualLuminosity.get());

		ofLogNotice("UIWrapper") << "📍 UI: Down LED Origin changed to " << value << "° for hourglass " << (currentHourGlass + 1) << " with indivLum=" << hg->individualLuminosity.get();

		// If sync is enabled, also update up LED origin
		if (syncColorsParam.get()) {
			upLedOriginParam.removeListener(this, &UIWrapper::onUpLedOriginChanged);
			upLedOriginParam.set(value);
			upLedOriginParam.addListener(this, &UIWrapper::onUpLedOriginChanged);

			ofColor upColor = hg->upLedColor.get();
			hg->getUpLedMagnet()->sendLED(upColor.r, upColor.g, upColor.b, upLedBlendParam.get(), value, upLedArcParam.get(), hg->individualLuminosity.get());
			ofLogNotice("UIWrapper") << "🔄 UI: Synced up LED origin to " << value << "°";
		}
	}
}

void UIWrapper::onDownLedArcChanged(int & value) {
	auto * hg = hourglassManager->getHourGlass(currentHourGlass);
	if (hg && hg->isConnected() && !hg->updatingFromOSC) {
		// Apply the current down LED color with the new arc parameter and individual luminosity
		ofColor downColor = hg->downLedColor.get();

		hg->getDownLedMagnet()->sendLED(downColor.r, downColor.g, downColor.b, downLedBlendParam.get(), downLedOriginParam.get(), value, hg->individualLuminosity.get());

		ofLogNotice("UIWrapper") << "🌅 UI: Down LED Arc changed to " << value << "° for hourglass " << (currentHourGlass + 1) << " with indivLum=" << hg->individualLuminosity.get();

		// If sync is enabled, also update up LED arc
		if (syncColorsParam.get()) {
			upLedArcParam.removeListener(this, &UIWrapper::onUpLedArcChanged);
			upLedArcParam.set(value);
			upLedArcParam.addListener(this, &UIWrapper::onUpLedArcChanged);

			ofColor upColor = hg->upLedColor.get();
			hg->getUpLedMagnet()->sendLED(upColor.r, upColor.g, upColor.b, upLedBlendParam.get(), upLedOriginParam.get(), value, hg->individualLuminosity.get());
			ofLogNotice("UIWrapper") << "🔄 UI: Synced up LED arc to " << value << "°";
		}
	}
}
