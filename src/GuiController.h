#pragma once

#include "CanProtocol.h"
#include "ofMain.h"
#include "ofxGui.h"

// Forward declarations for cleaner headers
class CanControlPanel;
class DevicePanel;
class StatusPanel;

class GuiController {
public:
	GuiController();
	~GuiController();

	// Main lifecycle - simple and clear
	void setup(CanProtocol * protocol);
	void update();
	void draw();

	// Public interface - only what's needed
	bool isConnected() const;
	void refreshDevices();

private:
	// Core components - composition over inheritance
	std::unique_ptr<CanControlPanel> canControls;
	std::unique_ptr<DevicePanel> deviceControls;
	std::unique_ptr<StatusPanel> statusDisplay;

	// Single responsibility
	CanProtocol * canProtocol;
	bool initialized;

	// Clean initialization
	void initializePanels();
	void connectToFirstDevice();
};