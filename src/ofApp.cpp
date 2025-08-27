#include "ofApp.h"

//--------------------------------------------------------------
void ofApp::setup() {
	ofSetWindowTitle("Myriades Control System v1.0");
	ofSetFrameRate(60);
	ofSetWindowShape(1200, 800);

	// Basic setup for smooth rendering
	ofEnableAntiAliasing();
	ofEnableSmoothing();

	// Initialize HourGlass system (OSC Out is configured automatically from hourglasses.json)
	ofLogNotice("ofApp") << "Initializing HourGlass system";
	hourglassManager.loadConfiguration("hourglasses.json");
	hourglassManager.connectAll();

	// Setup UI with references to core components
	ui.setup(&hourglassManager, &oscController);

	// Initialize OSC controller
	oscController.setup(8000); // Default receive port
	oscController.setUIWrapper(&ui); // Enable UI position parameter synchronization
	oscController.setEnabled(true);
	ofLogNotice("ofApp") << "🎛️ OSC Controller initialized on port 8000 (receiver only) with UI sync";

	ofLogNotice("ofApp") << "Setup complete";
}

//--------------------------------------------------------------
void ofApp::update() {
	ofSetLogLevel(OF_LOG_VERBOSE);

	// Update OSC controller (process incoming messages)
	oscController.update();

	// Update UI
	ui.update();

}

//--------------------------------------------------------------
void ofApp::draw() {
	ofBackgroundGradient(ofColor(30, 30, 35), ofColor(15, 15, 20));

	// Draw UI (panels and status)
	ui.draw();
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key) {
	// Delegate all keyboard handling to UI
	ui.handleKeyPressed(key);
}

//--------------------------------------------------------------
void ofApp::keyReleased(int key) { }
void ofApp::mouseMoved(int x, int y) { }
void ofApp::mouseDragged(int x, int y, int button) { }
void ofApp::mousePressed(int x, int y, int button) { }
void ofApp::mouseReleased(int x, int y, int button) { }
void ofApp::mouseEntered(int x, int y) { }
void ofApp::mouseExited(int x, int y) { }
void ofApp::windowResized(int w, int h) { }
void ofApp::gotMessage(ofMessage msg) { }
void ofApp::dragEvent(ofDragInfo dragInfo) { }
