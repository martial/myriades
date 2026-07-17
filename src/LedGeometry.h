#pragma once

#include "ofMain.h"

// Shared LED ring geometry model.
// Single home for the physical ring layout and the blend/arc math used by
// both the LEDVisualizer preview and the HourGlass minimal view.
namespace LedGeometry {

// LED counts per circle (matching hardware)
inline constexpr int NUM_LEDS_CIRCLE_1 = 32; // Inner circle
inline constexpr int NUM_LEDS_CIRCLE_2 = 36; // Middle circle
inline constexpr int NUM_LEDS_CIRCLE_3 = 42; // Outer circle

inline float normalizeAngle(float angle) {
	return ofWrap(angle, 0.0f, 360.0f);
}

// True when currentAngleDegrees falls inside the arc starting at
// startAngleDegrees spanning arcSpanDegrees (span 360 = full circle, 0 = off).
inline bool isAngleInArc(float currentAngleDegrees, int startAngleDegrees, int arcSpanDegrees) {
	currentAngleDegrees = normalizeAngle(currentAngleDegrees);
	startAngleDegrees = static_cast<int>(normalizeAngle(static_cast<float>(startAngleDegrees)));
	arcSpanDegrees = ofClamp(arcSpanDegrees, 0, 360);

	if (arcSpanDegrees == 360) return true;
	if (arcSpanDegrees == 0) return false;

	int endAngle = static_cast<int>(normalizeAngle(static_cast<float>(startAngleDegrees + arcSpanDegrees)));

	if (startAngleDegrees <= endAngle) {
		return currentAngleDegrees >= startAngleDegrees && currentAngleDegrees <= endAngle;
	} else {
		// Arc crosses 0 degrees (e.g., from 350° to 10°)
		return currentAngleDegrees >= startAngleDegrees || currentAngleDegrees <= endAngle;
	}
}

// Crossfade weight of each circle (0=inner, 1=middle, 2=outer) for a
// blend value in 0-768: 0-384 fades inner→middle, 384-768 middle→outer.
inline float circleAlphaForBlend(int circleIndex, int blend) {
	float normalizedBlend = ofMap(blend, 0, 768, 0.0f, 1.0f, true);
	if (normalizedBlend <= 0.5f) {
		if (circleIndex == 0) return 1.0f - (normalizedBlend * 2.0f);
		if (circleIndex == 1) return normalizedBlend * 2.0f;
		return 0.0f;
	} else {
		if (circleIndex == 0) return 0.0f;
		if (circleIndex == 1) return 1.0f - ((normalizedBlend - 0.5f) * 2.0f);
		return (normalizedBlend - 0.5f) * 2.0f;
	}
}

} // namespace LedGeometry
