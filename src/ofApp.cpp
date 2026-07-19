#include "ofApp.h"

//--------------------------------------------------------------
void ofApp::setup() {
	// Resolve data relative to the executable (cwd is "/" when launched from
	// Finder). Prefer an external data/ folder next to the .app so settings
	// saves never touch the signed bundle; fall back to bundled Resources/data
	// so a bare .app still finds its fonts and settings.
	{
		std::string exeDir = ofFilePath::getCurrentExeDir();
		std::string externalData = exeDir + "../../../data/";
		if (ofDirectory(externalData).exists()) {
			ofSetDataPathRoot(externalData);
		} else {
			ofSetDataPathRoot(exeDir + "../Resources/data/");
		}
	}

	ofSetWindowTitle("Myriades");
	ofSetFrameRate(30);

	// Basic setup for smooth rendering
	ofEnableAntiAliasing();
	ofEnableSmoothing();

	// Initialize HourGlass system (OSC Out is configured automatically from hourglasses.json)

	hourglassManager.loadConfiguration("hourglasses.json");
	hourglassManager.connectAll();

	// Sequencer playback goes through the same pipeline as network OSC
	vezerPlayer.setMessageSink([this](ofxOscMessage & msg) {
		oscController.processMessage(msg);
	});

	// Setup UI with references to core components
	ui.setup(&hourglassManager, &oscController, &vezerPlayer);

	// Initialize OSC controller
	oscController.setup(8000); // Default receive port
	oscController.setUIWrapper(&ui); // Enable UI position parameter synchronization
	oscController.setEnabled(true);
}

//--------------------------------------------------------------
void ofApp::update() {
	// Update OSC controller (process incoming messages)
	oscController.update();

	// Advance sequencer playback (before the hardware tick in ui.update())
	vezerPlayer.update(ofGetLastFrameTime());

	// Update UI
	ui.update();
}

//--------------------------------------------------------------
void ofApp::draw() {
	ofBackgroundGradient(ofColor(18, 20, 24), ofColor(10, 11, 14));

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
void ofApp::mousePressed(int x, int y, int button) { ui.handleMousePressed(x, y); }
void ofApp::mouseReleased(int x, int y, int button) { }
void ofApp::mouseEntered(int x, int y) { }
void ofApp::mouseExited(int x, int y) { }
void ofApp::windowResized(int w, int h) { ui.windowResized(w, h); }
void ofApp::gotMessage(ofMessage msg) { }
void ofApp::dragEvent(ofDragInfo dragInfo) { }
