#pragma once

#include "ofMain.h"
#include "ofxOsc.h"
#include <algorithm> // For std::clamp, std::min, std::max
#include <string>
#include <vector>

// Forward declaration
class OSCController; // For access to sendError, if needed, though trying to make OSCHelper standalone

class OSCHelper {
public:
	// Generic clamp function
	template <typename T>
	static const T & clamp(const T & value, const T & low, const T & high) {
		return std::max(low, std::min(value, high));
	}

	// Parameter validation (logs error internally if invalid)
	static bool validateParameters(const ofxOscMessage & msg, int expectedCount, const std::string & commandContext);

	// General OSC error logging
	static void logError(const std::string & context, const std::string & errorMessage);
	static void logError(const std::string & context, const std::string & originalAddress, const std::string & errorMessage);

	// Argument extraction
	template <typename T>
	static T getArgument(const ofxOscMessage & msg, int index, T defaultValue = T {}) {
		if (index >= msg.getNumArgs()) return defaultValue;

		if constexpr (std::is_same_v<T, int>) {
			if (msg.getArgType(index) == OFXOSC_TYPE_INT32 || msg.getArgType(index) == OFXOSC_TYPE_INT64) return msg.getArgAsInt(index);
			if (msg.getArgType(index) == OFXOSC_TYPE_FLOAT) return static_cast<T>(msg.getArgAsFloat(index)); // Allow float to int conversion
			logError("getArgument", msg.getAddress(), "Type mismatch for int argument at index " + ofToString(index));
			return defaultValue;
		} else if constexpr (std::is_same_v<T, float>) {
			if (msg.getArgType(index) == OFXOSC_TYPE_FLOAT) return msg.getArgAsFloat(index);
			if (msg.getArgType(index) == OFXOSC_TYPE_INT32 || msg.getArgType(index) == OFXOSC_TYPE_INT64) return static_cast<T>(msg.getArgAsInt(index)); // Allow int to float
			logError("getArgument", msg.getAddress(), "Type mismatch for float argument at index " + ofToString(index));
			return defaultValue;
		} else if constexpr (std::is_same_v<T, bool>) {
			if (msg.getArgType(index) == OFXOSC_TYPE_TRUE) return true;
			if (msg.getArgType(index) == OFXOSC_TYPE_FALSE) return false;
			if (msg.getArgType(index) == OFXOSC_TYPE_INT32 || msg.getArgType(index) == OFXOSC_TYPE_INT64) return msg.getArgAsInt(index) != 0; // Allow int to bool
			logError("getArgument", msg.getAddress(), "Type mismatch for bool argument at index " + ofToString(index));
			return defaultValue;
		} else if constexpr (std::is_same_v<T, std::string>) {
			if (msg.getArgType(index) == OFXOSC_TYPE_STRING) return msg.getArgAsString(index);
			logError("getArgument", msg.getAddress(), "Type mismatch for string argument at index " + ofToString(index));
			return defaultValue;
		}
		// Fallback for unhandled types, though the above should cover common OSC types.
		logError("getArgument", msg.getAddress(), "Unsupported type for argument at index " + ofToString(index));
		return defaultValue;
	}

	// Value validation functions
	static bool isValidColorValue(int value);
	static bool isValidMotorSpeed(int speed);
	static bool isValidMotorAcceleration(int accel);
	static bool isValidMicrostep(int microstep);
	static bool isValidAngle(float angle);
	static bool isValidPWMValue(int value);
};