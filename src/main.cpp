#include "ofApp.h"
#include "ofMain.h"

//========================================================================
int main() {

	ofGLWindowSettings settings;
	settings.setGLVersion(4, 0);

	// Six 225px panels + the stop button across the top (screen is 1800pt wide)
	settings.setSize(1740, 850);

#ifdef TARGET_OSX
	settings.windowMode = OF_WINDOW;
#endif

	settings.title = "Myriades";

	auto window = ofCreateWindow(settings);

	ofRunApp(window, std::make_shared<ofApp>());
	ofRunMainLoop();
}
