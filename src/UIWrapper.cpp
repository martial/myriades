#include "UIWrapper.h"
#include "ArcCosineEffect.h"
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

	// Setup LED visualizer - small preview below luminosity panel
	ledVisualizer.setup(200, 120); // Bigger size
	ledVisualizer.setLayoutMode(0); // Grid layout
	ledVisualizer.setShowLabels(false); // No labels for compact view
	ledVisualizer.setShowGrid(false);
	ledVisualizer.setUIWrapper(this); // Give visualizer access to UI parameters

	// Add all hourglasses to visualizer
	for (int i = 0; i < hourglassManager->getHourGlassCount(); i++) {
		auto * hg = hourglassManager->getHourGlass(i);
		if (hg) {
			ledVisualizer.addHourGlass(hg, "HG " + ofToString(i + 1));
		}
	}

	// Sync selector with current selection
	hourglassSelectorParam = currentHourGlass + 1;

	// Load saved settings and configurations
	loadSettings();

	// CRITICAL FIX: Set up initial UI panel bindings for the first hourglass
	updateUIPanelsBinding();

	ofLogNotice("UIWrapper") << "Setup complete with " << hourglassManager->getHourGlassCount() << " hourglasses";
}

void UIWrapper::update() {
	// Update OSC parameter sync (disabled)
	// oscSync.update();

	// Update serial statistics
	SerialPortManager::getInstance().updateStats();

	// Update LED visualizer
	ledVisualizer.update();

	// Update effects for all hourglasses
	if (hourglassManager) {
		float deltaTime = ofGetLastFrameTime();
		for (int i = 0; i < hourglassManager->getHourGlassCount(); ++i) {
			HourGlass * hg = hourglassManager->getHourGlass(i);
			if (hg) {
				hg->updateEffects(deltaTime);
				if (hg->isConnected()) {
					hg->applyLedParameters();
					hg->applyMotorParameters();
				}
			}
		}
	}

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
	effectsPanel.draw();
	// globalSettingsPanel.draw(); // Removed: Will be part of settingsPanel

	// Draw enhanced status panel
	drawStatus();

	// Draw LED visualizer at specified coordinates
	int visualizerX = 980;
	int visualizerY = 90;
	ledVisualizer.draw(visualizerX, visualizerY);
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

	// New addition: Set Zero ALL Motors button
	setZeroAllBtnParam.set("Set Zero ALL Motors");
	setZeroAllBtn.setup(setZeroAllBtnParam.getName());
	settingsPanel.add(&setZeroAllBtn);

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

	// NOTE: Motor panel contents will be populated by updateUIPanelsBinding()

	relativeAngleInput.setup(relativeAngleParam);
	absoluteAngleInput.setup(absoluteAngleParam);

	emergencyStopBtn.setup(emergencyStopBtnParam.getName());
	setZeroBtn.setup(setZeroBtnParam.getName());
	moveRelativeBtn.setup(moveRelativeBtnParam.getName());
	moveAbsoluteBtn.setup(moveAbsoluteBtnParam.getName());
	moveRelativeAngleBtn.setup(moveRelativeAngleBtnParam.getName());
	moveAbsoluteAngleBtn.setup(moveAbsoluteAngleBtnParam.getName());

	// === PANEL 3: UP LED CONTROLLER (Center) ===
	ledUpPanel.setup("UP LED CONTROLLER", "led_up.xml", startX + (panelWidth + panelSpacing) * 2, startY);
	ledUpPanel.setDefaultBackgroundColor(ofColor(20, 20, 20, 180));
	ledUpPanel.setDefaultFillColor(ofColor(60, 60, 60, 160));
	ledUpPanel.setDefaultHeaderBackgroundColor(ofColor(40, 40, 40, 200));
	ledUpPanel.setDefaultTextColor(ofColor(255, 255, 255));

	// NOTE: LED panel contents will be populated by updateUIPanelsBinding()

	// Up LED Effect Parameters
	upLedBlendParam.set("Blend", 0, 0, 768);
	upLedOriginParam.set("Origin (°)", 0, 0, 360);
	upLedArcParam.set("Arc (°)", 360, 0, 360);

	// === PANEL 4: DOWN LED CONTROLLER (Center-Right) ===
	ledDownPanel.setup("DOWN LED CONTROLLER", "led_down.xml", startX + (panelWidth + panelSpacing) * 3, startY);
	ledDownPanel.setDefaultBackgroundColor(ofColor(20, 20, 20, 180));
	ledDownPanel.setDefaultFillColor(ofColor(60, 60, 60, 160));
	ledDownPanel.setDefaultHeaderBackgroundColor(ofColor(40, 40, 40, 200));
	ledDownPanel.setDefaultTextColor(ofColor(255, 255, 255));

	// NOTE: LED panel contents will be populated by updateUIPanelsBinding()

	// Down LED Effect Parameters
	downLedBlendParam.set("Blend", 0, 0, 768);
	downLedOriginParam.set("Origin (°)", 0, 0, 360);
	downLedArcParam.set("Arc (°)", 360, 0, 360);

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

	// === PANEL 6: EFFECTS (New) ===
	// Adjust panelWidth, panelHeight, panelSpacing, startX, startY as needed.
	// Assuming 5 panels before this, so index 5
	int effectsPanelX = startX + (panelWidth + panelSpacing) * 5;
	effectsPanel.setup("EFFECTS", "effects.xml", effectsPanelX, startY);
	effectsPanel.setDefaultBackgroundColor(ofColor(20, 20, 20, 180));
	effectsPanel.setDefaultFillColor(ofColor(60, 60, 60, 160));
	effectsPanel.setDefaultHeaderBackgroundColor(ofColor(40, 40, 40, 200));
	effectsPanel.setDefaultTextColor(ofColor(255, 255, 255));

	addCosineArcEffectBtn.setup("Add Cosine Arc Effect");
	clearAllEffectsBtn.setup("Clear All Effects");

	effectsPanel.add(&addCosineArcEffectBtn);
	effectsPanel.add(&clearAllEffectsBtn);
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
		// Disable ofxOscParameterSync for now, use OSCController for everything
		// oscSync.setup(hg->getParameterGroup(), 9000, "localhost", 9001);
		ofLogNotice("UIWrapper") << "Using OSCController only - all OSC on port 8000";

		// Log simple OSC addresses for RGB control
		ofLogNotice("OSC Addresses") << "RGB Color control addresses (port 8000):";
		ofLogNotice("OSC Addresses") << "  /rgb [r] [g] [b]           - Set both LEDs";
		ofLogNotice("OSC Addresses") << "  /up/rgb [r] [g] [b]        - Set upper LED";
		ofLogNotice("OSC Addresses") << "  /down/rgb [r] [g] [b]      - Set lower LED";

		// NOTE: Parameter listeners will be set up by updateListenersForCurrentHourglass()
	}

	// Quick action listeners
	allOffBtn.addListener(this, &UIWrapper::onAllOffPressed);
	ledsOffBtn.addListener(this, &UIWrapper::onLedsOffPressed);

	// Global Luminosity Listener
	globalLuminosityParam.addListener(this, &UIWrapper::onGlobalLuminosityChanged);

	// Effects Listeners
	addCosineArcEffectBtn.addListener(this, &UIWrapper::onAddCosineArcEffectPressed);
	clearAllEffectsBtn.addListener(this, &UIWrapper::onClearAllEffectsPressed);

	// New addition: Set Zero ALL Motors button listener
	setZeroAllBtn.addListener(this, &UIWrapper::onSetZeroAllPressed);
}

void UIWrapper::drawStatus() {
	// Position lower on screen to use more bottom space
	int panelWidth = 225;
	int panelSpacing = 15;
	int startX = 15;
	int statusY = 420;
	int statusWidth = (panelWidth * 5) + (panelSpacing * 4);

	// Split into two sections: left for HG info, right for serial stats
	int leftSectionWidth = statusWidth * 0.65;
	int rightSectionWidth = statusWidth * 0.35;

	int textX = startX + 15;
	int rightTextX = startX + leftSectionWidth + 20;
	int initialStatusContentY = statusY + 25; // Base Y for content within the status box, below its top padding
	int lineHeight = 18;

	// Calculate content height dynamically based on what we're keeping
	// Left: Title(1.5), HG Name(1), Port(1), Status(1), Motor(1), Space(1), Separator(0.5), Shortcuts Title(1.5), Shortcuts(3 lines max)
	// Total approx = 1.5+4+1+0.5+1.5+3 = 11.5 lines
	int leftContentLines = 12;
	// Right: Title(1), Bandwidth Bar(takes space of ~1 line text), Instant Label(1)+Data(1), 1s Avg Label(1)+Data(1), Total Label(1)+Data(2), Separator(0.5), OSC Label(1)
	// Total approx = 1+1+2+2+3+0.5+1 = 10.5 lines (but bar is wider)
	int rightContentLines = 11;
	int maxContentLines = std::max(leftContentLines, rightContentLines);
	int statusHeight = 15 + (maxContentLines * lineHeight) + 80; // Top/bottom padding for content area

	ofSetColor(40, 40, 45, 180);
	ofDrawRectRounded(startX, statusY, statusWidth - startX, statusHeight, 6);

	ofSetColor(60, 60, 60);
	ofDrawLine(startX + leftSectionWidth + 10, statusY + 10, startX + leftSectionWidth + 10, statusY + statusHeight - 10);

	// === LEFT SECTION: HOURGLASS STATUS ===
	int currentLeftY = initialStatusContentY;
	ofSetColor(255);
	ofDrawBitmapString("HOURGLASS STATUS", textX, currentLeftY);
	currentLeftY += lineHeight * 1.5; // Space after title

	auto * hg = hourglassManager->getHourGlass(currentHourGlass);
	if (hg) {
		ofSetColor(220, 220, 220);
		ofDrawBitmapString("HG " + ofToString(currentHourGlass + 1) + ": " + hg->getName(), textX, currentLeftY);
		currentLeftY += lineHeight;
		ofDrawBitmapString(std::string("Port: ") + (hg->isConnected() ? hg->getSerialPort() : "N/A"), textX, currentLeftY);
		currentLeftY += lineHeight;
		ofDrawBitmapString(std::string("Status: ") + (hg->isConnected() ? "Connected" : "Disconnected"), textX, currentLeftY);
		currentLeftY += lineHeight;
		ofSetColor(hg->motorEnabled.get() ? ofColor::green : ofColor::red);
		ofDrawBitmapString(std::string("Motor: ") + (hg->motorEnabled.get() ? "Enabled" : "Disabled"), textX, currentLeftY);
		currentLeftY += lineHeight * 1.5; // Extra space after HG info
	} else {
		ofSetColor(ofColor::orangeRed);
		ofDrawBitmapString("No HourGlass selected or available.", textX, currentLeftY);
		currentLeftY += lineHeight * 1.5;
	}

	ofSetColor(100);
	ofDrawLine(textX, currentLeftY, textX + leftSectionWidth - 30, currentLeftY); // Separator line
	currentLeftY += lineHeight * 1.5; // Space after separator

	ofSetColor(220, 220, 220);
	ofDrawBitmapString("Keyboard Shortcuts:", textX, currentLeftY);
	currentLeftY += lineHeight; // Start shortcuts on the next line

	// Simplified shortcuts
	std::vector<std::string> shortcuts = {
		"1-2: Select HG", "c/x: Connect/Disconnect",
		"z: Zero Motor", "s: Stop Motor (Emerg.)", // Clarified 's'
		"Arrows: Rotate Motor", "u/d: Move Steps (Rel)" // Clarified 'u/d'
	};
	for (const auto & shortcut : shortcuts) {
		ofDrawBitmapString(shortcut, textX, currentLeftY);
		currentLeftY += lineHeight;
	}

	// === RIGHT SECTION: SERIAL STATISTICS ===
	int currentRightY = initialStatusContentY;
	ofSetColor(255);
	float statsTitleY = currentRightY; // Y for the "SERIAL STATISTICS" title
	ofDrawBitmapString("SERIAL STATISTICS", rightTextX, statsTitleY);
	// Bandwidth bar will be positioned relative to statsTitleY and rightTextX

	// --- Bandwidth Usage Indicator ---
	auto serialStats = SerialPortManager::getInstance().getStats();
	float baudRateBps = 28800.0f;
	float usagePercentage = 0.0f;
	if (baudRateBps > 0) {
		usagePercentage = (serialStats.avgBytesPerSecond_1s / baudRateBps) * 100.0f;
		usagePercentage = std::max(0.0f, std::min(usagePercentage, 100.0f));
	}
	ofColor usageBarColor = ofColor::darkGreen;
	ofColor usageTextColor = ofColor::white;
	if (usagePercentage >= 70.0f) {
		usageBarColor = ofColor::darkRed;
		usageTextColor = ofColor(255, 180, 180);
	} else if (usagePercentage >= 30.0f) {
		usageBarColor = ofColor::darkOrange;
		usageTextColor = ofColor::white;
	}

	int barMaxWidth = 80; // Slightly narrower bar
	int barHeight = lineHeight * 0.65f;
	int barX = ofGetWidth() - barMaxWidth - 60; // Adjusted X to be closer to title
	// Align bar's top with the title's baseline, then shift up slightly if needed
	int barY = statsTitleY - barHeight + (lineHeight - barHeight) / 2 - 2; // Fine-tune this offset
	float currentBarWidth = ofMap(usagePercentage, 0, 100, 0, barMaxWidth, true);

	ofSetColor(50, 50, 50);
	ofDrawRectangle(barX, barY, barMaxWidth, barHeight);
	ofSetColor(usageBarColor);
	ofDrawRectangle(barX, barY, currentBarWidth, barHeight);
	ofSetColor(80, 80, 80);
	ofNoFill();
	ofDrawRectangle(barX, barY, barMaxWidth, barHeight);
	ofFill();
	ofSetColor(usageTextColor);
	std::string usageText = ofToString(usagePercentage, 1) + "%";

	float textBaselineY = barY + barHeight - (barHeight * 0.15f); // e.g., 85% down the bar's height
	ofDrawBitmapString(usageText, barX + barMaxWidth + 5, textBaselineY);
	// --- End Bandwidth Usage Indicator ---

	currentRightY += lineHeight * 1.5; // Space after title line for stats data

	// Instant values
	ofSetColor(180, 180, 180);
	ofDrawBitmapString("Instant:", rightTextX, currentRightY);
	currentRightY += lineHeight;
	ofDrawBitmapString("C/s: " + ofToString(serialStats.avgCallsPerSecond, 1), rightTextX, currentRightY);
	ofDrawBitmapString("B/s: " + ofToString(serialStats.avgBytesPerSecond, 0), rightTextX + 80, currentRightY);

	// 1-second average
	currentRightY += lineHeight;
	ofSetColor(220, 220, 220);
	ofDrawBitmapString("1s avg:", rightTextX, currentRightY);
	currentRightY += lineHeight;
	ofDrawBitmapString("C: " + ofToString(serialStats.avgCallsPerSecond_1s, 1), rightTextX, currentRightY);
	ofDrawBitmapString("B: " + ofToString(serialStats.avgBytesPerSecond_1s, 0), rightTextX + 80, currentRightY);

	// Totals
	currentRightY += lineHeight;
	ofSetColor(160, 160, 160);
	ofDrawBitmapString("Total:", rightTextX, currentRightY);
	currentRightY += lineHeight;
	ofDrawBitmapString(ofToString(serialStats.totalCalls) + " calls", rightTextX, currentRightY);
	currentRightY += lineHeight;
	ofDrawBitmapString(ofToString(serialStats.totalBytes) + " bytes", rightTextX, currentRightY);

	currentRightY += lineHeight;
	currentRightY += 30;

	ofSetColor(100);
	ofDrawLine(rightTextX, currentRightY - lineHeight, rightTextX + rightSectionWidth - 60, currentRightY - lineHeight);

	// OSC Activity
	ofSetColor(220, 220, 220);
	currentRightY += lineHeight;
	ofDrawBitmapString("OSC:", rightTextX, currentRightY);
	float timeSinceLastOSC = ofGetElapsedTimef() - lastOSCMessageTime;
	if (timeSinceLastOSC < OSC_ACTIVITY_FADE_TIME) {
		float alpha = ofMap(timeSinceLastOSC, 0, OSC_ACTIVITY_FADE_TIME, 255, 0);
		ofSetColor(0, 255, 0, alpha);
		ofDrawCircle(rightTextX + 100, currentRightY - lineHeight * 0.3f + 2, 4); // Adjusted position and size
	} else {
		ofSetColor(100);
		ofDrawCircle(rightTextX + 100, currentRightY - lineHeight * 0.3f + 2, 4); // Adjusted position and size
	}
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

	// motorEnabled and emergencyStop/setZero can still be direct or go through command methods if preferred
	// For movement commands:
	switch (key) {
	case 'u': // Move motor up
		hg->commandRelativeMove(1000); // Example steps
		ofLogNotice("UIWrapper") << "Key U: Commanded relative move up for " << hg->getName();
		break;
	case 'd': // Move motor down
		hg->commandRelativeMove(-1000); // Example steps
		ofLogNotice("UIWrapper") << "Key D: Commanded relative move down for " << hg->getName();
		break;
	case OF_KEY_LEFT:
		hg->commandRelativeAngle(-45.0f);
		ofLogNotice("UIWrapper") << "Key Left: Commanded relative angle -45 for " << hg->getName();
		break;
	case OF_KEY_RIGHT:
		hg->commandRelativeAngle(45.0f);
		ofLogNotice("UIWrapper") << "Key Right: Commanded relative angle +45 for " << hg->getName();
		break;
	case OF_KEY_UP:
		hg->commandRelativeAngle(180.0f);
		ofLogNotice("UIWrapper") << "Key Up: Commanded relative angle 180 for " << hg->getName();
		break;
	case OF_KEY_DOWN:
		hg->commandRelativeAngle(-180.0f);
		ofLogNotice("UIWrapper") << "Key Down: Commanded relative angle -180 for " << hg->getName();
		break;
	}
	// Ensure motorEnabled changes also call hg->motorEnabled.set() so applyMotorParameters picks it up.
	// if (key == 'e') hg->motorEnabled.set(true);
	// if (key == 'q') hg->motorEnabled.set(false);
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

		// CRITICAL FIX: Update UI panels to bind to the newly selected hourglass
		updateUIPanelsBinding();
	} else {
		// Optionally disable or set to a default if no valid HG is selected
		currentHgIndividualLuminosityParam.removeListener(this, &UIWrapper::onIndividualLuminosityChanged);
		currentHgIndividualLuminosityParam.set(1.0f); // Default to full if HG invalid
		currentHgIndividualLuminosityParam.addListener(this, &UIWrapper::onIndividualLuminosityChanged);
	}
}

void UIWrapper::updateUIPanelsBinding() {
	auto * hg = hourglassManager->getHourGlass(currentHourGlass);
	if (!hg) return;

	// Clear and rebuild LED panels with correct hourglass bindings
	ledUpPanel.clear();
	ledDownPanel.clear();
	motorPanel.clear();

	// === UP LED PANEL ===
	ledUpPanel.add(hg->upLedColor.set("RGB Color", hg->upLedColor.get()));
	ledUpPanel.add(hg->upMainLed.set("Main LED", hg->upMainLed.get(), 0, 255));
	ledUpPanel.add(hg->upPwm.set("PWM", hg->upPwm.get(), 0, 255));
	ledUpPanel.add(upLedBlendParam);
	ledUpPanel.add(upLedOriginParam);
	ledUpPanel.add(upLedArcParam);
	ledUpPanel.getGroup("RGB Color").maximize();

	// === DOWN LED PANEL ===
	ledDownPanel.add(hg->downLedColor.set("RGB Color", hg->downLedColor.get()));
	ledDownPanel.add(hg->downMainLed.set("Main LED", hg->downMainLed.get(), 0, 255));
	ledDownPanel.add(hg->downPwm.set("PWM", hg->downPwm.get(), 0, 255));
	ledDownPanel.add(downLedBlendParam);
	ledDownPanel.add(downLedOriginParam);
	ledDownPanel.add(downLedArcParam);
	ledDownPanel.getGroup("RGB Color").maximize();

	// === MOTOR PANEL ===
	motorPanel.add(hg->motorEnabled);
	motorPanel.add(hg->microstep);
	motorPanel.add(hg->motorSpeed);
	motorPanel.add(hg->motorAcceleration);
	gearRatioInput.setup(hg->gearRatio);
	calibrationFactorInput.setup(hg->calibrationFactor);
	motorPanel.add(&gearRatioInput);
	motorPanel.add(&calibrationFactorInput);
	motorPanel.add(relativePositionParam);
	motorPanel.add(absolutePositionParam);
	motorPanel.add(&relativeAngleInput);
	motorPanel.add(&absoluteAngleInput);
	motorPanel.add(&emergencyStopBtn);
	motorPanel.add(&setZeroBtn);
	motorPanel.add(&moveRelativeBtn);
	motorPanel.add(&moveAbsoluteBtn);
	motorPanel.add(&moveRelativeAngleBtn);
	motorPanel.add(&moveAbsoluteAngleBtn);

	// Update listeners for the newly selected hourglass
	updateListenersForCurrentHourglass();

	ofLogNotice("UIWrapper") << "UI panels rebound to HourGlass " << (currentHourGlass + 1) << ": " << hg->getName();
}

void UIWrapper::updateListenersForCurrentHourglass() {
	// Remove old listeners from ALL hourglasses first
	for (int i = 0; i < hourglassManager->getHourGlassCount(); i++) {
		auto * hg = hourglassManager->getHourGlass(i);
		if (hg) {
			hg->motorEnabled.removeListener(this, &UIWrapper::onMotorEnabledChanged);
			hg->microstep.removeListener(this, &UIWrapper::onMicrostepChanged);
			hg->motorSpeed.removeListener(this, &UIWrapper::onMotorSpeedChanged);
			hg->motorAcceleration.removeListener(this, &UIWrapper::onMotorAccelerationChanged);
			hg->gearRatio.removeListener(this, &UIWrapper::onGearRatioChanged);
			hg->calibrationFactor.removeListener(this, &UIWrapper::onCalibrationFactorChanged);
			hg->upLedColor.removeListener(this, &UIWrapper::onUpLedColorChanged);
			hg->downLedColor.removeListener(this, &UIWrapper::onDownLedColorChanged);
			hg->upMainLed.removeListener(this, &UIWrapper::onUpMainLedChanged);
			hg->downMainLed.removeListener(this, &UIWrapper::onDownMainLedChanged);
			hg->upPwm.removeListener(this, &UIWrapper::onUpPwmChanged);
			hg->downPwm.removeListener(this, &UIWrapper::onDownPwmChanged);
		}
	}

	// Add listeners to the currently selected hourglass
	auto * hg = hourglassManager->getHourGlass(currentHourGlass);
	if (hg) {
		hg->motorEnabled.addListener(this, &UIWrapper::onMotorEnabledChanged);
		hg->microstep.addListener(this, &UIWrapper::onMicrostepChanged);
		hg->motorSpeed.addListener(this, &UIWrapper::onMotorSpeedChanged);
		hg->motorAcceleration.addListener(this, &UIWrapper::onMotorAccelerationChanged);
		hg->gearRatio.addListener(this, &UIWrapper::onGearRatioChanged);
		hg->calibrationFactor.addListener(this, &UIWrapper::onCalibrationFactorChanged);
		hg->upLedColor.addListener(this, &UIWrapper::onUpLedColorChanged);
		hg->downLedColor.addListener(this, &UIWrapper::onDownLedColorChanged);
		hg->upMainLed.addListener(this, &UIWrapper::onUpMainLedChanged);
		hg->downMainLed.addListener(this, &UIWrapper::onDownMainLedChanged);
		hg->upPwm.addListener(this, &UIWrapper::onUpPwmChanged);
		hg->downPwm.addListener(this, &UIWrapper::onDownPwmChanged);
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
		hg->emergencyStop(); // This can remain direct as it's an immediate action
		hg->motorEnabled.set(false); // Update the parameter state
		ofLogNotice("UIWrapper") << "Emergency Stop pressed for " << hg->getName();
	}
}

void UIWrapper::onSetZeroPressed() {
	auto * hg = hourglassManager->getHourGlass(currentHourGlass);
	if (hg && hg->isConnected()) {
		hg->getMotor()->setZero(); // Direct, immediate action
		ofLogNotice("UIWrapper") << "Set Zero for " << hg->getName();
	}
}

void UIWrapper::onMoveRelativePressed() {
	auto * hg = hourglassManager->getHourGlass(currentHourGlass);
	if (hg && hg->isConnected()) {
		// Use the UI's current values for speed/accel from ofParameters if not overridden
		// The command methods now take std::optional for speed/accel
		hg->commandRelativeMove(relativePositionParam.get());
		ofLogNotice("UIWrapper") << "Commanded Relative Move: " << relativePositionParam.get() << " for " << hg->getName();
	}
}

void UIWrapper::onMoveAbsolutePressed() {
	auto * hg = hourglassManager->getHourGlass(currentHourGlass);
	if (hg && hg->isConnected()) {
		hg->commandAbsoluteMove(absolutePositionParam.get());
		ofLogNotice("UIWrapper") << "Commanded Absolute Move: " << absolutePositionParam.get() << " for " << hg->getName();
	}
}

void UIWrapper::onMoveRelativeAnglePressed() {
	auto * hg = hourglassManager->getHourGlass(currentHourGlass);
	if (hg && hg->isConnected()) {
		hg->commandRelativeAngle(static_cast<float>(relativeAngleParam.get()));
		ofLogNotice("UIWrapper") << "Commanded Relative Angle: " << relativeAngleParam.get() << " for " << hg->getName();
	}
}

void UIWrapper::onMoveAbsoluteAnglePressed() {
	auto * hg = hourglassManager->getHourGlass(currentHourGlass);
	if (hg && hg->isConnected()) {
		hg->commandAbsoluteAngle(static_cast<float>(absoluteAngleParam.get()));
		ofLogNotice("UIWrapper") << "Commanded Absolute Angle: " << absoluteAngleParam.get() << " for " << hg->getName();
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
	if (isUpdatingFromEffects) return;
	auto * hg = hourglassManager->getHourGlass(currentHourGlass);
	if (hg && !hg->updatingFromOSC) {
		hg->upLedColor.set(color);
		if (syncColorsParam.get()) {
			hg->downLedColor.set(color);
		}
	}
}

void UIWrapper::onDownLedColorChanged(ofColor & color) {
	if (isUpdatingFromEffects) return;
	auto * hg = hourglassManager->getHourGlass(currentHourGlass);
	if (hg && !hg->updatingFromOSC) {
		hg->downLedColor.set(color);
		if (syncColorsParam.get()) {
			hg->upLedColor.set(color);
		}
	}
}

void UIWrapper::onUpMainLedChanged(int & value) {
	auto * hg = hourglassManager->getHourGlass(currentHourGlass);
	if (hg && !hg->updatingFromOSC) {
		hg->upMainLed.set(value);
	}
}

void UIWrapper::onDownMainLedChanged(int & value) {
	auto * hg = hourglassManager->getHourGlass(currentHourGlass);
	if (hg && !hg->updatingFromOSC) {
		hg->downMainLed.set(value);
	}
}

void UIWrapper::onUpPwmChanged(int & value) {
	auto * hg = hourglassManager->getHourGlass(currentHourGlass);
	if (hg && !hg->updatingFromOSC) {
		hg->upPwm.set(value);
	}
}

void UIWrapper::onDownPwmChanged(int & value) {
	auto * hg = hourglassManager->getHourGlass(currentHourGlass);
	if (hg && !hg->updatingFromOSC) {
		hg->downPwm.set(value);
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
	ofLogNotice("UIWrapper") << "💾 Saving settings for all hourglasses...";

	// === Save UI State (current selection, global settings) ===
	ofXml uiStateConfig;
	auto uiStateNode = uiStateConfig.appendChild("UIState");

	// Save current hourglass selection
	uiStateNode.setAttribute("currentHourGlass", ofToString(currentHourGlass));
	uiStateNode.setAttribute("globalLuminosity", ofToString(LedMagnetController::getGlobalLuminosity()));
	uiStateNode.setAttribute("syncColors", syncColorsParam.get() ? "true" : "false");

	uiStateConfig.save("ui_state.xml");

	// === Save Per-HourGlass Settings ===
	ofXml hourglassSettingsConfig;
	auto hourglassesNode = hourglassSettingsConfig.appendChild("HourGlassSettings");

	for (size_t i = 0; i < hourglassManager->getHourGlassCount(); i++) {
		auto * hg = hourglassManager->getHourGlass(i);
		if (hg) {
			auto hgNode = hourglassesNode.appendChild("HourGlass");
			hgNode.setAttribute("index", ofToString(i));
			hgNode.setAttribute("name", hg->getName());

			// === Hardware Configuration ===
			auto hardwareNode = hgNode.appendChild("Hardware");
			hardwareNode.setAttribute("serialPort", hg->getSerialPort());
			hardwareNode.setAttribute("baudRate", ofToString(hg->getBaudRate()));
			hardwareNode.setAttribute("upLedId", ofToString(hg->getUpLedId()));
			hardwareNode.setAttribute("downLedId", ofToString(hg->getDownLedId()));
			hardwareNode.setAttribute("motorId", ofToString(hg->getMotorId()));

			// === LED Settings ===
			auto ledsNode = hgNode.appendChild("LEDs");
			auto upColor = hg->upLedColor.get();
			auto downColor = hg->downLedColor.get();

			ledsNode.setAttribute("upColorR", ofToString(upColor.r));
			ledsNode.setAttribute("upColorG", ofToString(upColor.g));
			ledsNode.setAttribute("upColorB", ofToString(upColor.b));
			ledsNode.setAttribute("upMainLed", ofToString(hg->upMainLed.get()));
			ledsNode.setAttribute("upPwm", ofToString(hg->upPwm.get()));
			ledsNode.setAttribute("upBlend", ofToString(hg->upLedBlend.get()));
			ledsNode.setAttribute("upOrigin", ofToString(hg->upLedOrigin.get()));
			ledsNode.setAttribute("upArc", ofToString(hg->upLedArc.get()));

			ledsNode.setAttribute("downColorR", ofToString(downColor.r));
			ledsNode.setAttribute("downColorG", ofToString(downColor.g));
			ledsNode.setAttribute("downColorB", ofToString(downColor.b));
			ledsNode.setAttribute("downMainLed", ofToString(hg->downMainLed.get()));
			ledsNode.setAttribute("downPwm", ofToString(hg->downPwm.get()));
			ledsNode.setAttribute("downBlend", ofToString(hg->downLedBlend.get()));
			ledsNode.setAttribute("downOrigin", ofToString(hg->downLedOrigin.get()));
			ledsNode.setAttribute("downArc", ofToString(hg->downLedArc.get()));

			ledsNode.setAttribute("individualLuminosity", ofToString(hg->individualLuminosity.get()));

			// === Motor Settings ===
			auto motorNode = hgNode.appendChild("Motor");
			motorNode.setAttribute("enabled", hg->motorEnabled.get() ? "true" : "false");
			motorNode.setAttribute("microstep", ofToString(hg->microstep.get()));
			motorNode.setAttribute("speed", ofToString(hg->motorSpeed.get()));
			motorNode.setAttribute("acceleration", ofToString(hg->motorAcceleration.get()));
			motorNode.setAttribute("gearRatio", ofToString(hg->gearRatio.get(), 6)); // 6 decimal precision for float
			motorNode.setAttribute("calibrationFactor", ofToString(hg->calibrationFactor.get(), 6)); // 6 decimal precision for float

			// === UI Parameters (position values, etc.) ===
			auto uiNode = hgNode.appendChild("UIParams");
			// Only save these for the currently selected hourglass to avoid confusion
			if (i == currentHourGlass) {
				uiNode.setAttribute("relativePosition", ofToString(relativePositionParam.get()));
				uiNode.setAttribute("absolutePosition", ofToString(absolutePositionParam.get()));
				uiNode.setAttribute("relativeAngle", ofToString(relativeAngleParam.get()));
				uiNode.setAttribute("absoluteAngle", ofToString(absoluteAngleParam.get()));
			}
		}
	}

	hourglassSettingsConfig.save("hourglass_settings.xml");

	// Also save the legacy panel files for backup compatibility
	settingsPanel.saveToFile("settings_actions.xml");
	luminosityPanel.saveToFile("luminosity.xml");
	effectsPanel.saveToFile("effects.xml");

	ofLogNotice("UIWrapper") << "💾 Settings saved: " << hourglassManager->getHourGlassCount() << " hourglasses, current selection: " << (currentHourGlass + 1);
}

void UIWrapper::loadSettings() {
	ofLogNotice("UIWrapper") << "📂 Loading settings for all hourglasses...";

	// === Load UI State (selection, global settings) ===
	ofXml uiStateConfig;
	if (uiStateConfig.load("ui_state.xml")) {
		auto uiStateNode = uiStateConfig.findFirst("UIState");
		if (uiStateNode) {
			// Restore global settings first
			float globalLum = ofToFloat(uiStateNode.getAttribute("globalLuminosity").getValue());
			if (globalLum > 0) {
				LedMagnetController::setGlobalLuminosity(globalLum);
				updateGlobalLuminositySlider(globalLum);
			}

			bool syncColors = (uiStateNode.getAttribute("syncColors").getValue() == "true");
			syncColorsParam.removeListener(this, &UIWrapper::syncColorsChanged);
			syncColorsParam.set(syncColors);
			syncColorsParam.addListener(this, &UIWrapper::syncColorsChanged);

			// Restore hourglass selection
			int savedSelection = ofToInt(uiStateNode.getAttribute("currentHourGlass").getValue());
			if (savedSelection >= 0 && savedSelection < hourglassManager->getHourGlassCount()) {
				currentHourGlass = savedSelection;
				hourglassSelectorParam.removeListener(this, &UIWrapper::hourglassSelectorChanged);
				hourglassSelectorParam.set(currentHourGlass + 1);
				hourglassSelectorParam.addListener(this, &UIWrapper::hourglassSelectorChanged);
				ofLogNotice("UIWrapper") << "📂 Restored hourglass selection: " << (currentHourGlass + 1);
			}
		}
	}

	// === Load Per-HourGlass Settings ===
	ofXml hourglassSettingsConfig;
	if (hourglassSettingsConfig.load("hourglass_settings.xml")) {
		auto hourglassesNode = hourglassSettingsConfig.findFirst("HourGlassSettings");
		if (hourglassesNode) {
			auto hourglassNodes = hourglassesNode.getChildren("HourGlass");

			for (auto & node : hourglassNodes) {
				int index = ofToInt(node.getAttribute("index").getValue());
				if (index >= 0 && index < hourglassManager->getHourGlassCount()) {
					auto * hg = hourglassManager->getHourGlass(index);
					if (hg) {
						// === Load Hardware Configuration ===
						auto hardwareNode = node.findFirst("Hardware");
						if (hardwareNode) {
							std::string serialPort = hardwareNode.getAttribute("serialPort").getValue();
							int baudRate = ofToInt(hardwareNode.getAttribute("baudRate").getValue());
							int upLedId = ofToInt(hardwareNode.getAttribute("upLedId").getValue());
							int downLedId = ofToInt(hardwareNode.getAttribute("downLedId").getValue());
							int motorId = ofToInt(hardwareNode.getAttribute("motorId").getValue());

							// Reconfigure the HourGlass with saved settings
							if (!serialPort.empty() && baudRate > 0) {
								hg->configure(serialPort, baudRate, upLedId, downLedId, motorId);
							}
						}

						// === Load LED Settings ===
						auto ledsNode = node.findFirst("LEDs");
						if (ledsNode) {
							// Temporarily disable OSC updates while loading
							hg->updatingFromOSC = true;

							// Up LED settings
							int upR = ofToInt(ledsNode.getAttribute("upColorR").getValue());
							int upG = ofToInt(ledsNode.getAttribute("upColorG").getValue());
							int upB = ofToInt(ledsNode.getAttribute("upColorB").getValue());
							hg->upLedColor.set(ofColor(upR, upG, upB));
							hg->upMainLed.set(ofToInt(ledsNode.getAttribute("upMainLed").getValue()));
							hg->upPwm.set(ofToInt(ledsNode.getAttribute("upPwm").getValue()));
							hg->upLedBlend.set(ofToInt(ledsNode.getAttribute("upBlend").getValue()));
							hg->upLedOrigin.set(ofToInt(ledsNode.getAttribute("upOrigin").getValue()));
							hg->upLedArc.set(ofToInt(ledsNode.getAttribute("upArc").getValue()));

							// Down LED settings
							int downR = ofToInt(ledsNode.getAttribute("downColorR").getValue());
							int downG = ofToInt(ledsNode.getAttribute("downColorG").getValue());
							int downB = ofToInt(ledsNode.getAttribute("downColorB").getValue());
							hg->downLedColor.set(ofColor(downR, downG, downB));
							hg->downMainLed.set(ofToInt(ledsNode.getAttribute("downMainLed").getValue()));
							hg->downPwm.set(ofToInt(ledsNode.getAttribute("downPwm").getValue()));
							hg->downLedBlend.set(ofToInt(ledsNode.getAttribute("downBlend").getValue()));
							hg->downLedOrigin.set(ofToInt(ledsNode.getAttribute("downOrigin").getValue()));
							hg->downLedArc.set(ofToInt(ledsNode.getAttribute("downArc").getValue()));

							// Individual luminosity
							float indivLum = ofToFloat(ledsNode.getAttribute("individualLuminosity").getValue());
							if (indivLum > 0) {
								hg->individualLuminosity.set(indivLum);
							}

							hg->updatingFromOSC = false;
						}

						// === Load Motor Settings ===
						auto motorNode = node.findFirst("Motor");
						if (motorNode) {
							hg->updatingFromOSC = true;

							hg->motorEnabled.set(motorNode.getAttribute("enabled").getValue() == "true");
							hg->microstep.set(ofToInt(motorNode.getAttribute("microstep").getValue()));
							hg->motorSpeed.set(ofToInt(motorNode.getAttribute("speed").getValue()));
							hg->motorAcceleration.set(ofToInt(motorNode.getAttribute("acceleration").getValue()));
							hg->gearRatio.set(ofToFloat(motorNode.getAttribute("gearRatio").getValue()));
							hg->calibrationFactor.set(ofToFloat(motorNode.getAttribute("calibrationFactor").getValue()));

							hg->updatingFromOSC = false;
						}

						// === Load UI Parameters (only for current selection) ===
						if (index == currentHourGlass) {
							auto uiNode = node.findFirst("UIParams");
							if (uiNode) {
								relativePositionParam.set(ofToInt(uiNode.getAttribute("relativePosition").getValue()));
								absolutePositionParam.set(ofToInt(uiNode.getAttribute("absolutePosition").getValue()));
								relativeAngleParam.set(ofToInt(uiNode.getAttribute("relativeAngle").getValue()));
								absoluteAngleParam.set(ofToInt(uiNode.getAttribute("absoluteAngle").getValue()));
							}
						}

						ofLogNotice("UIWrapper") << "📂 Loaded settings for HG " << (index + 1) << ": " << hg->getName();
					}
				}
			}
		}
	}

	// Load legacy panel files for any remaining UI elements
	settingsPanel.loadFromFile("settings_actions.xml");
	luminosityPanel.loadFromFile("luminosity.xml");
	effectsPanel.loadFromFile("effects.xml");

	// CRITICAL: Update UI panels to show the loaded hourglass settings
	updateUIPanelsBinding();

	// Update individual luminosity slider to reflect current hourglass
	auto * hg = hourglassManager->getHourGlass(currentHourGlass);
	if (hg) {
		updateCurrentIndividualLuminositySlider(hg->individualLuminosity.get());
	}

	ofLogNotice("UIWrapper") << "📂 Settings loaded for " << hourglassManager->getHourGlassCount() << " hourglasses, current selection: " << (currentHourGlass + 1);
}

// Implement the new listener method
void UIWrapper::onGlobalLuminosityChanged(float & luminosity) {
	LedMagnetController::setGlobalLuminosity(luminosity);
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
	HourGlass * hg = hourglassManager->getHourGlass(currentHourGlass);
	if (hg && !hg->updatingFromOSC) {
		hg->upLedBlend.set(value);
	}
	if (syncColorsParam.get()) {
		if (hg && !hg->updatingFromOSC) {
			hg->downLedBlend.set(value);
		}
		downLedBlendParam.removeListener(this, &UIWrapper::onDownLedBlendChanged);
		downLedBlendParam.set(value);
		downLedBlendParam.addListener(this, &UIWrapper::onDownLedBlendChanged);
	}
}

void UIWrapper::onUpLedOriginChanged(int & value) {
	HourGlass * hg = hourglassManager->getHourGlass(currentHourGlass);
	if (hg && !hg->updatingFromOSC) {
		hg->upLedOrigin.set(value);
	}
	if (syncColorsParam.get()) {
		if (hg && !hg->updatingFromOSC) {
			hg->downLedOrigin.set(value);
		}
		downLedOriginParam.removeListener(this, &UIWrapper::onDownLedOriginChanged);
		downLedOriginParam.set(value);
		downLedOriginParam.addListener(this, &UIWrapper::onDownLedOriginChanged);
	}
}

void UIWrapper::onUpLedArcChanged(int & value) {
	HourGlass * hg = hourglassManager->getHourGlass(currentHourGlass);
	if (hg && !hg->updatingFromOSC) {
		hg->upLedArc.set(value);
	}
	if (syncColorsParam.get()) {
		if (hg && !hg->updatingFromOSC) {
			hg->downLedArc.set(value);
		}
		downLedArcParam.removeListener(this, &UIWrapper::onDownLedArcChanged);
		downLedArcParam.set(value);
		downLedArcParam.addListener(this, &UIWrapper::onDownLedArcChanged);
	}
}

void UIWrapper::onDownLedBlendChanged(int & value) {
	HourGlass * hg = hourglassManager->getHourGlass(currentHourGlass);
	if (hg && !hg->updatingFromOSC) {
		hg->downLedBlend.set(value);
	}
	if (syncColorsParam.get()) {
		if (hg && !hg->updatingFromOSC) {
			hg->upLedBlend.set(value);
		}
		upLedBlendParam.removeListener(this, &UIWrapper::onUpLedBlendChanged);
		upLedBlendParam.set(value);
		upLedBlendParam.addListener(this, &UIWrapper::onUpLedBlendChanged);
	}
}

void UIWrapper::onDownLedOriginChanged(int & value) {
	HourGlass * hg = hourglassManager->getHourGlass(currentHourGlass);
	if (hg && !hg->updatingFromOSC) {
		hg->downLedOrigin.set(value);
	}
	if (syncColorsParam.get()) {
		if (hg && !hg->updatingFromOSC) {
			hg->upLedOrigin.set(value);
		}
		upLedOriginParam.removeListener(this, &UIWrapper::onUpLedOriginChanged);
		upLedOriginParam.set(value);
		upLedOriginParam.addListener(this, &UIWrapper::onUpLedOriginChanged);
	}
}

void UIWrapper::onDownLedArcChanged(int & value) {
	HourGlass * hg = hourglassManager->getHourGlass(currentHourGlass);
	if (hg && !hg->updatingFromOSC) {
		hg->downLedArc.set(value);
	}
	if (syncColorsParam.get()) {
		if (hg && !hg->updatingFromOSC) {
			hg->upLedArc.set(value);
		}
		upLedArcParam.removeListener(this, &UIWrapper::onUpLedArcChanged);
		upLedArcParam.set(value);
		upLedArcParam.addListener(this, &UIWrapper::onUpLedArcChanged);
	}
}

void UIWrapper::onAddCosineArcEffectPressed() {
	if (!hourglassManager) return;
	HourGlass * hg = hourglassManager->getHourGlass(currentHourGlass);
	if (hg) {
		// Add to UP controller
		auto upEffect = std::make_unique<ArcCosineEffect>(90.0f, 270.0f, 5.0f); // Example params
		hg->addUpEffect(std::move(upEffect));
		// ofLogNotice("UIWrapper") << "Added CosineArcEffect to UP for HG " << (currentHourGlass + 1); // COMMENTED BACK

		// Add to DOWN controller
		auto downEffect = std::make_unique<ArcCosineEffect>(45.0f, 315.0f, 3.5f); // Example params
		hg->addDownEffect(std::move(downEffect));
		// ofLogNotice("UIWrapper") << "Added CosineArcEffect to DOWN for HG " << (currentHourGlass + 1); // COMMENTED BACK
	} else {
		ofLogWarning("UIWrapper") << "No HourGlass selected to add effect to.";
	}
}

void UIWrapper::onClearAllEffectsPressed() {
	if (!hourglassManager) return;
	HourGlass * hg = hourglassManager->getHourGlass(currentHourGlass);
	if (hg) {
		hg->clearUpEffects();
		hg->clearDownEffects();
		// ofLogNotice("UIWrapper") << "Cleared all effects for HG " << (currentHourGlass + 1); // COMMENTED BACK
	} else {
		ofLogWarning("UIWrapper") << "No HourGlass selected to clear effects from.";
	}
}

void UIWrapper::updateUpLedBlendFromOSC(int value) {
	// Check if the UI is currently displaying the HourGlass that OSC is controlling.
	// This check is implicitly handled by OSCController before calling these.
	upLedBlendParam.removeListener(this, &UIWrapper::onUpLedBlendChanged);
	upLedBlendParam.set(value);
	upLedBlendParam.addListener(this, &UIWrapper::onUpLedBlendChanged);
}

void UIWrapper::updateUpLedOriginFromOSC(int value) {
	upLedOriginParam.removeListener(this, &UIWrapper::onUpLedOriginChanged);
	upLedOriginParam.set(value);
	upLedOriginParam.addListener(this, &UIWrapper::onUpLedOriginChanged);
}

void UIWrapper::updateUpLedArcFromOSC(int value) {
	upLedArcParam.removeListener(this, &UIWrapper::onUpLedArcChanged);
	upLedArcParam.set(value);
	upLedArcParam.addListener(this, &UIWrapper::onUpLedArcChanged);
}

void UIWrapper::updateDownLedBlendFromOSC(int value) {
	downLedBlendParam.removeListener(this, &UIWrapper::onDownLedBlendChanged);
	downLedBlendParam.set(value);
	downLedBlendParam.addListener(this, &UIWrapper::onDownLedBlendChanged);
}

void UIWrapper::updateDownLedOriginFromOSC(int value) {
	downLedOriginParam.removeListener(this, &UIWrapper::onDownLedOriginChanged);
	downLedOriginParam.set(value);
	downLedOriginParam.addListener(this, &UIWrapper::onDownLedOriginChanged);
}

void UIWrapper::updateDownLedArcFromOSC(int value) {
	downLedArcParam.removeListener(this, &UIWrapper::onDownLedArcChanged);
	downLedArcParam.set(value);
	downLedArcParam.addListener(this, &UIWrapper::onDownLedArcChanged);
}

void UIWrapper::onSetZeroAllPressed() {
	ofLogNotice("UIWrapper") << "Set Zero ALL Motors pressed";
	if (hourglassManager) {
		for (int i = 0; i < hourglassManager->getHourGlassCount(); ++i) {
			HourGlass * hg = hourglassManager->getHourGlass(i);
			if (hg && hg->isConnected()) {
				hg->setMotorZero(); // Call the HourGlass method
			}
		}
	}
}
