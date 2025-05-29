#pragma once

#include "HourGlassManager.h"
#include "OSCController.h"
#include "UIWrapper.h"
#include "ofMain.h"

class ofApp : public ofBaseApp {
public:
	ofApp()
		: oscController(&hourglassManager) { }

	void setup();
	void update();
	void draw();

	void keyPressed(int key);
	void keyReleased(int key);
	void mouseMoved(int x, int y);
	void mouseDragged(int x, int y, int button);
	void mousePressed(int x, int y, int button);
	void mouseReleased(int x, int y, int button);
	void mouseEntered(int x, int y);
	void mouseExited(int x, int y);
	void windowResized(int w, int h);
	void dragEvent(ofDragInfo dragInfo);
	void gotMessage(ofMessage msg);

private:
	// Core components
	HourGlassManager hourglassManager;

	// UI wrapper handles all UI
	UIWrapper ui;

	// OSC controller for remote control
	OSCController oscController;
};
