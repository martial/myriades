#pragma once

#include "HourGlassManager.h"
#include "OSCController.h"
#include "UIWrapper.h"
#include "VezerPlayer.h"
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

	// Vezér XML sequence playback (feeds messages into oscController).
	// Declared before `ui`: UIWrapper's destructor persists sequencer state,
	// so the player must outlive it (members are destroyed in reverse order).
	VezerPlayer vezerPlayer;

	// UI wrapper handles all UI
	UIWrapper ui;

	// OSC controller for remote control
	OSCController oscController;
};
