#pragma once

#include "ofColor.h" // Assuming ofColor will be used

// Forward declaration if HourGlass context is needed by effects
// class HourGlass;

struct EffectParameters {
	ofColor color;
	int blend = 0;
	int origin = 0;
	int arc = 360;
	float effectLuminosityMultiplier = 1.0f; // Effects modify this; starts at 1.0
	int mainLedValue = 0;

	float deltaTime = 0.0f; // Time since last frame for time-based effects
	// HourGlass* hourglass = nullptr; // Optional: context of the hourglass if an effect needs it

	// Add any other parameters an effect might want to influence
};