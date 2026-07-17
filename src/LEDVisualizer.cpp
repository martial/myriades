#include "LEDVisualizer.h"
#include "LedGeometry.h"
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
	// Update last update time for each tracked hourglass
	float currentTime = ofGetElapsedTimef();
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
	ofColor upColor = hg->upLedColor.get();
	int upBlend = hg->upLedBlend.get(), upOrigin = hg->upLedOrigin.get(), upArc = hg->upLedArc.get();

	LedMagnetController * upMagnet = hg->getUpLedMagnet();
	if (upMagnet && upMagnet->isRgbInitialized()) {
		// Prefer the post-effects values actually sent to hardware
		upBlend = upMagnet->getLastSentBlend();
		upOrigin = upMagnet->getLastSentOrigin();
		upArc = upMagnet->getLastSentArc();
	}
	drawTinyController(50, 60, upColor, upBlend, upOrigin, upArc, globalLum, individualLum);

	// Draw DOWN controller (right)
	ofColor downColor = hg->downLedColor.get();
	int downBlend = hg->downLedBlend.get(), downOrigin = hg->downLedOrigin.get(), downArc = hg->downLedArc.get();

	LedMagnetController * downMagnet = hg->getDownLedMagnet();
	if (downMagnet && downMagnet->isRgbInitialized()) {
		downBlend = downMagnet->getLastSentBlend();
		downOrigin = downMagnet->getLastSentOrigin();
		downArc = downMagnet->getLastSentArc();
	}
	drawTinyController(150, 60, downColor, downBlend, downOrigin, downArc, globalLum, individualLum);

	ofPopMatrix();
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
	const int numLedsPerCircle[] = { LedGeometry::NUM_LEDS_CIRCLE_1, LedGeometry::NUM_LEDS_CIRCLE_2, LedGeometry::NUM_LEDS_CIRCLE_3 };

	float finalBrightness = globalLum * individualLum;

	for (int circleIndex = 0; circleIndex < 3; circleIndex++) {
		float alpha = LedGeometry::circleAlphaForBlend(circleIndex, blend);
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
			float correctedLedAngleDegrees = LedGeometry::normalizeAngle(ledAngleDegrees - 90.0f); // Shift by -90 to make 0 at top

			if (LedGeometry::isAngleInArc(correctedLedAngleDegrees, originDegrees, arcEndDegrees)) {
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
