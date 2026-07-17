#include "UIWrapper.h"
#include "ArcCosineEffect.h"
#include "OSCController.h"
#include "ofMain.h"

// OSC activity tracking constants
const float UIWrapper::OSC_ACTIVITY_FADE_TIME = 1.5f; // Dot visible for 1.5 seconds

// --- Layout & palette ("control-room amber") ---
// All positions derive from these + the window size; layoutPanels() is the only
// place that positions panels, so windowResized() can just call it again.
namespace {
constexpr int kMargin = 15;
constexpr int kSpacing = 12;
constexpr int kPanelWidth = 225;
constexpr int kPanelY = 20;
constexpr int kContentTop = 500; // below the tallest top panel (16-row MOTOR)
constexpr int kSequencerWidth = 300;
constexpr int kStatusBarHeight = 34;
constexpr int kEStopWidth = 140;
constexpr int kEStopHeight = 64;

const ofColor kPanelBg(16, 18, 22, 215);
const ofColor kHeaderBg(28, 32, 40, 235);
const ofColor kFillAmber(196, 134, 43, 190);
const ofColor kBorder(42, 47, 55, 200);
const ofColor kInk(233, 230, 222);
const ofColor kMuted(144, 150, 160);
const ofColor kAmber(226, 161, 60);
const ofColor kOk(83, 179, 107);
const ofColor kDanger(229, 72, 77);

// Medium weight + a larger raster size read far better than thin/10px: OF
// 0.12.1's ofxGui rasterizes TTF glyphs at logical size and the Retina
// framebuffer magnifies them 2x, so heavier ink and more source detail per
// glyph are the only real levers on perceived sharpness.
constexpr const char * kFontPath = "fonts/JetBrainsMono-Medium.ttf";

int panelX(int col) {
	return kMargin + col * (kPanelWidth + kSpacing);
}
}

UIWrapper::UIWrapper()
	: currentViewMode(DETAILED_VIEW)
	, oscControllerInstance(nullptr)
	, hourglassManager(nullptr)
	, currentHourGlass(0)
	, isUpdatingFromEffects(false)
	, lastOSCMessageTime(0) {
}

UIWrapper::~UIWrapper() {
	saveSettings();
}

void UIWrapper::setup(HourGlassManager * manager, OSCController * oscCtrl, VezerPlayer * player) {
	this->hourglassManager = manager;
	this->oscControllerInstance = oscCtrl;
	this->vezerPlayer = player;

	currentHourGlass = 0;

	// Global ofxGui style - must run before any panel/widget setup.
	// Full character set brings the ° glyph the bitmap font silently dropped.
	if (ofFile::doesFileExist(kFontPath)) {
		ofxBaseGui::loadFont(kFontPath, 13, true, true);
		statusFont.load(kFontPath, 13, true, true);
		estopFont.load(kFontPath, 16, true, true);
	} else {
		ofLogWarning("UIWrapper") << kFontPath << " missing - falling back to bitmap font";
	}
	ofxBaseGui::setDefaultWidth(kPanelWidth);
	ofxBaseGui::setDefaultHeight(24); // room for the 13px face
	ofxBaseGui::setDefaultBackgroundColor(kPanelBg);
	ofxBaseGui::setDefaultHeaderBackgroundColor(kHeaderBg);
	ofxBaseGui::setDefaultFillColor(kFillAmber);
	ofxBaseGui::setDefaultTextColor(kInk);
	ofxBaseGui::setDefaultBorderColor(kBorder);

	// Setup all panels
	setupPanels();
	setupSequencerPanel();

	// Setup all listeners
	setupListeners();

	// Setup LED visualizer - hero preview in the zone below the panels,
	// sized every frame from the window in draw()
	ledVisualizer.setup(600, 300);
	ledVisualizer.setLayoutMode(0);
	ledVisualizer.setShowLabels(true);
	ledVisualizer.setShowGrid(false);
	ledVisualizer.setUIWrapper(this); // Give visualizer access to the current selection
	ledVisualizer.setLabelFont(&statusFont);

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

	// Position everything for the current window size
	layoutPanels();
}

void UIWrapper::layoutPanels() {
	settingsPanel.setPosition(panelX(0), kPanelY);
	motorPanel.setPosition(panelX(1), kPanelY);
	ledUpPanel.setPosition(panelX(2), kPanelY);
	ledDownPanel.setPosition(panelX(3), kPanelY);
	luminosityPanel.setPosition(panelX(4), kPanelY);
	effectsPanel.setPosition(panelX(5), kPanelY);
	sequencerPanel.setPosition(ofGetWidth() - kMargin - kSequencerWidth, kContentTop);
	eStopRect.set(ofGetWidth() - kMargin - kEStopWidth, kPanelY, kEStopWidth, kEStopHeight);
}

void UIWrapper::windowResized(int, int) {
	layoutPanels();
}

void UIWrapper::update() {
	// Update LED visualizer
	ledVisualizer.update();

	// Reflect sequencer playback state in the GUI
	syncSequencerUI();

	// Per-frame hardware tick: effects, LED sends, pending motor commands
	if (hourglassManager) {
		hourglassManager->update(ofGetLastFrameTime());
	}
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
		sequencerPanel.draw();

		// Hero live preview: fills the zone between the panels and the status
		// bar, leaving the right column to the sequencer
		float vizW = ofGetWidth() - kMargin * 2 - kSequencerWidth - kSpacing;
		float vizH = ofGetHeight() - kContentTop - kStatusBarHeight - kMargin - 8;
		ledVisualizer.setSize(vizW, vizH);
		ledVisualizer.draw(kMargin, kContentTop);

		drawEStop();
		drawStatusBar();
		if (shortcutsVisible) {
			drawShortcutsOverlay();
		}
	} else if (currentViewMode == MINIMAL_VIEW) {
		drawMinimalView();
	}
}

void UIWrapper::drawMinimalView() {
	ofSetBackgroundColor(0); // Black background for minimal view

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
// (style itself is global, set once in setup())
void UIWrapper::setupStyledPanel(ofxPanel & panel, const std::string & panelName, const std::string & settingsFilename, float x, float y) {
	panel.setup(panelName, settingsFilename, x, y);
}

void UIWrapper::setupPanels() {
	// Six panels across the top; layoutPanels() owns the final positions
	int startY = kPanelY;

	// === PANEL 1: GLOBAL (Left) ===
	setupStyledPanel(settingsPanel, "GLOBAL", "settings_actions.xml", panelX(0), startY);

	// --- Settings Section ---
	// Selection state (not shown as a slider; driven by the HG buttons below and keys 1-9)
	hourglassSelectorParam.set("Select HourGlass", 1, 1, hourglassManager->getHourGlassCount() > 0 ? hourglassManager->getHourGlassCount() : 2);
	connectBtnParam.set("Connect All");
	disconnectBtnParam.set("Disconnect All");

	// One button per hourglass; the selected one is echoed in every panel header
	// and in the status bar
	int hgCount = std::max(1, (int)hourglassManager->getHourGlassCount());
	hgSelectParams.resize(hgCount);
	for (int i = 0; i < hgCount; i++) {
		hgSelectParams[i].set("Edit HG " + ofToString(i + 1));
		settingsPanel.add(hgSelectParams[i]);
		hgSelectListeners.push_back(hgSelectParams[i].newListener([this, i]() { selectHourglass(i); }));
	}

	connectBtn.setup(connectBtnParam.getName());
	disconnectBtn.setup(disconnectBtnParam.getName());
	settingsPanel.add(&connectBtn);
	settingsPanel.add(&disconnectBtn);

	// Global Luminosity (affects ALL modules)
	globalLuminosityParam.set("Global Luminosity", LedMagnetController::getGlobalLuminosity(), 0.0f, 1.0f);
	globalLuminositySlider.setup(globalLuminosityParam);
	settingsPanel.add(&globalLuminositySlider);

	// Framerate Control (20, 30, 50, 60 FPS)
	framerateParam.set("Framerate (FPS)", 60, 20, 60);
	framerateSlider.setup(framerateParam);
	settingsPanel.add(&framerateSlider);

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
	setupStyledPanel(motorPanel, "MOTOR", "motor.xml", panelX(1), startY);

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

	// === PANEL 3: UP LED (Center) ===
	setupStyledPanel(ledUpPanel, "UP LED", "led_up.xml", panelX(2), startY);

	// NOTE: LED panel contents will be populated by updateUIPanelsBinding()

	// === PANEL 4: DOWN LED (Center-Right) ===
	setupStyledPanel(ledDownPanel, "DOWN LED", "led_down.xml", panelX(3), startY);

	// NOTE: LED panel contents will be populated by updateUIPanelsBinding()

	// === PANEL 5: MODULE (Right) ===
	setupStyledPanel(luminosityPanel, "MODULE", "luminosity.xml", panelX(4), startY);

	// Sync Controls for LED Controllers
	syncColorsParam.set("Sync Controllers", false);
	luminosityPanel.add(syncColorsParam);

	// Individual Luminosity for current module (shared between Up/Down controllers)
	currentHgIndividualLuminosityParam.set("Module Luminosity", 1.0f, 0.0f, 1.0f);
	if (hourglassManager->getHourGlassCount() > 0) {
		auto * hg = hourglassManager->getHourGlass(0);
		if (hg) {
			currentHgIndividualLuminosityParam.set(hg->individualLuminosity.get());
		}
	}
	currentHgIndividualLuminositySlider.setup(currentHgIndividualLuminosityParam);
	luminosityPanel.add(&currentHgIndividualLuminositySlider);

	// === PANEL 6: EFFECTS ===
	setupStyledPanel(effectsPanel, "EFFECTS", "effects.xml", panelX(5), startY);

	addCosineArcEffectBtn.setup("Add Arc Effect");
	clearAllEffectsBtn.setup("Clear Effects");

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

	// Add listener for individual luminosity
	currentHgIndividualLuminosityParam.addListener(this, &UIWrapper::onIndividualLuminosityChanged);

	// NOTE: HourGlass parameter listeners (colors, blend/origin/arc, motor params)
	// are managed per-selection by updateListenersForCurrentHourglass()

	// Quick action listeners
	allOffBtn.addListener(this, &UIWrapper::onAllOffPressed);
	ledsOffBtn.addListener(this, &UIWrapper::onLedsOffPressed);

	// Global Luminosity Listener
	globalLuminosityParam.addListener(this, &UIWrapper::onGlobalLuminosityChanged);

	// Framerate Listener
	framerateParam.addListener(this, &UIWrapper::onFramerateChanged);

	// Effects Listeners
	addCosineArcEffectBtn.addListener(this, &UIWrapper::onAddCosineArcEffectPressed);
	clearAllEffectsBtn.addListener(this, &UIWrapper::onClearAllEffectsPressed);

	// New addition: Set Zero ALL Motors button listener
	setZeroAllBtn.addListener(this, &UIWrapper::onSetZeroAllPressed);
}

// --- Chrome: status bar, stop-all button, shortcuts overlay ---

// Draws with statusFont when loaded, bitmap font otherwise; returns text width
static float drawChromeText(const ofTrueTypeFont & font, const std::string & text, float x, float y) {
	if (font.isLoaded()) {
		font.drawString(text, x, y);
		return font.stringWidth(text);
	}
	ofDrawBitmapString(text, x, y);
	return text.size() * 8.0f;
}

void UIWrapper::drawStatusBar() {
	const float h = kStatusBarHeight;
	const float y = ofGetHeight() - h - 10;
	const float x = kMargin;
	const float w = ofGetWidth() - kMargin * 2;

	ofSetColor(kPanelBg);
	ofDrawRectRounded(x, y, w, h, 6);

	auto * hg = hourglassManager->getHourGlass(currentHourGlass);
	const float textY = y + h * 0.5f + 4;
	float tx = x + 14;

	if (hg) {
		// Live color swatch of the module being edited
		ofSetColor(hg->upLedColor.get());
		ofDrawRectRounded(tx, y + h * 0.5f - 6, 12, 12, 3);
		tx += 20;

		ofSetColor(kInk);
		tx += drawChromeText(statusFont, "EDITING HG " + ofToString(currentHourGlass + 1) + " · " + hg->getName(), tx, textY) + 24;

		ofSetColor(hg->isConnected() ? kOk : kDanger);
		tx += drawChromeText(statusFont, hg->isConnected() ? "CONNECTED" : "DISCONNECTED", tx, textY) + 24;

		ofSetColor(hg->motorEnabled.get() ? kOk : kMuted);
		tx += drawChromeText(statusFont, hg->motorEnabled.get() ? "MOTOR ON" : "MOTOR OFF", tx, textY) + 24;
	} else {
		ofSetColor(kDanger);
		tx += drawChromeText(statusFont, "NO HOURGLASS AVAILABLE", tx, textY) + 24;
	}

	// Right side: OSC activity + shortcuts hint
	float rx = x + w - 14;
	const std::string hint = "?  SHORTCUTS";
	float hintW = statusFont.isLoaded() ? statusFont.stringWidth(hint) : hint.size() * 8.0f;
	rx -= hintW;
	ofSetColor(kMuted);
	drawChromeText(statusFont, hint, rx, textY);

	rx -= 22;
	float timeSinceLastOSC = ofGetElapsedTimef() - lastOSCMessageTime;
	if (timeSinceLastOSC < OSC_ACTIVITY_FADE_TIME) {
		float alpha = ofMap(timeSinceLastOSC, 0, OSC_ACTIVITY_FADE_TIME, 255, 0);
		ofSetColor(kOk, alpha);
	} else {
		ofSetColor(kMuted, 90);
	}
	ofDrawCircle(rx, y + h * 0.5f, 4);

	rx -= 14 + (statusFont.isLoaded() ? statusFont.stringWidth("OSC") : 24.0f);
	ofSetColor(kMuted);
	drawChromeText(statusFont, "OSC", rx, textY);
}

void UIWrapper::drawEStop() {
	bool hover = eStopRect.inside(ofGetMouseX(), ofGetMouseY());
	float flash = ofClamp(1.0f - (ofGetElapsedTimef() - lastEStopTime) / 0.35f, 0.0f, 1.0f);

	ofColor fill = hover ? ofColor(154, 34, 38) : ofColor(127, 29, 32);
	fill.lerp(kDanger, flash * 0.8f);
	ofSetColor(fill);
	ofDrawRectRounded(eStopRect, 8);
	ofNoFill();
	ofSetLineWidth(2);
	ofSetColor(kDanger);
	ofDrawRectRounded(eStopRect, 8);
	ofFill();
	ofSetLineWidth(1);

	ofSetColor(255, 217, 218);
	const std::string line1 = "STOP ALL";
	const std::string line2 = "MOTORS";
	float cx = eStopRect.getCenter().x;
	if (estopFont.isLoaded()) {
		estopFont.drawString(line1, cx - estopFont.stringWidth(line1) * 0.5f, eStopRect.y + 28);
		estopFont.drawString(line2, cx - estopFont.stringWidth(line2) * 0.5f, eStopRect.y + 48);
	} else {
		ofDrawBitmapString(line1, cx - line1.size() * 4.0f, eStopRect.y + 28);
		ofDrawBitmapString(line2, cx - line2.size() * 4.0f, eStopRect.y + 48);
	}
}

void UIWrapper::drawShortcutsOverlay() {
	ofSetColor(0, 0, 0, 170);
	ofDrawRectangle(0, 0, ofGetWidth(), ofGetHeight());

	const float w = 480, h = 320;
	const float x = (ofGetWidth() - w) * 0.5f;
	const float y = (ofGetHeight() - h) * 0.5f;

	ofSetColor(22, 25, 31, 245);
	ofDrawRectRounded(x, y, w, h, 10);
	ofNoFill();
	ofSetColor(kBorder);
	ofDrawRectRounded(x, y, w, h, 10);
	ofFill();

	ofSetColor(kAmber);
	drawChromeText(statusFont, "KEYBOARD SHORTCUTS", x + 24, y + 34);

	// Keep this list in sync with handleKeyPressed()
	static const char * const rows[][2] = {
		{ "1-9", "Select hourglass" },
		{ "C / X", "Connect / disconnect all" },
		{ "U / D", "Motor +/- 1000 steps" },
		{ "LEFT / RIGHT", "Rotate -/+ 45°" },
		{ "UP / DOWN", "Rotate +/- 180°" },
		{ "O", "LEDs off (current module)" },
		{ "V", "Toggle minimal view" },
		{ "CMD+S", "Save settings" },
		{ "?", "Close this help" },
	};
	float rowY = y + 66;
	for (auto & row : rows) {
		ofSetColor(kInk);
		drawChromeText(statusFont, row[0], x + 24, rowY);
		ofSetColor(kMuted);
		drawChromeText(statusFont, row[1], x + 150, rowY);
		rowY += 26;
	}
}

void UIWrapper::selectHourglass(int index) {
	if (index < 0 || index >= hourglassManager->getHourGlassCount()) return;
	hourglassSelectorParam = index + 1; // listener rebinds the panels
}

void UIWrapper::stopAllMotors() {
	lastEStopTime = ofGetElapsedTimef();
	for (int i = 0; i < hourglassManager->getHourGlassCount(); i++) {
		auto * hg = hourglassManager->getHourGlass(i);
		if (hg && hg->isConnected()) {
			hg->emergencyStop();
			hg->motorEnabled.set(false);
		}
	}
	ofLogNotice("UIWrapper") << "STOP ALL MOTORS pressed";
}

bool UIWrapper::handleMousePressed(int x, int y) {
	if (shortcutsVisible) {
		shortcutsVisible = false;
		return true;
	}
	if (currentViewMode == DETAILED_VIEW && eStopRect.inside(x, y)) {
		stopAllMotors();
		return true;
	}
	return false;
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
		}
	}
}

void UIWrapper::handleConnectionCommands(int key) {
	switch (key) {
	case 'c': // Connect all
		hourglassManager->connectAll();

		break;
	case 'x': // Disconnect all
		hourglassManager->disconnectAll();

		break;
	}
}

void UIWrapper::handleMotorCommands(int key) {
	auto * hg = hourglassManager->getHourGlass(currentHourGlass);
	if (!hg || !hg->isConnected()) return;

	switch (key) {
	case 'u': // Move motor up
		hg->commandRelativeMove(1000); // Example steps

		break;
	case 'd': // Move motor down
		hg->commandRelativeMove(-1000); // Example steps

		break;
	case OF_KEY_LEFT:
		hg->commandRelativeAngle(-45.0f);

		break;
	case OF_KEY_RIGHT:
		hg->commandRelativeAngle(45.0f);

		break;
	case OF_KEY_UP:
		hg->commandRelativeAngle(180.0f);

		break;
	case OF_KEY_DOWN:
		hg->commandRelativeAngle(-180.0f);

		break;
	}
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
		currentViewMode = (currentViewMode == DETAILED_VIEW) ? MINIMAL_VIEW : DETAILED_VIEW;
	}
	if (key == '?' || key == 'h') { // shortcuts overlay
		shortcutsVisible = !shortcutsVisible;
	}
}

// GUI Event Handlers
void UIWrapper::hourglassSelectorChanged(int & selection) {
	currentHourGlass = selection - 1;

	auto * hg = hourglassManager->getHourGlass(currentHourGlass);
	if (hg) {
		// CRITICAL: Update UI panels to bind to the newly selected hourglass's state
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

	// Prevent listeners from acting during this batch rebind from HG state
	isInternallySyncing = true;

	// Echo the selection in every per-module panel header
	const std::string tag = " · HG " + ofToString(currentHourGlass + 1);
	motorPanel.setName("MOTOR" + tag);
	ledUpPanel.setName("UP LED" + tag);
	ledDownPanel.setName("DOWN LED" + tag);
	luminosityPanel.setName("MODULE" + tag);
	effectsPanel.setName("EFFECTS" + tag);

	// Sync the individual luminosity UI parameter from the HourGlass
	currentHgIndividualLuminosityParam.set(hg->individualLuminosity.get());

	// Clear and rebuild panels with correct hourglass bindings
	ledUpPanel.clear();
	ledDownPanel.clear();
	motorPanel.clear(); // Also clear motor panel to re-add its specific elements like buttons

	// === UP LED PANEL === (panels bind the HourGlass's own parameters)
	// The picker child captures the default fill at creation - keep its canvas
	// dark, not amber
	ofxBaseGui::setDefaultFillColor(ofColor(20, 23, 28, 235));
	ledUpPanel.add(hg->upLedColor.set("RGB Color", hg->upLedColor.get()));
	ofxBaseGui::setDefaultFillColor(kFillAmber);
	ledUpPanel.add(hg->upMainLed.set("Main LED", hg->upMainLed.get(), 0, 255));
	ledUpPanel.add(hg->upPwm.set("PWM", hg->upPwm.get(), 0, 255));
	ledUpPanel.add(hg->upLedBlend.set("Up Blend", hg->upLedBlend.get(), 0, 768));
	ledUpPanel.add(hg->upLedOrigin.set("Up Origin", hg->upLedOrigin.get(), 0, 360));
	ledUpPanel.add(hg->upLedArc.set("Up Arc", hg->upLedArc.get(), 0, 360));
	ledUpPanel.getGroup("RGB Color").maximize();

	// === DOWN LED PANEL ===
	ofxBaseGui::setDefaultFillColor(ofColor(20, 23, 28, 235));
	ledDownPanel.add(hg->downLedColor.set("RGB Color", hg->downLedColor.get()));
	ofxBaseGui::setDefaultFillColor(kFillAmber);
	ledDownPanel.add(hg->downMainLed.set("Main LED", hg->downMainLed.get(), 0, 255));
	ledDownPanel.add(hg->downPwm.set("PWM", hg->downPwm.get(), 0, 255));
	ledDownPanel.add(hg->downLedBlend.set("Down Blend", hg->downLedBlend.get(), 0, 768));
	ledDownPanel.add(hg->downLedOrigin.set("Down Origin", hg->downLedOrigin.get(), 0, 360));
	ledDownPanel.add(hg->downLedArc.set("Down Arc", hg->downLedArc.get(), 0, 360));
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
	motorPanel.add(&relativeAngleInput);
	motorPanel.add(&absoluteAngleInput);

	// Buttons
	motorPanel.add(&emergencyStopBtn);
	motorPanel.add(&setZeroBtn);
	motorPanel.add(&moveRelativeBtn);
	motorPanel.add(&moveAbsoluteBtn);
	motorPanel.add(&moveRelativeAngleBtn);
	motorPanel.add(&moveAbsoluteAngleBtn);

	isInternallySyncing = false;

	// Update listeners for the newly selected hourglass (for parameters directly from HourGlass)
	updateListenersForCurrentHourglass();
}

void UIWrapper::updateListenersForCurrentHourglass() {
	// Remove old listeners from ALL hourglasses first
	for (int i = 0; i < hourglassManager->getHourGlassCount(); i++) {
		auto * hg = hourglassManager->getHourGlass(i);
		if (hg) {
			hg->motorEnabled.removeListener(this, &UIWrapper::onMotorEnabledChanged);
			hg->microstep.removeListener(this, &UIWrapper::onMicrostepChanged);
			hg->motorAcceleration.removeListener(this, &UIWrapper::onMotorAccelerationChanged);
			hg->upLedColor.removeListener(this, &UIWrapper::onUpLedColorChanged);
			hg->downLedColor.removeListener(this, &UIWrapper::onDownLedColorChanged);
			hg->upLedBlend.removeListener(this, &UIWrapper::onUpLedBlendChanged);
			hg->upLedOrigin.removeListener(this, &UIWrapper::onUpLedOriginChanged);
			hg->upLedArc.removeListener(this, &UIWrapper::onUpLedArcChanged);
			hg->downLedBlend.removeListener(this, &UIWrapper::onDownLedBlendChanged);
			hg->downLedOrigin.removeListener(this, &UIWrapper::onDownLedOriginChanged);
			hg->downLedArc.removeListener(this, &UIWrapper::onDownLedArcChanged);
		}
	}

	// Add listeners to the currently selected hourglass
	auto * hg = hourglassManager->getHourGlass(currentHourGlass);
	if (hg) {
		hg->motorEnabled.addListener(this, &UIWrapper::onMotorEnabledChanged);
		hg->microstep.addListener(this, &UIWrapper::onMicrostepChanged);
		hg->motorAcceleration.addListener(this, &UIWrapper::onMotorAccelerationChanged);
		hg->upLedColor.addListener(this, &UIWrapper::onUpLedColorChanged);
		hg->downLedColor.addListener(this, &UIWrapper::onDownLedColorChanged);
		hg->upLedBlend.addListener(this, &UIWrapper::onUpLedBlendChanged);
		hg->upLedOrigin.addListener(this, &UIWrapper::onUpLedOriginChanged);
		hg->upLedArc.addListener(this, &UIWrapper::onUpLedArcChanged);
		hg->downLedBlend.addListener(this, &UIWrapper::onDownLedBlendChanged);
		hg->downLedOrigin.addListener(this, &UIWrapper::onDownLedOriginChanged);
		hg->downLedArc.addListener(this, &UIWrapper::onDownLedArcChanged);
	}
}

void UIWrapper::onConnectPressed() {
	hourglassManager->connectAll();
}

void UIWrapper::onDisconnectPressed() {
	hourglassManager->disconnectAll();
}

void UIWrapper::onEmergencyStopPressed() {
	auto * hg = hourglassManager->getHourGlass(currentHourGlass);
	if (hg && hg->isConnected()) {
		hg->emergencyStop(); // This can remain direct as it's an immediate action
		hg->motorEnabled.set(false); // Update the parameter state
	}
}

void UIWrapper::onSetZeroPressed() {
	auto * hg = hourglassManager->getHourGlass(currentHourGlass);
	if (hg && hg->isConnected()) {
		hg->setMotorZero(); // Use HourGlass method (handles OSC output)
	}
}

void UIWrapper::onMoveRelativePressed() {
	auto * hg = hourglassManager->getHourGlass(currentHourGlass);
	if (hg && hg->isConnected()) {
		hg->commandRelativeMove(relativePositionParam.get());
	}
}

void UIWrapper::onMoveAbsolutePressed() {
	auto * hg = hourglassManager->getHourGlass(currentHourGlass);
	if (hg && hg->isConnected()) {
		hg->commandAbsoluteMove(absolutePositionParam.get());
	}
}

void UIWrapper::onMoveRelativeAnglePressed() {
	auto * hg = hourglassManager->getHourGlass(currentHourGlass);
	if (hg && hg->isConnected()) {
		hg->commandRelativeAngle(static_cast<float>(relativeAngleParam.get()));
	}
}

void UIWrapper::onMoveAbsoluteAnglePressed() {
	auto * hg = hourglassManager->getHourGlass(currentHourGlass);
	if (hg && hg->isConnected()) {
		hg->commandAbsoluteAngle(static_cast<float>(absoluteAngleParam.get()));
	}
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

		// Use safe parameter setting; applied by applyLedParameters() in the update loop
		hg->upLedColor.set(ofColor::black);
		hg->downLedColor.set(ofColor::black);
		hg->upMainLed.set(0);
		hg->downMainLed.set(0);
		hg->upPwm.set(0);
		hg->downPwm.set(0);

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

		} else {
			hg->upLedColor.set(ofColor::black);
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

		} else {
			hg->disableMotor();
		}
	}
}

void UIWrapper::onMicrostepChanged(int & value) {
	auto * hg = hourglassManager->getHourGlass(currentHourGlass);
	if (hg && hg->isConnected() && !hg->updatingFromOSC) {
		// Apply microstep change to hardware
		hg->applyMotorParameters();
	}
}

void UIWrapper::onMotorAccelerationChanged(int & value) {
	// Motor acceleration is just a parameter for the next movement command,
	// but HourGlass might apply it if motor is enabled.
	auto * hg = hourglassManager->getHourGlass(currentHourGlass);
	if (hg && hg->isConnected() && !hg->updatingFromOSC) {
		hg->applyMotorParameters();
	}
}

// LED parameter listeners - mirror up<->down when "Sync Controllers" is enabled
void UIWrapper::onUpLedColorChanged(ofColor & color) {
	if (isUpdatingFromEffects) return;
	auto * hg = hourglassManager->getHourGlass(currentHourGlass);
	if (hg && !hg->updatingFromOSC) {
		if (syncColorsParam.get()) {
			hg->downLedColor.set(color);
		}
	}
}

void UIWrapper::onDownLedColorChanged(ofColor & color) {
	if (isUpdatingFromEffects) return;
	auto * hg = hourglassManager->getHourGlass(currentHourGlass);
	if (hg && !hg->updatingFromOSC) {
		if (syncColorsParam.get()) {
			hg->upLedColor.set(color);
		}
	}
}

void UIWrapper::onUpLedBlendChanged(int & value) {
	HourGlass * hg = hourglassManager->getHourGlass(currentHourGlass);
	if (!hg || hg->updatingFromOSC || isInternallySyncing) return;

	if (syncColorsParam.get()) {
		isInternallySyncing = true;
		hg->downLedBlend.set(value);
		isInternallySyncing = false;
	}
}

void UIWrapper::onUpLedOriginChanged(int & value) {
	HourGlass * hg = hourglassManager->getHourGlass(currentHourGlass);
	if (!hg || hg->updatingFromOSC || isInternallySyncing) return;

	if (syncColorsParam.get()) {
		isInternallySyncing = true;
		hg->downLedOrigin.set(value);
		isInternallySyncing = false;
	}
}

void UIWrapper::onUpLedArcChanged(int & value) {
	HourGlass * hg = hourglassManager->getHourGlass(currentHourGlass);
	if (!hg || hg->updatingFromOSC || isInternallySyncing) return;

	if (syncColorsParam.get()) {
		isInternallySyncing = true;
		hg->downLedArc.set(value);
		isInternallySyncing = false;
	}
}

void UIWrapper::onDownLedBlendChanged(int & value) {
	HourGlass * hg = hourglassManager->getHourGlass(currentHourGlass);
	if (!hg || hg->updatingFromOSC || isInternallySyncing) return;

	if (syncColorsParam.get()) {
		isInternallySyncing = true;
		hg->upLedBlend.set(value);
		isInternallySyncing = false;
	}
}

void UIWrapper::onDownLedOriginChanged(int & value) {
	HourGlass * hg = hourglassManager->getHourGlass(currentHourGlass);
	if (!hg || hg->updatingFromOSC || isInternallySyncing) return;

	if (syncColorsParam.get()) {
		isInternallySyncing = true;
		hg->upLedOrigin.set(value);
		isInternallySyncing = false;
	}
}

void UIWrapper::onDownLedArcChanged(int & value) {
	HourGlass * hg = hourglassManager->getHourGlass(currentHourGlass);
	if (!hg || hg->updatingFromOSC || isInternallySyncing) return;

	if (syncColorsParam.get()) {
		isInternallySyncing = true;
		hg->upLedArc.set(value);
		isInternallySyncing = false;
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
// Guarded wrappers: ofXml load/save resolve paths through std::filesystem,
// which throws when macOS denies folder access (TCC). That must never take
// the app down - especially not saveSettings() running in the destructor.
void UIWrapper::saveSettings() {
	try {
		saveSettingsImpl();
	} catch (const std::exception & e) {
		ofLogError("UIWrapper") << "saveSettings failed: " << e.what();
	}
}

void UIWrapper::loadSettings() {
	try {
		loadSettingsImpl();
	} catch (const std::exception & e) {
		ofLogError("UIWrapper") << "loadSettings failed: " << e.what();
	}
}

void UIWrapper::saveSettingsImpl() {

	// === Save UI State (current selection, global settings) ===
	ofXml uiStateConfig;
	auto uiStateNode = uiStateConfig.appendChild("UIState");
	uiStateNode.setAttribute("currentHourGlass", ofToString(currentHourGlass));
	uiStateNode.setAttribute("globalLuminosity", ofToString(LedMagnetController::getGlobalLuminosity()));
	uiStateNode.setAttribute("framerate", ofToString(framerateParam.get()));
	uiStateNode.setAttribute("syncColors", syncColorsParam.get() ? "true" : "false");

	// Sequencer state: reload the same XML/scene (and resume playback) on next launch
	uiStateNode.setAttribute("vezerXmlPath", (vezerPlayer && vezerPlayer->isLoaded()) ? vezerPlayer->getPath() : "");
	uiStateNode.setAttribute("vezerScene", ofToString(vezerPlayer ? vezerPlayer->getCompositionIndex() : 0));
	uiStateNode.setAttribute("vezerLoop", (vezerPlayer && vezerPlayer->getLoop()) ? "true" : "false");
	uiStateNode.setAttribute("vezerPlaying", (vezerPlayer && vezerPlayer->isPlaying()) ? "true" : "false");

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
}

void UIWrapper::loadSettingsImpl() {

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

			int savedFramerate = ofToInt(uiStateNode.getAttribute("framerate").getValue());
			if (savedFramerate >= 20 && savedFramerate <= 60) { // Basic validation for framerate range
				framerateParam.set(savedFramerate);
				ofSetFrameRate(savedFramerate); // Apply the framerate immediately
			}

			bool syncColors = (uiStateNode.getAttribute("syncColors").getValue() == "true");
			syncColorsParam.set(syncColors);

			int savedSelection = ofToInt(uiStateNode.getAttribute("currentHourGlass").getValue());
			if (savedSelection >= 0 && savedSelection < hourglassManager->getHourGlassCount()) {
				currentHourGlass = savedSelection;
				hourglassSelectorParam.set(currentHourGlass + 1);
			}

			// Restore the sequencer: reload XML, scene, loop, and resume if it was playing
			std::string vezerPath = uiStateNode.getAttribute("vezerXmlPath").getValue();
			if (!vezerPath.empty() && vezerPlayer && ofFile::doesFileExist(vezerPath, false)) {
				if (vezerPlayer->load(vezerPath)) {
					vezerPlayer->selectComposition(ofToInt(uiStateNode.getAttribute("vezerScene").getValue()));
					vezerPlayer->setLoop(uiStateNode.getAttribute("vezerLoop").getValue() == "true");
					if (uiStateNode.getAttribute("vezerPlaying").getValue() == "true") {
						vezerPlayer->play();
					}
					rebuildSequencerPanel();
				}
			} else if (!vezerPath.empty()) {
				ofLogWarning("UIWrapper") << "Saved Vezer XML no longer exists: " << vezerPath;
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
}

// Implement the new listener method
void UIWrapper::onGlobalLuminosityChanged(float & luminosity) {
	LedMagnetController::setGlobalLuminosity(luminosity);
	if (hourglassManager) {
		hourglassManager->refreshAllLedStates();
	}
}

void UIWrapper::onFramerateChanged(int & framerate) {
	// Set OpenFrameworks target framerate
	ofSetFrameRate(framerate);
	ofLogNotice("UIWrapper") << "Framerate changed to: " << framerate << " FPS";
}

void UIWrapper::onIndividualLuminosityChanged(float & luminosity) {
	auto * hg = hourglassManager->getHourGlass(currentHourGlass);
	if (hg && !hg->updatingFromOSC && !isInternallySyncing) {
		hg->individualLuminosity.set(luminosity);

		// Effective output changed - force re-send of this hourglass's LED state
		hg->refreshLedState();
	} else if (!hg) {
		ofLogWarning("UIWrapper") << "Attempted to change individual luminosity for an invalid HourGlass selection.";
	}
}

void UIWrapper::updateGlobalLuminositySlider(float luminosity) {
	// Temporarily remove listener to prevent feedback loop if set triggers listener
	globalLuminosityParam.removeListener(this, &UIWrapper::onGlobalLuminosityChanged);
	globalLuminosityParam.set(luminosity);
	globalLuminosityParam.addListener(this, &UIWrapper::onGlobalLuminosityChanged);
}

void UIWrapper::updateCurrentIndividualLuminositySlider(float luminosity) {
	// Temporarily remove listener to prevent feedback loop
	currentHgIndividualLuminosityParam.removeListener(this, &UIWrapper::onIndividualLuminosityChanged);
	currentHgIndividualLuminosityParam.set(luminosity);
	currentHgIndividualLuminosityParam.addListener(this, &UIWrapper::onIndividualLuminosityChanged);
}

void UIWrapper::onAddCosineArcEffectPressed() {
	if (!hourglassManager) return;
	HourGlass * hg = hourglassManager->getHourGlass(currentHourGlass);
	if (hg) {
		// Add to UP controller
		auto upEffect = std::make_unique<ArcCosineEffect>(90.0f, 270.0f, 5.0f); // Example params
		hg->addUpEffect(std::move(upEffect));

		// Add to DOWN controller
		auto downEffect = std::make_unique<ArcCosineEffect>(45.0f, 315.0f, 3.5f); // Example params
		hg->addDownEffect(std::move(downEffect));
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
	} else {
		ofLogWarning("UIWrapper") << "No HourGlass selected to clear effects from.";
	}
}

void UIWrapper::onSetZeroAllPressed() {
	if (hourglassManager) {
		hourglassManager->setZeroAll();
	}
}

// --- Vezér sequencer GUI ---

void UIWrapper::setupSequencerPanel() {
	// Wider than the top panels: scene names need the room. Keep the wide
	// default active until the sequencer widgets below are set up.
	ofxBaseGui::setDefaultWidth(kSequencerWidth);
	setupStyledPanel(sequencerPanel, "SEQUENCER (VEZER)", "sequencer.xml", ofGetWidth() - kMargin - kSequencerWidth, kContentTop);

	seqCompParam.set("Scene", 1, 1, 1);
	seqPlayParam.set("Play", false);
	seqLoopParam.set("Loop", false);
	seqPositionParam.set("Position", 0.0f, 0.0f, 1.0f);

	// Setup widgets ONCE, before listeners: ofxButton::setup() fires the
	// button's parameter event, which would open the load dialog mid-setup.
	seqLoadBtn.setup("Load Vezer XML...");
	seqFileLabel.setup("File", "none loaded");
	seqCompNameLabel.setup("Name", "");
	seqTimeLabel.setup("Time", "0.0 / 0.0 s");

	seqLoadBtn.addListener(this, &UIWrapper::onSeqLoadPressed);
	seqCompParam.addListener(this, &UIWrapper::onSeqCompositionChanged);
	seqPlayParam.addListener(this, &UIWrapper::onSeqPlayChanged);
	seqLoopParam.addListener(this, &UIWrapper::onSeqLoopChanged);
	seqPositionParam.addListener(this, &UIWrapper::onSeqPositionChanged);

	rebuildSequencerPanel();

	ofxBaseGui::setDefaultWidth(kPanelWidth); // back to the top-panel width
}

void UIWrapper::rebuildSequencerPanel() {
	sequencerPanel.clear();
	sequencerPanel.add(&seqLoadBtn);

	if (!vezerPlayer || !vezerPlayer->isLoaded()) {
		seqFileLabel = "none loaded";
		sequencerPanel.add(&seqFileLabel);
		return;
	}

	seqFileLabel = ofFilePath::getFileName(vezerPlayer->getPath());
	sequencerPanel.add(&seqFileLabel);

	isSyncingSequencerUI = true;
	seqCompParam.setMin(1);
	seqCompParam.setMax(vezerPlayer->getCompositionCount());
	seqCompParam.set(vezerPlayer->getCompositionIndex() + 1);
	seqLoopParam.set(vezerPlayer->getLoop());
	seqPlayParam.set(vezerPlayer->isPlaying());
	isSyncingSequencerUI = false;

	sequencerPanel.add(seqCompParam);
	sequencerPanel.add(&seqCompNameLabel);
	sequencerPanel.add(seqPlayParam);
	sequencerPanel.add(seqLoopParam);
	sequencerPanel.add(seqPositionParam);
	sequencerPanel.add(&seqTimeLabel);
}

void UIWrapper::syncSequencerUI() {
	if (!vezerPlayer || !vezerPlayer->isLoaded()) return;
	const VezerPlayer::Composition * comp = vezerPlayer->getCurrent();
	if (!comp) return;

	isSyncingSequencerUI = true;
	// Playback can stop on its own at the end of a scene
	if (seqPlayParam.get() != vezerPlayer->isPlaying()) seqPlayParam.set(vezerPlayer->isPlaying());
	if (seqLoopParam.get() != vezerPlayer->getLoop()) seqLoopParam.set(vezerPlayer->getLoop());
	if (seqCompParam.get() != vezerPlayer->getCompositionIndex() + 1) seqCompParam.set(vezerPlayer->getCompositionIndex() + 1);
	seqPositionParam.set(vezerPlayer->getPositionNormalized());
	isSyncingSequencerUI = false;

	seqCompNameLabel = comp->name + (comp->enabled ? "" : " [off]");
	seqTimeLabel = ofToString(vezerPlayer->getPositionSeconds(), 1) + " / "
		+ ofToString(comp->durationSeconds(), 1) + " s  ("
		+ ofToString((int)comp->fps) + "fps, " + ofToString(comp->playableTrackCount) + " trk)";
}

void UIWrapper::onSeqLoadPressed() {
	ofFileDialogResult result = ofSystemLoadDialog("Select a Vezer XML export");
	if (!result.bSuccess) return;

	if (vezerPlayer && vezerPlayer->load(result.getPath())) {
		rebuildSequencerPanel();
	} else {
		ofLogError("UIWrapper") << "Failed to load Vezer XML: " << result.getPath();
	}
}

void UIWrapper::onSeqCompositionChanged(int & index) {
	if (isSyncingSequencerUI || !vezerPlayer || !vezerPlayer->isLoaded()) return;
	bool wasPlaying = vezerPlayer->isPlaying();
	vezerPlayer->selectComposition(index - 1);
	if (wasPlaying) vezerPlayer->play(); // keep running when switching scenes live
}

void UIWrapper::onSeqPlayChanged(bool & play) {
	if (isSyncingSequencerUI || !vezerPlayer) return;
	if (play) {
		vezerPlayer->play();
	} else {
		vezerPlayer->stop();
	}
}

void UIWrapper::onSeqLoopChanged(bool & loop) {
	if (isSyncingSequencerUI || !vezerPlayer) return;
	vezerPlayer->setLoop(loop);
}

void UIWrapper::onSeqPositionChanged(float & position) {
	if (isSyncingSequencerUI || !vezerPlayer || !vezerPlayer->isLoaded()) return;
	vezerPlayer->seekNormalized(position);
}
