#pragma once

#include "HourGlassManager.h"
#include "OSCHelper.h"
#include "ofMain.h"
#include "ofxOsc.h"
#include <map>
#include <string>
#include <vector>

// Forward declaration
class UIWrapper;

// Structure to track last sent values to hardware for each hourglass
struct LastSentValues {
	ofColor upLedColor;
	ofColor downLedColor;
	int upPwm;
	int downPwm;
	int upMainLed;
	int downMainLed;
	float individualLuminosity;
	// Add effect parameters
	int upBlend;
	int upOrigin;
	int upArc;
	int downBlend;
	int downOrigin;
	int downArc;
	bool initialized;

	LastSentValues()
		: upLedColor(0, 0, 0)
		, downLedColor(0, 0, 0)
		, upPwm(-1)
		, downPwm(-1)
		, upMainLed(-1)
		, downMainLed(-1)
		, individualLuminosity(1.0f)
		// Initialize effect parameters
		, upBlend(0)
		, upOrigin(0)
		, upArc(360)
		, downBlend(0)
		, downOrigin(0)
		, downArc(360)
		, initialized(false) { }
};

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
	void forceRefreshAllHardwareStates();

	// Motor Presets (private)
	std::map<std::string, std::pair<int, int>> motorPresets;
	void loadMotorPresets(const std::string & filename = "motor_presets.json");

	// Utility functions for hourglass targeting
	std::vector<std::string> splitAddress(const std::string & address);
	int extractHourglassId(const std::vector<std::string> & addressParts);
	std::vector<int> extractHourglassIds(const std::vector<std::string> & addressParts); // New: supports "all" and "1-3" syntax
	HourGlass * getHourglassById(int id);
	std::vector<HourGlass *> getHourglassesByIds(const std::vector<int> & ids); // New: get multiple hourglasses
	bool isValidHourglassId(int id);

	// Helper functions for multi-hourglass LED operations
	void handleIndividualLedMessageForHourglass(ofxOscMessage & msg, HourGlass * hg, int hourglassId, const std::string & target, const std::string & command, const std::vector<std::string> & addressParts);
	void handleAllLedMessageForHourglass(ofxOscMessage & msg, HourGlass * hg, int hourglassId, const std::string & command, const std::vector<std::string> & addressParts);
	void handlePWMMessageForHourglass(ofxOscMessage & msg, HourGlass * hg, int hourglassId, const std::vector<std::string> & addressParts);
	void handleMainLedMessageForHourglass(ofxOscMessage & msg, HourGlass * hg, int hourglassId, const std::vector<std::string> & addressParts);

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

	// Track last sent values to hardware (indexed by hourglass ID)
	std::map<int, LastSentValues> lastSentValues;

	// Process last commands in main thread
	void processLastCommands();

	// Message handlers
	void handleConnectionMessage(ofxOscMessage & msg);
	void handleMotorMessage(ofxOscMessage & msg);
	void handleLedMessage(ofxOscMessage & msg);
	void handleSystemMessage(ofxOscMessage & msg);
	void handleGlobalBlackoutMessage(ofxOscMessage & msg);
	void handleGlobalLuminosityMessage(ofxOscMessage & msg);
	void handleIndividualLuminosityMessage(ofxOscMessage & msg);
	void handleMotorPresetMessage(ofxOscMessage & msg);
	void handleSystemMotorPresetMessage(ofxOscMessage & msg);
	void handleSystemMotorConfigMessage(ofxOscMessage & msg, const std::vector<std::string> & addressParts);
	void handleIndividualMotorConfigMessage(ofxOscMessage & msg, const std::vector<std::string> & addressParts);
	void handleSystemMotorRotateMessage(ofxOscMessage & msg, const std::vector<std::string> & addressParts);
	void handleSystemMotorPositionMessage(ofxOscMessage & msg, const std::vector<std::string> & addressParts);

	// New feature handlers (ensure this block and its contents are preserved)
	void handlePWMMessage(ofxOscMessage & msg, HourGlass * hg, const std::vector<std::string> & addressParts);
	void handleMainLedMessage(ofxOscMessage & msg, HourGlass * hg, const std::vector<std::string> & addressParts);
	void handleMotorRelativeMessage(ofxOscMessage & msg, HourGlass * hg, const std::vector<std::string> & addressParts);
	void handleMotorAbsoluteMessage(ofxOscMessage & msg, HourGlass * hg, const std::vector<std::string> & addressParts);

	// UI parameter synchronization helpers
	void updateUIAngleParameters(float relativeAngle, float absoluteAngle);

	// Utility functions
	void sendError(const std::string & originalAddress, const std::string & errorMessage);

	// Logging helpers
	void logOSCMessage(const ofxOscMessage & msg, const std::string & action = "received");
};