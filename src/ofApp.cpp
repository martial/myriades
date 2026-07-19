#include "ofApp.h"

//--------------------------------------------------------------
void ofApp::setup() {
	// Resolve data relative to the executable (cwd is "/" when launched from
	// Finder). Prefer an external data/ folder next to the .app; for a bare
	// .app, seed a writable copy of the bundled Resources/data into
	// Application Support on first run. Settings saves must never write into
	// the signed bundle — that breaks its codesign seal.
	{
		std::string exeDir = ofFilePath::getCurrentExeDir();
		std::string externalData = exeDir + "../../../data/";
		std::string bundledData = exeDir + "../Resources/data/";
		if (ofDirectory(externalData).exists()) {
			ofSetDataPathRoot(externalData);
		} else if (ofDirectory(bundledData).exists()) {
			std::string appSupportData = ofFilePath::getUserHomeDir()
				+ "/Library/Application Support/Myriades/data";
			if (!ofDirectory(appSupportData).exists()
				&& !ofDirectory(bundledData).copyTo(appSupportData, false, false)) {
				ofLogError("ofApp") << "Could not seed " << appSupportData
									<< ", using read-only bundled data";
				ofSetDataPathRoot(bundledData);
			} else {
				ofSetDataPathRoot(appSupportData + "/");
			}
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
