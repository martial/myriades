#include "OSCHelper.h"

// Parameter validation (logs error internally if invalid)
bool OSCHelper::validateParameters(const ofxOscMessage & msg, int expectedCount, const std::string & commandContext) {
	if (msg.getNumArgs() < expectedCount) {
		logError(commandContext, msg.getAddress(),
			"Insufficient parameters (expected " + ofToString(expectedCount) + ", got " + ofToString(msg.getNumArgs()) + ")");
		return false;
	}
	return true;
}

// General OSC error logging
void OSCHelper::logError(const std::string & context, const std::string & errorMessage) {
	ofLogError("OSCHelper::" + context) << errorMessage;
}

void OSCHelper::logError(const std::string & context, const std::string & originalAddress, const std::string & errorMessage) {
	ofLogError("OSCHelper::" + context) << "[" << originalAddress << "]: " << errorMessage;
}

// Value validation functions
bool OSCHelper::isValidColorValue(int value) {
	return value >= 0 && value <= 255;
}

bool OSCHelper::isValidMotorSpeed(int speed) {
	return speed >= 0 && speed <= 500;
}

bool OSCHelper::isValidMotorAcceleration(int accel) {
	return accel >= 0 && accel <= 255;
}

bool OSCHelper::isValidMicrostep(int microstep) {
	// Valid microsteps are powers of 2 from 1 to 256
	return microstep > 0 && microstep <= 256 && (microstep & (microstep - 1)) == 0;
}

bool OSCHelper::isValidAngle(float angle) {
	return angle >= -36000.0f && angle <= 36000.0f; // Example range
}

bool OSCHelper::isValidPWMValue(int value) {
	return value >= 0 && value <= 255;
}

// Explicit instantiations for getArgument if needed, or keep in header if simple enough
// template std::string OSCHelper::getArgument<std::string>(const ofxOscMessage& msg, int index, std::string defaultValue);
// template int OSCHelper::getArgument<int>(const ofxOscMessage& msg, int index, int defaultValue);
// template float OSCHelper::getArgument<float>(const ofxOscMessage& msg, int index, float defaultValue);
// template bool OSCHelper::getArgument<bool>(const ofxOscMessage& msg, int index, bool defaultValue);