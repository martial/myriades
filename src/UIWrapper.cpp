#include "UIWrapper.h"
#include "ArcCosineEffect.h"
#include "OSCController.h"
#include "ofMain.h"

// OSC activity tracking constants
const float UIWrapper::OSC_ACTIVITY_FADE_TIME = 1.5f; // Dot visible for 1.5 seconds

UIWrapper::UIWrapper()
	: currentViewMode(DETAILED_VIEW)
	, oscControllerInstance(nullptr)
	, hourglassManager(nullptr)
	, currentHourGlass(0)
	, isUpdatingFromEffects(false)
	, lastOSCMessageTime(0)
// , lastColorUpdateTime(0) // This member was commented out in UIWrapper.h
{
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

	// Debug: Check parameter values (Removed unused debugFrameCount)
	// static int debugFrameCount = 0; // Removed
	// debugFrameCount++; // Removed

	// Remove the debug logging section completely
}

void UIWrapper::draw() {
	if (currentViewMode == DETAILED_VIEW) {
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
	} else if (currentViewMode == MINIMAL_VIEW) {
		drawMinimalView();
	}
}

void UIWrapper::drawMinimalView() {
	// Placeholder implementation
	ofSetBackgroundColor(0); // Black background for minimal view
	// ofSetColor(255); // Color will be set by individual HourGlass::drawMinimal calls
	// ofDrawBitmapString("MINIMAL VIEW - TODO: Implement HourGlass::drawMinimal() and call it here for each HG", 20, 20);

	if (hourglassManager) {
		int yOffset = 20; // Initial Y offset
		int xOffset = 20; // Initial X offset
		int columnWidth = 280; // Estimated width of one minimal HG view, adjust as needed
		int rowHeight = 150; // Estimated height of one minimal HG view, adjust as needed
		int maxCols = ofGetWidth() / columnWidth;
		int currentCol = 0;

		for (int i = 0; i < hourglassManager->getHourGlassCount(); ++i) {
			HourGlass * hg = hourglassManager->getHourGlass(i);
			if (hg) {
				hg->drawMinimal(xOffset + (currentCol * columnWidth), yOffset);
				currentCol++;
				if (currentCol >= maxCols) {
					currentCol = 0;
					yOffset += rowHeight;
				}
			}
		}
	}
}

// --- Helper method for styled panel setup ---
void UIWrapper::setupStyledPanel(ofxPanel & panel, const std::string & panelName, const std::string & settingsFilename, float x, float y) {
	panel.setup(panelName, settingsFilename, x, y);
	panel.setDefaultBackgroundColor(ofColor(20, 20, 20, 180));
	panel.setDefaultFillColor(ofColor(60, 60, 60, 160));
	panel.setDefaultHeaderBackgroundColor(ofColor(40, 40, 40, 200));
	panel.setDefaultTextColor(ofColor(255, 255, 255));
}

void UIWrapper::setupPanels() {
	// Calculate panel dimensions for 5 horizontal panels - made even more compact
	int panelWidth = 225;
	// int panelHeight = 300; // Removed: (Unused variable)
	int panelSpacing = 15;
	int startX = 15;
	int startY = 20;

	// === PANEL 1: SETTINGS & ACTIONS (Left) ===
	setupStyledPanel(settingsPanel, "SETTINGS & ACTIONS", "settings_actions.xml", startX, startY);

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
	setupStyledPanel(motorPanel, "MOTOR", "motor.xml", startX + (panelWidth + panelSpacing) * 1, startY);

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
	setupStyledPanel(ledUpPanel, "UP LED CONTROLLER", "led_up.xml", startX + (panelWidth + panelSpacing) * 2, startY);

	// NOTE: LED panel contents will be populated by updateUIPanelsBinding()
	// Up LED Effect Parameters are initialized with names and values in UIWrapper.h
	// No need to .set() them here again unless overriding defaults for a specific reason.

	// === PANEL 4: DOWN LED CONTROLLER (Center-Right) ===
	setupStyledPanel(ledDownPanel, "DOWN LED CONTROLLER", "led_down.xml", startX + (panelWidth + panelSpacing) * 3, startY);

	// NOTE: LED panel contents will be populated by updateUIPanelsBinding()
	// Down LED Effect Parameters are initialized with names and values in UIWrapper.h

	// === PANEL 5: MODULE LUMINOSITY (Right) ===
	setupStyledPanel(luminosityPanel, "MODULE LUMINOSITY", "luminosity.xml", startX + (panelWidth + panelSpacing) * 4, startY);

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
	setupStyledPanel(effectsPanel, "EFFECTS", "effects.xml", effectsPanelX, startY);

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

	// Listeners for LED effect parameters (Blend, Origin, Arc) - Reverted to original individual listeners
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

// --- Helper methods for drawing status sections ---
void UIWrapper::drawStatusSection_HourglassInfo(HourGlass * hg, float x, float & y, float lineHeight, float sectionWidth) {
	ofSetColor(255);
	ofDrawBitmapString("HOURGLASS STATUS", x, y);
	y += lineHeight * 1.5f; // Space after title

	if (hg) {
		ofSetColor(220, 220, 220);
		ofDrawBitmapString("HG " + ofToString(currentHourGlass + 1) + ": " + hg->getName(), x, y);
		y += lineHeight;
		ofDrawBitmapString(std::string("Status: ") + (hg->isConnected() ? "Connected" : "Disconnected"), x, y);
		y += lineHeight;
		ofSetColor(hg->motorEnabled.get() ? ofColor::green : ofColor::red);
		ofDrawBitmapString(std::string("Motor: ") + (hg->motorEnabled.get() ? "Enabled" : "Disabled"), x, y);
		y += lineHeight * 1.5f; // Extra space after HG info
	} else {
		ofSetColor(ofColor::orangeRed);
		ofDrawBitmapString("No HourGlass selected or available.", x, y);
		y += lineHeight * 1.5f;
	}
}

void UIWrapper::drawStatusSection_KeyboardShortcuts(float x, float & y, float lineHeight, float sectionWidth) {
	ofSetColor(100);
	ofDrawLine(x, y, x + sectionWidth, y); // Separator line
	y += lineHeight * 1.5f; // Space after separator

	ofSetColor(220, 220, 220);
	ofDrawBitmapString("Keyboard Shortcuts:", x, y);
	y += lineHeight; // Start shortcuts on the next line

	std::vector<std::string> shortcuts = {
		"1-2: Select HG", "c/x: Connect/Disconnect",
		"z: Zero Motor", "s: Stop Motor (Emerg.)",
		"Arrows: Rotate Motor", "u/d: Move Steps (Rel)"
	};
	for (const auto & shortcut : shortcuts) {
		ofDrawBitmapString(shortcut, x, y);
		y += lineHeight;
	}
}

void UIWrapper::drawStatusSection_OSC(float x, float & y, float lineHeight) {
	y += lineHeight; // Space before separator from previous content
	y += 30; // Additional fixed space, adjust as needed

	ofSetColor(100);
	ofDrawLine(x, y - lineHeight, x + 100, y - lineHeight); // Adjust width of separator as needed, e.g., (rightSectionWidth - 60)

	ofSetColor(220, 220, 220);
	y += lineHeight; // Position for OSC text
	ofDrawBitmapString("OSC:", x, y);
	float timeSinceLastOSC = ofGetElapsedTimef() - lastOSCMessageTime;
	if (timeSinceLastOSC < OSC_ACTIVITY_FADE_TIME) {
		float alpha = ofMap(timeSinceLastOSC, 0, OSC_ACTIVITY_FADE_TIME, 255, 0);
		ofSetColor(0, 255, 0, alpha);
	} else {
		ofSetColor(100);
	}
	ofDrawCircle(x + 100, y - lineHeight * 0.3f + 2, 4); // Adjusted position and size
}

void UIWrapper::drawStatus() {
	int panelWidth = 225;
	int panelSpacing = 15;
	int startX = 15;
	int statusY = 420;
	int statusWidth = (panelWidth * 5) + (panelSpacing * 4);
	int leftSectionWidth = statusWidth * 0.65;
	int textX = startX + 15;
	int rightTextX = startX + leftSectionWidth + 20;
	int initialStatusContentY = statusY + 25;
	int lineHeight = 18;

	// Approximate content lines for height calculation (can be refined)
	int leftContentLines = 12;
	int rightContentLines = 11;
	int maxContentLines = std::max(leftContentLines, rightContentLines);
	int statusHeight = 15 + (maxContentLines * lineHeight) + 80;

	ofSetColor(40, 40, 45, 180);
	ofDrawRectRounded(startX, statusY, statusWidth - startX, statusHeight, 6);
	ofSetColor(60, 60, 60);
	ofDrawLine(startX + leftSectionWidth + 10, statusY + 10, startX + leftSectionWidth + 10, statusY + statusHeight - 10);

	// Prepare data for helpers
	auto * hg = hourglassManager->getHourGlass(currentHourGlass);

	float currentLeftY = initialStatusContentY;
	float currentRightY = initialStatusContentY;

	// === LEFT SECTION ===
	drawStatusSection_HourglassInfo(hg, textX, currentLeftY, lineHeight, leftSectionWidth - 30);
	drawStatusSection_KeyboardShortcuts(textX, currentLeftY, lineHeight, leftSectionWidth - 30);

	// === RIGHT SECTION ===
	drawStatusSection_OSC(rightTextX, currentRightY, lineHeight);
}

// Keyboard handling
void UIWrapper::handleKeyPressed(int key) {
	handleHourGlassSelection(key);
	handleConnectionCommands(key);
	handleMotorCommands(key);
	handleLEDCommands(key);
	handleViewToggle(key);
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

	switch (key) {
	case 'o': // LEDs off
		// Use safe parameter setting instead of direct hardware access
		hg->upLedColor.set(ofColor::black);
		hg->downLedColor.set(ofColor::black);
		setColorPreset(ofColor::black);
		break;
	}

	// Handle save shortcut separately to check for modifier keys
	if (key == 's' && (ofGetKeyPressed(OF_KEY_COMMAND) || ofGetKeyPressed(OF_KEY_CONTROL))) {
		saveSettings();
	}
}

void UIWrapper::handleViewToggle(int key) {
	if (key == 'v') { // 'v' for view toggle
		if (currentViewMode == DETAILED_VIEW) {
			currentViewMode = MINIMAL_VIEW;
			ofLogNotice("UIWrapper") << "Switched to Minimal View";
		} else {
			currentViewMode = DETAILED_VIEW;
			ofLogNotice("UIWrapper") << "Switched to Detailed View";
		}
	}
}

// GUI Event Handlers
void UIWrapper::hourglassSelectorChanged(int & selection) {
	currentHourGlass = selection - 1;
	ofLogNotice("UIWrapper") << "Selected HourGlass: " << selection;

	auto * hg = hourglassManager->getHourGlass(currentHourGlass);
	if (hg) {
		// CRITICAL: Update UI panels to bind to the newly selected hourglass's state
		// This will also handle syncing UIWrapper's parameters like upLedBlendParam, etc.
		updateUIPanelsBinding();
	} else {
		// Optionally disable or set to a default if no valid HG is selected
		// For parameters like currentHgIndividualLuminosityParam
		currentHgIndividualLuminosityParam.removeListener(this, &UIWrapper::onIndividualLuminosityChanged);
		currentHgIndividualLuminosityParam.set(1.0f); // Default to full if HG invalid
		currentHgIndividualLuminosityParam.addListener(this, &UIWrapper::onIndividualLuminosityChanged);
		// Similar for blend/origin/arc if a default "no hg selected" state is desired for their sliders
		// For now, they will retain the values from the last valid hg, or their initial values.
		// updateUIPanelsBinding() handles clearing/rebuilding panels, which is often sufficient visual feedback.
	}
}

void UIWrapper::updateUIPanelsBinding() {
	auto * hg = hourglassManager->getHourGlass(currentHourGlass);
	if (!hg) return;

	// Set the flag to prevent listeners from acting during this batch update from HG state
	isInternallySyncing = true;

	// Update UIWrapper's parameters from the HourGlass state.
	// Their listeners WILL fire, but the generic handler (and others if modified)
	// should exit early due to isInternallySyncing == true.
	upLedBlendParam.set(hg->upLedBlend.get());
	upLedOriginParam.set(hg->upLedOrigin.get());
	upLedArcParam.set(hg->upLedArc.get());

	downLedBlendParam.set(hg->downLedBlend.get());
	downLedOriginParam.set(hg->downLedOrigin.get());
	downLedArcParam.set(hg->downLedArc.get());

	// Also update the individual luminosity UI parameter
	// Assuming onIndividualLuminosityChanged also checks isInternallySyncing or hg->updatingFromOSC
	currentHgIndividualLuminosityParam.set(hg->individualLuminosity.get());

	// Clear the flag once all relevant UI parameters are synced from HourGlass state
	isInternallySyncing = false;

	// Clear and rebuild panels with correct hourglass bindings
	// Note: The .add() method for panels will use the current values of the parameters.
	ledUpPanel.clear();
	ledDownPanel.clear();
	motorPanel.clear(); // Also clear motor panel to re-add its specific elements like buttons

	// === UP LED PANEL ===
	ledUpPanel.add(hg->upLedColor.set("RGB Color", hg->upLedColor.get())); // Uses HG's param directly
	ledUpPanel.add(hg->upMainLed.set("Main LED", hg->upMainLed.get(), 0, 255)); // Uses HG's param directly
	ledUpPanel.add(hg->upPwm.set("PWM", hg->upPwm.get(), 0, 255)); // Uses HG's param directly
	ledUpPanel.add(upLedBlendParam); // Uses UIWrapper's param (now synced)
	ledUpPanel.add(upLedOriginParam); // Uses UIWrapper's param (now synced)
	ledUpPanel.add(upLedArcParam); // Uses UIWrapper's param (now synced)
	ledUpPanel.getGroup("RGB Color").maximize();

	// === DOWN LED PANEL ===
	ledDownPanel.add(hg->downLedColor.set("RGB Color", hg->downLedColor.get())); // Uses HG's param directly
	ledDownPanel.add(hg->downMainLed.set("Main LED", hg->downMainLed.get(), 0, 255)); // Uses HG's param directly
	ledDownPanel.add(hg->downPwm.set("PWM", hg->downPwm.get(), 0, 255)); // Uses HG's param directly
	ledDownPanel.add(downLedBlendParam); // Uses UIWrapper's param (now synced)
	ledDownPanel.add(downLedOriginParam); // Uses UIWrapper's param (now synced)
	ledDownPanel.add(downLedArcParam); // Uses UIWrapper's param (now synced)
	ledDownPanel.getGroup("RGB Color").maximize();

	// === MOTOR PANEL ===
	// Motor parameters are generally direct from HourGlass or are UI inputs/buttons
	motorPanel.add(hg->motorEnabled);
	motorPanel.add(hg->microstep);
	motorPanel.add(hg->motorSpeed);
	motorPanel.add(hg->motorAcceleration);
	gearRatioInput.setup(hg->gearRatio); // setup re-binds if necessary
	calibrationFactorInput.setup(hg->calibrationFactor); // setup re-binds if necessary
	motorPanel.add(&gearRatioInput);
	motorPanel.add(&calibrationFactorInput);

	// UI Input parameters (these don't need to be set from hg, they are inputs for actions)
	motorPanel.add(relativePositionParam);
	motorPanel.add(absolutePositionParam);
	motorPanel.add(&relativeAngleInput); // Assuming relativeAngleInput is setup with relativeAngleParam
	motorPanel.add(&absoluteAngleInput); // Assuming absoluteAngleInput is setup with absoluteAngleParam

	// Buttons
	motorPanel.add(&emergencyStopBtn);
	motorPanel.add(&setZeroBtn);
	motorPanel.add(&moveRelativeBtn);
	motorPanel.add(&moveAbsoluteBtn);
	motorPanel.add(&moveRelativeAngleBtn);
	motorPanel.add(&moveAbsoluteAngleBtn);

	// LUMINOSITY PANEL (already has its own logic for currentHgIndividualLuminosityParam syncing)
	// We added explicit sync for currentHgIndividualLuminosityParam at the top of this function.
	// The luminosityPanel itself likely adds currentHgIndividualLuminositySlider which is bound to currentHgIndividualLuminosityParam.

	// Update listeners for the newly selected hourglass (for parameters directly from HourGlass)
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
		hg->setMotorZero(); // Use HourGlass method (handles OSC output)
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

		// Use safe parameter setting instead of dangerous direct hardware access
		// These will be applied by applyLedParameters() in the update loop
		hg->upLedColor.set(ofColor::black);
		hg->downLedColor.set(ofColor::black);
		hg->upMainLed.set(0);
		hg->downMainLed.set(0);
		hg->upPwm.set(0);
		hg->downPwm.set(0);

		ofLogNotice("UIWrapper") << "ALL OFF - All parameters set to 0 (RGB, Main LEDs, PWM)";

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
		// Use safe parameter setting instead of dangerous direct hardware access
		if (syncColorsParam.get()) {
			hg->upLedColor.set(ofColor::black);
			hg->downLedColor.set(ofColor::black);
			ofLogNotice("UIWrapper") << "LEDs Off - BOTH LEDs set to black";
		} else {
			hg->upLedColor.set(ofColor::black);
			ofLogNotice("UIWrapper") << "LEDs Off - UP LED set to black";
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

// --- XML Serialization Helper Implementations ---
void UIWrapper::saveHourGlassToXml(ofXml & hgNode, HourGlass * hg, int hgIndex) {
	if (!hg) return;

	hgNode.setAttribute("index", ofToString(hgIndex));
	hgNode.setAttribute("name", hg->getName());

	// === Hardware Configuration ===
	auto hardwareNode = hgNode.appendChild("Hardware");
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
	motorNode.setAttribute("gearRatio", ofToString(hg->gearRatio.get(), 6));
	motorNode.setAttribute("calibrationFactor", ofToString(hg->calibrationFactor.get(), 6));

	// === UI Parameters (only for current selection) ===
	if (hgIndex == currentHourGlass) {
		auto uiNode = hgNode.appendChild("UIParams");
		uiNode.setAttribute("relativePosition", ofToString(relativePositionParam.get()));
		uiNode.setAttribute("absolutePosition", ofToString(absolutePositionParam.get()));
		uiNode.setAttribute("relativeAngle", ofToString(relativeAngleParam.get()));
		uiNode.setAttribute("absoluteAngle", ofToString(absoluteAngleParam.get()));
	}
}

void UIWrapper::loadHourGlassFromXml(const ofXml & hgNode, HourGlass * hg, int hgIndex) {
	if (!hg) return;

	// === Load Hardware Configuration ===
	auto hardwareNode = hgNode.findFirst("Hardware");
	if (hardwareNode) {
		int upLedId = ofToInt(hardwareNode.getAttribute("upLedId").getValue());
		int downLedId = ofToInt(hardwareNode.getAttribute("downLedId").getValue());
		int motorId = ofToInt(hardwareNode.getAttribute("motorId").getValue());
		// Configure with empty serial port for OSC-only mode
		hg->configure("", 0, upLedId, downLedId, motorId);
	}

	hg->updatingFromOSC = true; // Prevent listeners from firing during bulk load

	// === Load LED Settings ===
	auto ledsNode = hgNode.findFirst("LEDs");
	if (ledsNode) {
		int upR = ofToInt(ledsNode.getAttribute("upColorR").getValue());
		int upG = ofToInt(ledsNode.getAttribute("upColorG").getValue());
		int upB = ofToInt(ledsNode.getAttribute("upColorB").getValue());
		hg->upLedColor.set(ofColor(upR, upG, upB));
		hg->upMainLed.set(ofToInt(ledsNode.getAttribute("upMainLed").getValue()));
		hg->upPwm.set(ofToInt(ledsNode.getAttribute("upPwm").getValue()));
		hg->upLedBlend.set(ofToInt(ledsNode.getAttribute("upBlend").getValue()));
		hg->upLedOrigin.set(ofToInt(ledsNode.getAttribute("upOrigin").getValue()));
		hg->upLedArc.set(ofToInt(ledsNode.getAttribute("upArc").getValue()));

		int downR = ofToInt(ledsNode.getAttribute("downColorR").getValue());
		int downG = ofToInt(ledsNode.getAttribute("downColorG").getValue());
		int downB = ofToInt(ledsNode.getAttribute("downColorB").getValue());
		hg->downLedColor.set(ofColor(downR, downG, downB));
		hg->downMainLed.set(ofToInt(ledsNode.getAttribute("downMainLed").getValue()));
		hg->downPwm.set(ofToInt(ledsNode.getAttribute("downPwm").getValue()));
		hg->downLedBlend.set(ofToInt(ledsNode.getAttribute("downBlend").getValue()));
		hg->downLedOrigin.set(ofToInt(ledsNode.getAttribute("downOrigin").getValue()));
		hg->downLedArc.set(ofToInt(ledsNode.getAttribute("downArc").getValue()));

		float indivLum = ofToFloat(ledsNode.getAttribute("individualLuminosity").getValue());
		if (indivLum >= 0.0f && indivLum <= 1.0f) { // Basic validation for luminosity
			hg->individualLuminosity.set(indivLum);
		} else {
			hg->individualLuminosity.set(1.0f); // Default if invalid
		}
	}

	// === Load Motor Settings ===
	auto motorNode = hgNode.findFirst("Motor");
	if (motorNode) {
		hg->motorEnabled.set(motorNode.getAttribute("enabled").getValue() == "true");
		hg->microstep.set(ofToInt(motorNode.getAttribute("microstep").getValue()));
		hg->motorSpeed.set(ofToInt(motorNode.getAttribute("speed").getValue()));
		hg->motorAcceleration.set(ofToInt(motorNode.getAttribute("acceleration").getValue()));
		hg->gearRatio.set(ofToFloat(motorNode.getAttribute("gearRatio").getValue()));
		hg->calibrationFactor.set(ofToFloat(motorNode.getAttribute("calibrationFactor").getValue()));
	}

	hg->updatingFromOSC = false; // Restore listener behavior

	// === Load UI Parameters (only for current selection) ===
	if (hgIndex == currentHourGlass) {
		auto uiNode = hgNode.findFirst("UIParams");
		if (uiNode) {
			// Set UIWrapper's parameters directly, not hg's params for these
			isInternallySyncing = true; // Prevent UI listeners from re-updating hg or causing feedback
			relativePositionParam.set(ofToInt(uiNode.getAttribute("relativePosition").getValue()));
			absolutePositionParam.set(ofToInt(uiNode.getAttribute("absolutePosition").getValue()));
			relativeAngleParam.set(ofToInt(uiNode.getAttribute("relativeAngle").getValue()));
			absoluteAngleParam.set(ofToInt(uiNode.getAttribute("absoluteAngle").getValue()));
			isInternallySyncing = false;
		}
	}
}

// XML save/load for persistence
void UIWrapper::saveSettings() {
	ofLogNotice("UIWrapper") << "💾 Saving settings for all hourglasses...";

	// === Save UI State (current selection, global settings) ===
	ofXml uiStateConfig;
	auto uiStateNode = uiStateConfig.appendChild("UIState");
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
			saveHourGlassToXml(hgNode, hg, i); // Use helper function
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
			float globalLum = ofToFloat(uiStateNode.getAttribute("globalLuminosity").getValue());
			if (globalLum >= 0.0f && globalLum <= 1.0f) { // Basic validation
				LedMagnetController::setGlobalLuminosity(globalLum);
				updateGlobalLuminositySlider(globalLum);
			}

			bool syncColors = (uiStateNode.getAttribute("syncColors").getValue() == "true");
			isInternallySyncing = true; // Prevent listener from firing during this set
			syncColorsParam.set(syncColors);
			isInternallySyncing = false;

			int savedSelection = ofToInt(uiStateNode.getAttribute("currentHourGlass").getValue());
			if (savedSelection >= 0 && savedSelection < hourglassManager->getHourGlassCount()) {
				currentHourGlass = savedSelection;
				isInternallySyncing = true; // Prevent listener from firing
				hourglassSelectorParam.set(currentHourGlass + 1);
				isInternallySyncing = false;
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
						loadHourGlassFromXml(node, hg, index); // Use helper function
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

	// CRITICAL: Update UI panels to show the loaded hourglass settings for the initially selected one
	updateUIPanelsBinding();

	// The individual luminosity slider is already updated within updateUIPanelsBinding
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
	if (hg && !hg->updatingFromOSC && !isInternallySyncing) {
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

// --- RE-IMPLEMENT original individual listener definitions ---
void UIWrapper::onUpLedBlendChanged(int & value) {
	HourGlass * hg = hourglassManager->getHourGlass(currentHourGlass);
	if (!hg || hg->updatingFromOSC || this->isInternallySyncing) return;

	hg->upLedBlend.set(value);

	if (syncColorsParam.get()) {
		this->isInternallySyncing = true;
		downLedBlendParam.set(value); // Update UIWrapper's param
		if (!hg->updatingFromOSC) { // Check again in case OSC updated hg in a weird timing
			hg->downLedBlend.set(value); // Update HourGlass's param
		}
		this->isInternallySyncing = false;
	}
}

void UIWrapper::onUpLedOriginChanged(int & value) {
	HourGlass * hg = hourglassManager->getHourGlass(currentHourGlass);
	if (!hg || hg->updatingFromOSC || this->isInternallySyncing) return;

	hg->upLedOrigin.set(value);

	if (syncColorsParam.get()) {
		this->isInternallySyncing = true;
		downLedOriginParam.set(value);
		if (!hg->updatingFromOSC) {
			hg->downLedOrigin.set(value);
		}
		this->isInternallySyncing = false;
	}
}

void UIWrapper::onUpLedArcChanged(int & value) {
	HourGlass * hg = hourglassManager->getHourGlass(currentHourGlass);
	if (!hg || hg->updatingFromOSC || this->isInternallySyncing) return;

	hg->upLedArc.set(value);

	if (syncColorsParam.get()) {
		this->isInternallySyncing = true;
		downLedArcParam.set(value);
		if (!hg->updatingFromOSC) {
			hg->downLedArc.set(value);
		}
		this->isInternallySyncing = false;
	}
}

void UIWrapper::onDownLedBlendChanged(int & value) {
	HourGlass * hg = hourglassManager->getHourGlass(currentHourGlass);
	if (!hg || hg->updatingFromOSC || this->isInternallySyncing) return;

	hg->downLedBlend.set(value);

	if (syncColorsParam.get()) {
		this->isInternallySyncing = true;
		upLedBlendParam.set(value);
		if (!hg->updatingFromOSC) {
			hg->upLedBlend.set(value);
		}
		this->isInternallySyncing = false;
	}
}

void UIWrapper::onDownLedOriginChanged(int & value) {
	HourGlass * hg = hourglassManager->getHourGlass(currentHourGlass);
	if (!hg || hg->updatingFromOSC || this->isInternallySyncing) return;

	hg->downLedOrigin.set(value);

	if (syncColorsParam.get()) {
		this->isInternallySyncing = true;
		upLedOriginParam.set(value);
		if (!hg->updatingFromOSC) {
			hg->upLedOrigin.set(value);
		}
		this->isInternallySyncing = false;
	}
}

void UIWrapper::onDownLedArcChanged(int & value) {
	HourGlass * hg = hourglassManager->getHourGlass(currentHourGlass);
	if (!hg || hg->updatingFromOSC || this->isInternallySyncing) return;

	hg->downLedArc.set(value);

	if (syncColorsParam.get()) {
		this->isInternallySyncing = true;
		upLedArcParam.set(value);
		if (!hg->updatingFromOSC) {
			hg->upLedArc.set(value);
		}
		this->isInternallySyncing = false;
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
	isInternallySyncing = true;
	upLedBlendParam.set(value);
	isInternallySyncing = false;
}

void UIWrapper::updateUpLedOriginFromOSC(int value) {
	isInternallySyncing = true;
	upLedOriginParam.set(value);
	isInternallySyncing = false;
}

void UIWrapper::updateUpLedArcFromOSC(int value) {
	isInternallySyncing = true;
	upLedArcParam.set(value);
	isInternallySyncing = false;
}

void UIWrapper::updateDownLedBlendFromOSC(int value) {
	isInternallySyncing = true;
	downLedBlendParam.set(value);
	isInternallySyncing = false;
}

void UIWrapper::updateDownLedOriginFromOSC(int value) {
	isInternallySyncing = true;
	downLedOriginParam.set(value);
	isInternallySyncing = false;
}

void UIWrapper::updateDownLedArcFromOSC(int value) {
	isInternallySyncing = true;
	downLedArcParam.set(value);
	isInternallySyncing = false;
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
