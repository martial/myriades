#include "ofApp.h"
#include "ofMain.h"

//========================================================================
int main() {

	ofGLWindowSettings settings;
	settings.setGLVersion(4, 0);

	// Set window size
	settings.setSize(1200, 800);

// Enable HiDPI support for Retina displays
#ifdef TARGET_OSX
	settings.windowMode = OF_WINDOW; // Windowed mode
	// Note: HiDPI support may be automatic in your OF version
#endif

	// Set window title
	settings.title = "🔧 HourGlass Control System v1.0";

	auto window = ofCreateWindow(settings);

	ofRunApp(window, std::make_shared<ofApp>());
	ofRunMainLoop();
}
