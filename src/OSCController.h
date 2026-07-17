#pragma once

#include "HourGlassManager.h"
#include "OSCHelper.h"
#include "ofMain.h"
#include "ofxOsc.h"
#include <map>
#include <optional>
#include <string>
#include <vector>

// Forward declaration
class UIWrapper;

class OSCController {
public:
	OSCController(HourGlassManager * manager);
	~OSCController();

	// Setup and lifecycle
	void setup(int receivePort = 8000);
	void update();
	void shutdown();

	// Configuration
	void setEnabled(bool enabled) { oscEnabled = enabled; }
	bool isEnabled() const { return oscEnabled; }

	// UI synchronization
	void setUIWrapper(UIWrapper * uiWrapper) { this->uiWrapper = uiWrapper; }

	// OSC message handling
	void processMessage(ofxOscMessage & message);

	// Motor Presets
	std::map<std::string, std::pair<int, int>> motorPresets;
	void loadMotorPresets(const std::string & filename = "motor_presets.json");

	// Utility functions for hourglass targeting (1-based OSC ids)
	int extractHourglassId(const std::vector<std::string> & addressParts);
	std::vector<int> extractHourglassIds(const std::vector<std::string> & addressParts); // supports "all", "1,3" and "1-3" syntax
	HourGlass * getHourglassById(int id);
	bool isValidHourglassId(int id);

	// Helper functions for multi-hourglass LED operations
	void handleIndividualLedMessageForHourglass(ofxOscMessage & msg, HourGlass * hg, const std::string & target, const std::string & command);
	void handleAllLedMessageForHourglass(ofxOscMessage & msg, HourGlass * hg, const std::string & command, const std::vector<std::string> & addressParts);
	void handlePWMMessageForHourglass(ofxOscMessage & msg, HourGlass * hg, const std::vector<std::string> & addressParts);
	void handleMainLedMessageForHourglass(ofxOscMessage & msg, HourGlass * hg, const std::vector<std::string> & addressParts);

private:
	// OSC communication
	ofxOscReceiver receiver;

	// System references
	HourGlassManager * hourglassManager;
	UIWrapper * uiWrapper; // For position parameter synchronization

	// Configuration
	bool oscEnabled;

	// Connection settings
	int receivePort;

	// Message handlers
	void handleMotorMessage(ofxOscMessage & msg, const std::vector<std::string> & addressParts);
	void handleLedMessage(ofxOscMessage & msg, const std::vector<std::string> & addressParts);
	void handleSystemMessage(ofxOscMessage & msg, const std::vector<std::string> & addressParts);
	void handleGlobalBlackoutMessage(ofxOscMessage & msg);
	void handleGlobalLuminosityMessage(ofxOscMessage & msg);
	void handleIndividualLuminosityMessage(ofxOscMessage & msg, const std::vector<std::string> & addressParts);
	void handleSystemMotorPresetMessage(ofxOscMessage & msg);
	void handleSystemMotorConfigMessage(ofxOscMessage & msg, const std::vector<std::string> & addressParts);
	void handleSystemMotorRotateMessage(ofxOscMessage & msg, const std::vector<std::string> & addressParts);
	void handleSystemMotorPositionMessage(ofxOscMessage & msg, const std::vector<std::string> & addressParts);

	// Shared helpers
	bool lookupPreset(const std::string & presetName, const std::string & address, int & speed, int & accel);
	static void applyMotorSpeedAccel(HourGlass & hg, int speed, int accel);
	bool parseAngleSpeedAccel(ofxOscMessage & msg, const std::vector<std::string> & addressParts, size_t angleIdx,
		const std::string & context, float & degrees, std::optional<int> & speed, std::optional<int> & accel);
	void setLedRangeParam(ofxOscMessage & msg, const std::string & context, int minValue, int maxValue, int defaultValue,
		ofParameter<int> * upParam, ofParameter<int> * downParam);
	void applyIndividualLuminosity(const std::vector<std::string> & addressParts, const std::string & address, float value);

	// UI parameter synchronization helpers
	void updateUIAngleParameters(float relativeAngle, float absoluteAngle);

	// Utility functions
	void sendError(const std::string & originalAddress, const std::string & errorMessage);
};
