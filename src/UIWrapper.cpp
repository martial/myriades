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
