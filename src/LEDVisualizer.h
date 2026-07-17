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

	// Set UIWrapper reference to access the current hourglass selection
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

	// UI selection access
	class UIWrapper * uiWrapper;

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
};
