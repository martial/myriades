#pragma once

#include "HourGlassManager.h"
#include "ofMain.h"
#include "ofxGui.h"
#include "ofxOscParameterSync.h"
#include <functional>

class OSCController; // Forward declaration for the pointer

class UIWrapper {
public:
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

	// Global Luminosity Panel and Parameters (public for access if needed, though typically managed internally)
	ofxPanel globalSettingsPanel; // Panel to hold global settings like luminosity
	ofParameter<float> globalLuminosityParam; // UI Parameter for global luminosity
	ofxFloatSlider globalLuminositySlider; // UI Slider for global luminosity
	ofParameter<float> currentHgIndividualLuminosityParam { "Indiv. Luminosity", 1.0f, 0.0f, 1.0f };
	ofxFloatSlider currentHgIndividualLuminositySlider;

	// LED effect parameters - now public
	ofParameter<int> upLedBlendParam;
	ofParameter<int> upLedOriginParam;
	ofParameter<int> upLedArcParam;
	ofParameter<int> downLedBlendParam;
	ofParameter<int> downLedOriginParam;
	ofParameter<int> downLedArcParam;

	static const float OSC_ACTIVITY_FADE_TIME; // How long the dot stays visible

private:
	OSCController * oscControllerInstance = nullptr; // Pointer to OSCController
	HourGlassManager * hourglassManager;
	ofParameterGroup parameters; // Main parameter group for all UI elements
	ofxPanel settingsPanel; // Panel for general settings & hourglass selection
	ofxPanel motorPanel; // Panel for motor controls
	ofxPanel ledUpPanel; // Panel for UP LED controls
	ofxPanel ledDownPanel; // Panel for DOWN LED controls
	ofxPanel luminosityPanel; // Panel for luminosity and main LED controls

	// Parameters for UI elements (will be added to panels)
	ofParameter<int> hourglassSelectorParam;
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
	ofParameter<int> downPwm;

	// Direct UI elements (Buttons, Text Inputs)
	ofxButton connectBtn, disconnectBtn;
	ofxButton emergencyStopBtn, setZeroBtn, moveRelativeBtn, moveAbsoluteBtn, moveRelativeAngleBtn, moveAbsoluteAngleBtn;
	ofxButton allOffBtn, ledsOffBtn;
	ofxIntField relativeAngleInput, absoluteAngleInput; // Text input for precise angle control
	ofxFloatField gearRatioInput, calibrationFactorInput; // Text input for precise calibration control

	// UI sliders for LED effect parameters (separate for up and down)
	ofxIntSlider upLedBlendSlider, upLedOriginSlider, upLedArcSlider;
	ofxIntSlider downLedBlendSlider, downLedOriginSlider, downLedArcSlider;

	int currentHourGlass; // Index of the currently selected HourGlass
	ofColor lastUpColor; // To detect changes for sync
	ofColor lastDownColor; // To detect changes for sync
	bool isUpdatingFromEffects; // Flag to prevent feedback loops with effects system
	float lastOSCMessageTime; // For OSC activity indicator

	// Listener method for global luminosity
	void onGlobalLuminosityChanged(float & luminosity);

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

	// Motor parameter listeners
	void onMotorEnabledChanged(bool & enabled);
	void onMicrostepChanged(int & value);
	void onMotorSpeedChanged(int & value);
	void onMotorAccelerationChanged(int & value);
	void onGearRatioChanged(float & value);
	void onCalibrationFactorChanged(float & value);

	// LED parameter listeners
	void onUpLedColorChanged(ofColor & color);
	void onDownLedColorChanged(ofColor & color);
	void onUpMainLedChanged(int & value);
	void onDownMainLedChanged(int & value);
	void onUpPwmChanged(int & value);
	void onDownPwmChanged(int & value);

	// LED effect parameter listeners (separate for up and down)
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

	// Throttling for color updates
	float lastColorUpdateTime;
	const float COLOR_UPDATE_INTERVAL = 0.05f; // Update at most 20 times per second

	// Helper method for color presets
	void setColorPreset(const ofColor & color);

	void onIndividualLuminosityChanged(float & luminosity); // Listener for individual luminosity slider
};