#pragma once

#include "HourGlass.h"
#include "ofMain.h"
#include <vector>

class LEDVisualizer {
public:
	LEDVisualizer();
	~LEDVisualizer();

	// Setup and configuration
	void setup(int width = 800, int height = 600);
	void update();
	void draw(float x = 0, float y = 0);

	// Set UIWrapper reference to access LED effect parameters
	void setUIWrapper(class UIWrapper * uiWrapper);

	// Configuration
	void setSize(int width, int height);
	void setBackgroundColor(const ofColor & color);
	void setShowLabels(bool show);
	void setShowGrid(bool show);

	// Add/remove hourglasses to visualize
	void addHourGlass(HourGlass * hg, const std::string & label = "");
	void removeHourGlass(HourGlass * hg);
	void clearHourGlasses();

	// Layout options
	void setLayoutMode(int mode); // 0=grid, 1=horizontal, 2=vertical
	void setControllerSpacing(float spacing);

	// Real-time visualization of specific controller
	void drawController(float x, float y, float radius,
		const ofColor & rgbColor, int blend, int origin, int arc,
		float globalLum, float individualLum,
		const std::string & label = "");

	// Compact preview controller
	void drawTinyController(float x, float y, const ofColor & rgbColor,
		int blend, int origin, int arc, float globalLum, float individualLum);

private:
	// Visualization settings
	int vizWidth, vizHeight;
	ofColor backgroundColor;
	bool showLabels;
	bool showGrid;
	int layoutMode;
	float controllerSpacing;

	// UI parameter access
	class UIWrapper * uiWrapper;

	// LED Circle System Constants (matching hardware)
	static const int NUM_LEDS_CIRCLE_1 = 32; // Inner circle
	static const int NUM_LEDS_CIRCLE_2 = 36; // Middle circle
	static const int NUM_LEDS_CIRCLE_3 = 42; // Outer circle

	// Visual parameters
	static constexpr float CIRCLE_1_RADIUS = 40.0f;
	static constexpr float CIRCLE_2_RADIUS = 65.0f;
	static constexpr float CIRCLE_3_RADIUS = 90.0f;

	// Tracked hourglasses
	struct HourGlassVisualization {
		HourGlass * hourglass;
		std::string label;
		ofVec2f position;
		float lastUpdateTime;
	};

	std::vector<HourGlassVisualization> hourglasses;

	// Helper methods
	void calculateLayout();
	void drawHourGlassControllers(const HourGlassVisualization & hgViz);
	void drawLEDCircles(float centerX, float centerY,
		const ofColor & rgbColor, int blend, int origin, int arc,
		float globalLum, float individualLum);

	void drawSingleCircle(float centerX, float centerY, float radius, int numLeds,
		const ofColor & baseColor, float alpha,
		int origin, int arc);

	void drawArcIndicators(float centerX, float centerY, float radius,
		int origin, int arc, const ofColor & color, float alpha);

	bool isAngleInArc(float currentAngleDegrees, int startAngleDegrees, int endAngleDegrees);
	float normalizeAngle(float angle);
	float getCircleAlpha(int circleIndex, int blend);

	// Visual effects
	void drawGlow(float x, float y, float radius, const ofColor & color, float alpha);
	void drawLabel(float x, float y, const std::string & text, const ofColor & color = ofColor::white);
	void drawParameterDisplay(float x, float y, int blend, int origin, int arc,
		float globalLum, float individualLum);
};