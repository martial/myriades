#include "ArcCosineEffect.h"
#include <cmath> // For std::cos

// Define TWO_PI if not available from ofMain.h context here
#ifndef TWO_PI
	#define TWO_PI 6.28318530717958647693
#endif

ArcCosineEffect::ArcCosineEffect(float minArc, float maxArc, float periodSeconds)
	: minArc(minArc)
	, maxArc(maxArc)
	, periodSeconds(periodSeconds)
	, elapsedTime(0.0f) {
	// Ensure period is positive to avoid division by zero or unexpected behavior
	if (this->periodSeconds <= 0.0f) {
		this->periodSeconds = 1.0f; // Default to a safe value
	}
}

void ArcCosineEffect::update(float deltaTime) {
	elapsedTime += deltaTime;
	// ofLogVerbose("ArcCosineEffect::update") << "ElapsedTime: " << elapsedTime << " Delta: " << deltaTime; // COMMENTED BACK
}

void ArcCosineEffect::apply(EffectParameters & params) {
	if (!isEnabled()) return;

	// Calculate the cosine wave value: ranges from -1 to 1
	float cosValue = std::cos(elapsedTime * TWO_PI / periodSeconds);

	// Map the cosine value (from -1 to 1) to the arc range (minArc to maxArc)
	// When cosValue is -1, result is minArc.
	// When cosValue is  1, result is maxArc.
	float newArc = minArc + (cosValue + 1.0f) * 0.5f * (maxArc - minArc);

	ofLogVerbose("ArcCosineEffect::apply") << "Applying. Old Arc: " << params.arc
										   << " New Arc: " << static_cast<int>(newArc)
										   << " ElapsedTime: " << elapsedTime; // UNCOMMENTED
	params.arc = static_cast<int>(newArc);
}