#pragma once

#include "ArcCosineEffect.h"
#include "HourGlassManager.h"
#include "LEDVisualizer.h"
#include "ofMain.h"
#include "ofxGui.h"
#include "ofxOscParameterSync.h"
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
	void setup(HourGlassManager * manager, OSCController * oscCtrl);
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

	// XML save/load for persistence
	void saveSettings();
	void loadSettings();

	// Methods to update UI elements from external changes (e.g., OSC)
	void updateGlobalLuminositySlider(float luminosity);
	void updateCurrentIndividualLuminositySlider(float luminosity);

	// LED Visualizer access
	LEDVisualizer & getLEDVisualizer() { return ledVisualizer; }

	// Status section drawing helpers
	void drawStatusSection_HourglassInfo(HourGlass * hg, float x, float & y, float lineHeight, float sectionWidth);
	void drawStatusSection_KeyboardShortcuts(float x, float & y, float lineHeight, float sectionWidth);
	void drawStatusSection_OSC(float x, float & y, float lineHeight);

	// Methods for OSCController to update UI parameters safely
	// These will also need to use the isInternallySyncing flag or ensure
	// their programmatic .set() calls on UIWrapper params don't cause undesired listener firing.
	void updateUpLedBlendFromOSC(int value);
	void updateUpLedOriginFromOSC(int value);
	void updateUpLedArcFromOSC(int value);
	void updateDownLedBlendFromOSC(int value);
	void updateDownLedOriginFromOSC(int value);
	void updateDownLedArcFromOSC(int value);

	// Global Luminosity Panel and Parameters (public for access if needed, though typically managed internally)
	ofxPanel globalSettingsPanel; // Panel to hold global settings like luminosity
	ofParameter<float> globalLuminosityParam; // UI Parameter for global luminosity
	ofxFloatSlider globalLuminositySlider; // UI Slider for global luminosity
	ofParameter<float> currentHgIndividualLuminosityParam { "Indiv. Luminosity", 1.0f, 0.0f, 1.0f };
	ofxFloatSlider currentHgIndividualLuminositySlider;

	// LED effect parameters - now public and named
	ofParameter<int> upLedBlendParam { "Up Blend", 0, 0, 768 };
	ofParameter<int> upLedOriginParam { "Up Origin", 0, 0, 360 };
	ofParameter<int> upLedArcParam { "Up Arc", 360, 0, 360 };
	ofParameter<int> downLedBlendParam { "Down Blend", 0, 0, 768 };
	ofParameter<int> downLedOriginParam { "Down Origin", 0, 0, 360 };
	ofParameter<int> downLedArcParam { "Down Arc", 360, 0, 360 };

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
	ofParameter<int> hourglassSelectorParam;
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
	ofParameter<int> downPwm; // This seems like a leftover, should it be named or grouped?

	// Direct UI elements (Buttons, Text Inputs)
	ofxButton connectBtn, disconnectBtn;
	ofxButton emergencyStopBtn, setZeroBtn, moveRelativeBtn, moveAbsoluteBtn, moveRelativeAngleBtn, moveAbsoluteAngleBtn;
	ofxButton allOffBtn, ledsOffBtn;
	ofxButton setZeroAllBtn; // New button for Set Zero All
	ofxIntField relativeAngleInput, absoluteAngleInput; // Text input for precise angle control
	ofxFloatField gearRatioInput, calibrationFactorInput; // Text input for precise calibration control

	// UI sliders for LED effect parameters (separate for up and down)
	// These ofxIntSlider objects might become redundant if panels directly use ofParameter<int>
	// or if they are managed more generically. For now, keeping them if they are explicitly used.
	ofxIntSlider framerateSlider;
	ofxIntSlider upLedBlendSlider, upLedOriginSlider, upLedArcSlider;
	ofxIntSlider downLedBlendSlider, downLedOriginSlider, downLedArcSlider;

	int currentHourGlass; // Index of the currently selected HourGlass
	ofColor lastUpColor; // To detect changes for sync
	ofColor lastDownColor; // To detect changes for sync
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
	void syncColorsChanged(bool & enabled);
	void onAllOffPressed();
	void onLedsOffPressed();
	void onSetZeroAllPressed(); // Declaration for the new handler

	// Motor parameter listeners
	void onMotorEnabledChanged(bool & enabled);
	void onMicrostepChanged(int & value);
	void onMotorSpeedChanged(int & value);
	void onMotorAccelerationChanged(int & value);
	void onGearRatioChanged(float & value);
	void onCalibrationFactorChanged(float & value);

	// LED parameter listeners (RGB Color, Main LED, PWM)
	void onUpLedColorChanged(ofColor & color);
	void onDownLedColorChanged(ofColor & color);
	void onUpMainLedChanged(int & value);
	void onDownMainLedChanged(int & value);
	void onUpPwmChanged(int & value);
	void onDownPwmChanged(int & value);

	// Original individual listeners for LED Effect int parameters (Blend, Origin, Arc)
	void onUpLedBlendChanged(int & value);
	void onUpLedOriginChanged(int & value);
	void onUpLedArcChanged(int & value);
	void onDownLedBlendChanged(int & value);
	void onDownLedOriginChanged(int & value);
	void onDownLedArcChanged(int & value);

	// OSC Parameter Sync
	ofxOscParameterSync oscSync;

	// Status display
	void drawStatus();

	// Keyboard command handlers
	void handleHourGlassSelection(int key);
	void handleConnectionCommands(int key);
	void handleMotorCommands(int key);
	void handleLEDCommands(int key);
	void handleViewToggle(int key); // Added declaration

	// Throttling for color updates
	// float lastColorUpdateTime; // Commented out as unused
	// const float COLOR_UPDATE_INTERVAL = 0.05f; // Commented out as unused

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

	// Helper for setting up common panel properties
	void setupStyledPanel(ofxPanel & panel, const std::string & panelName, const std::string & settingsFilename, float x, float y);

	// Helpers for XML serialization
	void saveHourGlassToXml(ofXml & hgNode, HourGlass * hg, int hgIndex);
	void loadHourGlassFromXml(const ofXml & hgNode, HourGlass * hg, int hgIndex);
};