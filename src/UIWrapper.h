#pragma once

#include "ArcCosineEffect.h"
#include "HourGlassManager.h"
#include "LEDVisualizer.h"
#include "VezerPlayer.h"
#include "ofMain.h"
#include "ofxGui.h"
#include <functional>

class OSCController; // Forward declaration for the pointer

class UIWrapper {
public:
	// Add this enum definition
	enum ViewMode {
		DETAILED_VIEW,
		MINIMAL_VIEW
	};

	UIWrapper();
	~UIWrapper();

	// Main UI lifecycle
	void setup(HourGlassManager * manager, OSCController * oscCtrl, VezerPlayer * player);
	void update();
	void draw();

	// Get current selection
	int getCurrentHourGlass() const { return currentHourGlass; }

	// Keyboard event delegation
	void handleKeyPressed(int key);

	// Position parameter access for OSC synchronization
	ofParameter<int> & getRelativeAngleParam() { return relativeAngleParam; }
	ofParameter<int> & getAbsoluteAngleParam() { return absoluteAngleParam; }
	void updatePositionParameters(float relativeAngle, float absoluteAngle);

	// OSC activity tracking
	void notifyOSCMessageReceived();

	// XML save/load for persistence (guarded: a filesystem error - e.g. macOS
	// denying folder access - must not crash the app)
	void saveSettings();
	void loadSettings();

	// Methods to update UI elements from external changes (e.g., OSC)
	void updateGlobalLuminositySlider(float luminosity);
	void updateCurrentIndividualLuminositySlider(float luminosity);

	// LED Visualizer access
	LEDVisualizer & getLEDVisualizer() { return ledVisualizer; }

	// Window / mouse plumbing (layout reacts to resizes, E-stop is a custom-drawn button)
	void windowResized(int w, int h);
	bool handleMousePressed(int x, int y);

	// Global Luminosity Panel and Parameters (public for access if needed, though typically managed internally)
	ofxPanel globalSettingsPanel; // Panel to hold global settings like luminosity
	ofParameter<float> globalLuminosityParam; // UI Parameter for global luminosity
	ofxFloatSlider globalLuminositySlider; // UI Slider for global luminosity
	ofParameter<float> currentHgIndividualLuminosityParam { "Indiv. Luminosity", 1.0f, 0.0f, 1.0f };
	ofxFloatSlider currentHgIndividualLuminositySlider;

	static const float OSC_ACTIVITY_FADE_TIME; // How long the dot stays visible

	// Add this method declaration
	void drawMinimalView();

private:
	// Add this member variable
	ViewMode currentViewMode;
	bool isInternallySyncing = false; // Flag to prevent recursion during sync

	OSCController * oscControllerInstance = nullptr; // Pointer to OSCController
	HourGlassManager * hourglassManager;
	ofParameterGroup parameters; // Main parameter group for all UI elements
	ofxPanel settingsPanel; // Panel for general settings & hourglass selection
	ofxPanel motorPanel; // Panel for motor controls
	ofxPanel ledUpPanel; // Panel for UP LED controls
	ofxPanel ledDownPanel; // Panel for DOWN LED controls
	ofxPanel luminosityPanel; // Panel for luminosity and main LED controls
	ofxPanel effectsPanel; // New panel for effects

	// Parameters for UI elements (will be added to panels)
	ofParameter<int> hourglassSelectorParam; // selection state; driven by the HG buttons + keyboard
	std::vector<ofParameter<void>> hgSelectParams; // one button per hourglass
	std::vector<std::unique_ptr<of::priv::AbstractEventToken>> hgSelectListeners;
	ofParameter<int> framerateParam;
	ofParameter<void> connectBtnParam;
	ofParameter<void> disconnectBtnParam;
	ofParameter<void> emergencyStopBtnParam;
	ofParameter<void> setZeroBtnParam;
	ofParameter<int> relativePositionParam;
	ofParameter<int> absolutePositionParam;
	ofParameter<int> relativeAngleParam;
	ofParameter<int> absoluteAngleParam;
	ofParameter<void> moveRelativeBtnParam;
	ofParameter<void> moveAbsoluteBtnParam;
	ofParameter<void> moveRelativeAngleBtnParam;
	ofParameter<void> moveAbsoluteAngleBtnParam;
	ofParameter<bool> syncColorsParam;
	ofParameter<void> allOffBtnParam;
	ofParameter<void> ledsOffBtnParam;
	ofParameter<void> setZeroAllBtnParam; // New param for Set Zero All

	// Direct UI elements (Buttons, Text Inputs)
	ofxButton connectBtn, disconnectBtn;
	ofxButton emergencyStopBtn, setZeroBtn, moveRelativeBtn, moveAbsoluteBtn, moveRelativeAngleBtn, moveAbsoluteAngleBtn;
	ofxButton allOffBtn, ledsOffBtn;
	ofxButton setZeroAllBtn; // New button for Set Zero All
	ofxIntField relativeAngleInput, absoluteAngleInput; // Text input for precise angle control
	ofxFloatField gearRatioInput, calibrationFactorInput; // Text input for precise calibration control

	ofxIntSlider framerateSlider;

	int currentHourGlass; // Index of the currently selected HourGlass
	bool isUpdatingFromEffects; // Flag to prevent feedback loops with effects system
	float lastOSCMessageTime; // For OSC activity indicator

	// Listener method for global luminosity
	void onGlobalLuminosityChanged(float & luminosity);
	void onFramerateChanged(int & framerate);

	// Setup methods
	void setupPanels();
	void setupListeners();

	// Event handlers
	void hourglassSelectorChanged(int & selection);
	void onConnectPressed();
	void onDisconnectPressed();
	void onEmergencyStopPressed();
	void onSetZeroPressed();
	void onMoveRelativePressed();
	void onMoveAbsolutePressed();
	void onMoveRelativeAnglePressed();
	void onMoveAbsoluteAnglePressed();
	void onAllOffPressed();
	void onLedsOffPressed();
	void onSetZeroAllPressed(); // Declaration for the new handler

	// Motor parameter listeners
	void onMotorEnabledChanged(bool & enabled);
	void onMicrostepChanged(int & value);
	void onMotorAccelerationChanged(int & value);

	// LED parameter listeners (bound to the current HourGlass's parameters;
	// they mirror up<->down when "Sync Controllers" is enabled)
	void onUpLedColorChanged(ofColor & color);
	void onDownLedColorChanged(ofColor & color);
	void onUpLedBlendChanged(int & value);
	void onUpLedOriginChanged(int & value);
	void onUpLedArcChanged(int & value);
	void onDownLedBlendChanged(int & value);
	void onDownLedOriginChanged(int & value);
	void onDownLedArcChanged(int & value);

	// Chrome: status bar, stop-all button, shortcuts overlay
	void layoutPanels();
	void selectHourglass(int index);
	void stopAllMotors();
	void drawStatusBar();
	void drawEStop();
	void drawShortcutsOverlay();

	ofRectangle eStopRect;
	float lastEStopTime = -10.0f; // for the press flash
	bool shortcutsVisible = false;
	ofTrueTypeFont statusFont; // real TTF for custom-drawn chrome (ofxGui has its own)
	ofTrueTypeFont estopFont;

	// Keyboard command handlers
	void handleHourGlassSelection(int key);
	void handleConnectionCommands(int key);
	void handleMotorCommands(int key);
	void handleLEDCommands(int key);
	void handleViewToggle(int key); // Added declaration

	// Helper method for color presets
	void setColorPreset(const ofColor & color);

	void onIndividualLuminosityChanged(float & luminosity); // Listener for individual luminosity slider

	LEDVisualizer ledVisualizer;

	// Listeners for new buttons
	void onAddCosineArcEffectPressed();
	void onClearAllEffectsPressed();

	ofxButton addCosineArcEffectBtn;
	ofxButton clearAllEffectsBtn;

	// NEW METHODS: Fix for hourglass selection bug
	void updateUIPanelsBinding();
	void updateListenersForCurrentHourglass();

	// --- Vezér sequencer (XML OSC playback) ---
	VezerPlayer * vezerPlayer = nullptr;
	bool isSyncingSequencerUI = false; // Guard: GUI updated FROM player state

	ofxPanel sequencerPanel;
	ofxButton seqLoadBtn;
	ofxLabel seqFileLabel;
	ofParameter<int> seqCompParam; // 1-based scene selector
	ofxLabel seqCompNameLabel;
	ofParameter<bool> seqPlayParam;
	ofParameter<bool> seqLoopParam;
	ofParameter<float> seqPositionParam; // normalized 0..1, draggable to seek
	ofxLabel seqTimeLabel;

	void setupSequencerPanel();
	void rebuildSequencerPanel(); // after a file is (re)loaded
	void syncSequencerUI(); // reflect player state each frame
	void onSeqLoadPressed();
	void onSeqCompositionChanged(int & index);
	void onSeqPlayChanged(bool & play);
	void onSeqLoopChanged(bool & loop);
	void onSeqPositionChanged(float & position);

	// Helper for setting up common panel properties
	void setupStyledPanel(ofxPanel & panel, const std::string & panelName, const std::string & settingsFilename, float x, float y);

	// Helpers for XML serialization
	void saveHourGlassToXml(ofXml & hgNode, HourGlass * hg, int hgIndex);
	void loadHourGlassFromXml(const ofXml & hgNode, HourGlass * hg, int hgIndex);
	void saveSettingsImpl();
	void loadSettingsImpl();
};
