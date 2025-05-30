#include "LEDVisualizer.h"
#include "LedMagnetController.h"
#include "UIWrapper.h"

LEDVisualizer::LEDVisualizer()
	: vizWidth(800)
	, vizHeight(600)
	, backgroundColor(20, 20, 30)
	, showLabels(true)
	, showGrid(false)
	, layoutMode(0)
	, controllerSpacing(200.0f)
	, uiWrapper(nullptr) {
}

LEDVisualizer::~LEDVisualizer() {
	clearHourGlasses();
}

void LEDVisualizer::setup(int width, int height) {
	vizWidth = width;
	vizHeight = height;
	calculateLayout();
}

void LEDVisualizer::update() {
	// Update animation time for any visual effects
	float currentTime = ofGetElapsedTimef();

	// Update last update time for each tracked hourglass
	for (auto & hgViz : hourglasses) {
		if (hgViz.hourglass && hgViz.hourglass->isConnected()) {
			hgViz.lastUpdateTime = currentTime;
		}
	}
}

void LEDVisualizer::draw(float x, float y) {
	if (!uiWrapper || hourglasses.empty()) return;

	// Get current selected hourglass from UIWrapper
	int currentHG = uiWrapper->getCurrentHourGlass();
	if (currentHG < 0 || currentHG >= hourglasses.size()) return;

	HourGlass * hg = hourglasses[currentHG].hourglass;
	if (!hg) return;

	ofPushMatrix();
	ofTranslate(x, y);

	// Draw compact background
	ofSetColor(30, 30, 35, 180);
	ofDrawRectangle(0, 0, vizWidth, vizHeight);

	// Get LED parameters
	float globalLum = LedMagnetController::getGlobalLuminosity();
	float individualLum = hg->individualLuminosity.get();

	// Draw UP controller (left)
	float upX = 50;
	float upY = 60;
	ofColor upColor = hg->upLedColor.get();
	int upBlend = 0, upOrigin = 0, upArc = 360;

	LedMagnetController * upMagnet = hg->getUpLedMagnet();
	if (upMagnet && upMagnet->isRgbInitialized()) {
		upBlend = upMagnet->getLastSentBlend();
		upOrigin = upMagnet->getLastSentOrigin();
		upArc = upMagnet->getLastSentArc();
	} else if (uiWrapper) {
		upBlend = uiWrapper->upLedBlendParam.get();
		upOrigin = uiWrapper->upLedOriginParam.get();
		upArc = uiWrapper->upLedArcParam.get();
	}
	drawTinyController(upX, upY, upColor, upBlend, upOrigin, upArc, globalLum, individualLum);

	// Draw DOWN controller (right)
	float downX = 150;
	float downY = 60;
	ofColor downColor = hg->downLedColor.get();
	int downBlend = 0, downOrigin = 0, downArc = 360;

	LedMagnetController * downMagnet = hg->getDownLedMagnet();
	if (downMagnet && downMagnet->isRgbInitialized()) {
		downBlend = downMagnet->getLastSentBlend();
		downOrigin = downMagnet->getLastSentOrigin();
		downArc = downMagnet->getLastSentArc();
	} else if (uiWrapper) {
		downBlend = uiWrapper->downLedBlendParam.get();
		downOrigin = uiWrapper->downLedOriginParam.get();
		downArc = uiWrapper->downLedArcParam.get();
	}
	drawTinyController(downX, downY, downColor, downBlend, downOrigin, downArc, globalLum, individualLum);

	ofPopMatrix();
}

void LEDVisualizer::drawHourGlassControllers(const HourGlassVisualization & hgViz) {
	HourGlass * hg = hgViz.hourglass;
	float centerX = hgViz.position.x;
	float centerY = hgViz.position.y;

	// Get current LED states from OSC controller's last sent values
	float globalLum = LedMagnetController::getGlobalLuminosity();
	float individualLum = hg->individualLuminosity.get();

	// Draw UP controller (left)
	float upX = centerX - 120;
	float upY = centerY;

	int upBlendHG = 0, upOriginHG = 0, upArcHG = 360;

	LedMagnetController * upMagnetHG = hg->getUpLedMagnet();
	if (upMagnetHG && upMagnetHG->isRgbInitialized()) {
		upBlendHG = upMagnetHG->getLastSentBlend();
		upOriginHG = upMagnetHG->getLastSentOrigin();
		upArcHG = upMagnetHG->getLastSentArc();
	} else if (uiWrapper) {
		upBlendHG = uiWrapper->upLedBlendParam.get();
		upOriginHG = uiWrapper->upLedOriginParam.get();
		upArcHG = uiWrapper->upLedArcParam.get();
	}

	drawController(upX, upY, CIRCLE_3_RADIUS, hg->upLedColor.get(), upBlendHG, upOriginHG, upArcHG,
		globalLum, individualLum, "UP");

	// Draw DOWN controller (right)
	float downX = centerX + 120;
	float downY = centerY;

	int downBlendHG = 0, downOriginHG = 0, downArcHG = 360;

	LedMagnetController * downMagnetHG = hg->getDownLedMagnet();
	if (downMagnetHG && downMagnetHG->isRgbInitialized()) {
		downBlendHG = downMagnetHG->getLastSentBlend();
		downOriginHG = downMagnetHG->getLastSentOrigin();
		downArcHG = downMagnetHG->getLastSentArc();
	} else if (uiWrapper) {
		downBlendHG = uiWrapper->downLedBlendParam.get();
		downOriginHG = uiWrapper->downLedOriginParam.get();
		downArcHG = uiWrapper->downLedArcParam.get();
	}

	drawController(downX, downY, CIRCLE_3_RADIUS, hg->downLedColor.get(), downBlendHG, downOriginHG, downArcHG,
		globalLum, individualLum, "DOWN");

	// Draw connection between controllers
	ofSetColor(60, 60, 60);
	ofDrawLine(upX + CIRCLE_3_RADIUS + 10, upY, downX - CIRCLE_3_RADIUS - 10, downY);

	// Draw hourglass label and status
	if (showLabels) {
		float labelY = centerY - CIRCLE_3_RADIUS - 40;

		// Hourglass name
		ofSetColor(255);
		std::string hgLabel = hgViz.label.empty() ? ("HG " + hgViz.hourglass->getName()) : hgViz.label;
		ofDrawBitmapString(hgLabel, centerX - 50, labelY);

		// Connection status
		ofSetColor(hg->isConnected() ? ofColor::green : ofColor::red);
		ofDrawBitmapString(hg->isConnected() ? "CONNECTED" : "DISCONNECTED", centerX - 50, labelY + 15);

		// Motor status
		ofSetColor(hg->motorEnabled.get() ? ofColor::green : ofColor::gray);
		ofDrawBitmapString(string("Motor: ") + (hg->motorEnabled.get() ? "ON" : "OFF"), centerX - 50, labelY + 30);

		// Luminosity info
		ofSetColor(200, 200, 200);
		ofDrawBitmapString("Global: " + ofToString(globalLum, 2), centerX - 50, labelY + 45);
		ofDrawBitmapString("Individual: " + ofToString(individualLum, 2), centerX - 50, labelY + 60);
	}
}

void LEDVisualizer::drawController(float x, float y, float radius,
	const ofColor & rgbColor, int blend, int origin, int arc,
	float globalLum, float individualLum,
	const std::string & label) {
	// Draw background circle for reference
	ofSetColor(30, 30, 30);
	ofDrawCircle(x, y, radius + 10);

	// Draw the 3-circle LED system
	drawLEDCircles(x, y, rgbColor, blend, origin, arc, globalLum, individualLum);

	// Draw center point
	ofSetColor(100, 100, 100);
	ofDrawCircle(x, y, 2); // Bigger center point

	// Draw controller label
	if (showLabels && !label.empty()) {
		ofSetColor(255);
		ofDrawBitmapString(label, x - 15, y - radius - 20);

		// Draw parameter values
		drawParameterDisplay(x, y + radius + 20, blend, origin, arc, globalLum, individualLum);
	}

	// Draw RGB color indicator
	ofSetColor(rgbColor);
	ofDrawRectangle(x - 20, y + radius + 35, 40, 8);

	// Draw final brightness indicator (after luminosity calculations)
	float finalBrightness = globalLum * individualLum;
	ofColor finalColor = ofColor(rgbColor.r * finalBrightness,
		rgbColor.g * finalBrightness,
		rgbColor.b * finalBrightness);
	ofSetColor(finalColor);
	ofDrawRectangle(x - 20, y + radius + 45, 40, 8);

	if (showLabels) {
		ofSetColor(200);
		ofDrawBitmapString("RGB", x - 50, y + radius + 42);
		ofDrawBitmapString("Final", x - 50, y + radius + 52);
	}
}

void LEDVisualizer::drawLEDCircles(float centerX, float centerY,
	const ofColor & rgbColor, int blend, int origin, int arc,
	float globalLum, float individualLum) {
	// Calculate which circles should be active based on blend value
	for (int circleIndex = 0; circleIndex < 3; circleIndex++) {
		float alpha = getCircleAlpha(circleIndex, blend);

		if (alpha > 0.01f) { // Only draw if visible
			float radius = (circleIndex == 0) ? CIRCLE_1_RADIUS : (circleIndex == 1) ? CIRCLE_2_RADIUS
																					 : CIRCLE_3_RADIUS;
			int numLeds = (circleIndex == 0) ? NUM_LEDS_CIRCLE_1 : (circleIndex == 1) ? NUM_LEDS_CIRCLE_2
																					  : NUM_LEDS_CIRCLE_3;

			// Apply luminosity to the alpha
			float finalAlpha = alpha * globalLum * individualLum;

			drawSingleCircle(centerX, centerY, radius, numLeds, rgbColor, finalAlpha, origin, arc);

			// Draw arc indicators if significant alpha
			if (finalAlpha > 0.2f) {
				drawArcIndicators(centerX, centerY, radius, origin, arc, rgbColor, finalAlpha * 0.5f);
			}
		}
	}
}

void LEDVisualizer::drawSingleCircle(float centerX, float centerY, float radius, int numLeds,
	const ofColor & baseColor, float alpha,
	int origin, int arc) {
	for (int i = 0; i < numLeds; i++) {
		float angle = ofMap(i, 0, numLeds, 0, TWO_PI);
		float angleDegrees = ofRadToDeg(angle);

		if (isAngleInArc(angleDegrees, origin, arc)) {
			float x = centerX + cos(angle) * radius;
			float y = centerY + sin(angle) * radius;

			// Draw LED glow
			drawGlow(x, y, 6, baseColor, alpha * 0.3f);

			// Draw LED center
			ofSetColor(baseColor.r, baseColor.g, baseColor.b, alpha * 255);
			ofDrawCircle(x, y, 3);

			// Draw bright center
			ofSetColor(255, 255, 255, alpha * 200);
			ofDrawCircle(x, y, 1);
		}
	}

	// Draw reference circle
	ofSetColor(baseColor.r, baseColor.g, baseColor.b, alpha * 60);
	ofNoFill();
	ofDrawCircle(centerX, centerY, radius);
	ofFill();
}

void LEDVisualizer::drawArcIndicators(float centerX, float centerY, float radius,
	int origin, int arc, const ofColor & color, float alpha) {
	ofSetColor(color.r, color.g, color.b, alpha * 255);

	// Draw origin line
	float originRad = ofDegToRad(origin);
	float originX = centerX + cos(originRad) * radius;
	float originY = centerY + sin(originRad) * radius;
	ofDrawLine(centerX, centerY, originX, originY);

	// Draw arc end line
	float endAngle = origin + arc;
	float endRad = ofDegToRad(endAngle);
	float endX = centerX + cos(endRad) * radius;
	float endY = centerY + sin(endRad) * radius;
	ofDrawLine(centerX, centerY, endX, endY);
}

bool LEDVisualizer::isAngleInArc(float currentAngleDegrees, int startAngleDegrees, int arcSpanDegrees) {
	// Normalize all angles to 0-360 range
	currentAngleDegrees = normalizeAngle(currentAngleDegrees);
	startAngleDegrees = normalizeAngle(startAngleDegrees);

	// Calculate end angle based on origin and span
	// Ensure arcSpanDegrees is treated correctly, e.g. if it can be > 360 or negative from effect.
	// For simplicity, let's assume arcSpanDegrees is intended to be 0-360.
	arcSpanDegrees = std::max(0, std::min(arcSpanDegrees, 360)); // Clamp span to 0-360

	if (arcSpanDegrees == 360) { // Full circle if span is 360
		return true;
	}
	if (arcSpanDegrees == 0) { // Nothing if span is 0
		return false;
	}

	int endAngleDegreesCalculated = normalizeAngle(static_cast<float>(startAngleDegrees) + arcSpanDegrees);

	if (startAngleDegrees <= endAngleDegreesCalculated) {
		// Arc doesn't cross 0 degrees
		return currentAngleDegrees >= startAngleDegrees && currentAngleDegrees <= endAngleDegreesCalculated;
	} else {
		// Arc crosses 0 degrees (e.g., from 350° to 10°)
		return currentAngleDegrees >= startAngleDegrees || currentAngleDegrees <= endAngleDegreesCalculated;
	}
}

float LEDVisualizer::normalizeAngle(float angle) {
	while (angle < 0)
		angle += 360;
	while (angle >= 360)
		angle -= 360;
	return angle;
}

float LEDVisualizer::getCircleAlpha(int circleIndex, int blend) {
	// Map blend value (0-768) to circle selection like in the simulator
	float normalizedBlend = ofMap(blend, 0, 768, 0.0f, 1.0f);
	normalizedBlend = ofClamp(normalizedBlend, 0.0f, 1.0f);

	if (normalizedBlend <= 0.5f) {
		// Between 0 and 384: transition from circle 0 to circle 1
		if (circleIndex == 0) {
			return 1.0f - (normalizedBlend * 2.0f);
		} else if (circleIndex == 1) {
			return normalizedBlend * 2.0f;
		} else {
			return 0.0f;
		}
	} else {
		// Between 384 and 768: transition from circle 1 to circle 2
		if (circleIndex == 0) {
			return 0.0f;
		} else if (circleIndex == 1) {
			return 1.0f - ((normalizedBlend - 0.5f) * 2.0f);
		} else {
			return (normalizedBlend - 0.5f) * 2.0f;
		}
	}
}

void LEDVisualizer::drawGlow(float x, float y, float radius, const ofColor & color, float alpha) {
	for (int i = 0; i < 3; i++) {
		float glowRadius = radius + (i * 4);
		float glowAlpha = alpha * (1.0f - (i * 0.3f));
		ofSetColor(color.r, color.g, color.b, glowAlpha * 255);
		ofDrawCircle(x, y, glowRadius);
	}
}

void LEDVisualizer::drawLabel(float x, float y, const std::string & text, const ofColor & color) {
	ofSetColor(color);
	ofDrawBitmapString(text, x, y);
}

void LEDVisualizer::drawParameterDisplay(float x, float y, int blend, int origin, int arc,
	float globalLum, float individualLum) {
	ofSetColor(180, 180, 180);
	string params = "B:" + ofToString(blend) + " O:" + ofToString(origin) + "° A:" + ofToString(arc) + "°";
	ofDrawBitmapString(params, x - 60, y);

	string lums = "G:" + ofToString(globalLum, 2) + " I:" + ofToString(individualLum, 2);
	ofDrawBitmapString(lums, x - 60, y + 12);
}

// Configuration methods
void LEDVisualizer::setSize(int width, int height) {
	vizWidth = width;
	vizHeight = height;
	calculateLayout();
}

void LEDVisualizer::setBackgroundColor(const ofColor & color) {
	backgroundColor = color;
}

void LEDVisualizer::setShowLabels(bool show) {
	showLabels = show;
}

void LEDVisualizer::setShowGrid(bool show) {
	showGrid = show;
}

void LEDVisualizer::setLayoutMode(int mode) {
	layoutMode = mode;
	calculateLayout();
}

void LEDVisualizer::setControllerSpacing(float spacing) {
	controllerSpacing = spacing;
	calculateLayout();
}

void LEDVisualizer::setUIWrapper(UIWrapper * wrapper) {
	uiWrapper = wrapper;
}

// HourGlass management
void LEDVisualizer::addHourGlass(HourGlass * hg, const std::string & label) {
	if (!hg) return;

	// Check if already added
	for (const auto & hgViz : hourglasses) {
		if (hgViz.hourglass == hg) return;
	}

	HourGlassVisualization viz;
	viz.hourglass = hg;
	viz.label = label;
	viz.lastUpdateTime = ofGetElapsedTimef();

	hourglasses.push_back(viz);
	calculateLayout();
}

void LEDVisualizer::removeHourGlass(HourGlass * hg) {
	hourglasses.erase(
		std::remove_if(hourglasses.begin(), hourglasses.end(),
			[hg](const HourGlassVisualization & viz) { return viz.hourglass == hg; }),
		hourglasses.end());
	calculateLayout();
}

void LEDVisualizer::clearHourGlasses() {
	hourglasses.clear();
}

void LEDVisualizer::calculateLayout() {
	if (hourglasses.empty()) return;

	int numHourglasses = hourglasses.size();

	switch (layoutMode) {
	case 0: { // Grid layout
		int cols = ceil(sqrt(numHourglasses));
		int rows = ceil(float(numHourglasses) / cols);

		for (int i = 0; i < numHourglasses; i++) {
			int row = i / cols;
			int col = i % cols;

			float x = (vizWidth / (cols + 1)) * (col + 1);
			float y = (vizHeight / (rows + 1)) * (row + 1);

			hourglasses[i].position.set(x, y);
		}
		break;
	}
	case 1: { // Horizontal layout
		for (int i = 0; i < numHourglasses; i++) {
			float x = (vizWidth / (numHourglasses + 1)) * (i + 1);
			float y = vizHeight / 2;
			hourglasses[i].position.set(x, y);
		}
		break;
	}
	case 2: { // Vertical layout
		for (int i = 0; i < numHourglasses; i++) {
			float x = vizWidth / 2;
			float y = (vizHeight / (numHourglasses + 1)) * (i + 1);
			hourglasses[i].position.set(x, y);
		}
		break;
	}
	}
}

void LEDVisualizer::drawTinyController(float x, float y, const ofColor & rgbColor,
	int blend, int originDegrees, int arcEndDegrees,
	float globalLum, float individualLum) {

	// Bigger circle radii for better visibility
	const float radii[] = { 12, 18, 24 }; // Inner, Middle, Outer
	const int numLedsPerCircle[] = { NUM_LEDS_CIRCLE_1, NUM_LEDS_CIRCLE_2, NUM_LEDS_CIRCLE_3 };

	float finalBrightness = globalLum * individualLum;

	for (int circleIndex = 0; circleIndex < 3; circleIndex++) {
		float alpha = getCircleAlpha(circleIndex, blend);
		if (alpha < 0.01f) continue;

		float radius = radii[circleIndex];
		int numLeds = numLedsPerCircle[circleIndex];
		float finalAlpha = alpha * finalBrightness;

		// Draw circle outline
		ofSetColor(rgbColor.r, rgbColor.g, rgbColor.b, finalAlpha * 100);
		ofNoFill();
		ofDrawCircle(x, y, radius);
		ofFill();

		// Draw the LEDs for this circle
		for (int i = 0; i < numLeds; i++) {
			float ledAngleDegrees = ofMap(i, 0, numLeds, 0, 360);
			float correctedLedAngleDegrees = normalizeAngle(ledAngleDegrees - 90.0f); // Shift by -90 to make 0 at top

			if (isAngleInArc(correctedLedAngleDegrees, originDegrees, arcEndDegrees)) {
				float angleRad = ofDegToRad(ledAngleDegrees); // Use original angle for drawing position
				float ledX = x + cos(angleRad) * radius;
				float ledY = y + sin(angleRad) * radius;

				ofSetColor(rgbColor.r, rgbColor.g, rgbColor.b, finalAlpha * 255);
				ofDrawCircle(ledX, ledY, 2); // LED dot
			}
		}
	}

	// Draw center point
	ofSetColor(100, 100, 100);
	ofDrawCircle(x, y, 2); // Bigger center point
}