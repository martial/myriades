#include "HourGlass.h"

// Define the static const member for LED command interval (e.g., 50ms for 20 FPS)
const float HourGlass::MIN_LED_COMMAND_INTERVAL_MS = 50.0f;

// Constructor matches HourGlass.h (name only)
HourGlass::HourGlass(const std::string & name)
	: name(name)
	, serialPortName("") // Initialize serialPortName
	, baudRate(0)
	, upLedId(0) // Default initialize IDs
	, downLedId(0)
	, motorId(0)
	, connected(false)
	, updatingFromOSC(false)
	, lastLedCommandSendTime(0.0f)
	, motorAcceleration("motorAcceleration", 128, 0, 255)
	, individualLuminosity("individualLuminosity", 1.0f, 0.0f, 1.0f) {
	ofLogNotice("HourGlass") << "🏗️ Constructing HourGlass: " << name;

	// Add parameters to the group
	params.add(motorEnabled);
	params.add(microstep);
	params.add(motorSpeed);
	params.add(motorAcceleration);
	params.add(gearRatio);
	params.add(calibrationFactor);
	params.add(upLedColor);
	params.add(downLedColor);
	params.add(upMainLed);
	params.add(downMainLed);
	params.add(upPwm);
	params.add(downPwm);
	params.add(individualLuminosity);
}

HourGlass::~HourGlass() {
	disconnect();
}

void HourGlass::configure(const std::string & serialPort, int baudRate,
	int upLedId, int downLedId, int motorId) {
	this->serialPortName = serialPort;
	this->baudRate = baudRate;
	this->upLedId = upLedId;
	this->downLedId = downLedId;
	this->motorId = motorId;

	ofLogNotice("HourGlass") << "Configured " << name
							 << " - Port: " << serialPort
							 << " - UpLED:" << upLedId
							 << " - DownLED:" << downLedId
							 << " - Motor:" << motorId;
}

bool HourGlass::connect() {
	if (connected) {
		ofLogWarning("HourGlass") << name << " already connected";
		return true;
	}

	if (serialPortName.empty()) {
		ofLogError("HourGlass") << name << " - No serial port configured";
		return false;
	}

	// Get shared serial port
	auto & portManager = SerialPortManager::getInstance();
	sharedSerialPort = portManager.getPort(serialPortName, baudRate);

	if (!sharedSerialPort) {
		ofLogError("HourGlass") << name << " - Failed to get serial port: " << serialPortName;
		return false;
	}

	setupControllers();
	connected = true;

	ofLogNotice("HourGlass") << name << " connected to " << serialPortName;
	return true;
}

bool HourGlass::isConnected() const {
	return connected && sharedSerialPort && sharedSerialPort->isInitialized();
}

void HourGlass::disconnect() {
	if (connected) {
		// Controllers will automatically release the shared port
		if (upLedMagnet) upLedMagnet->disconnect();
		if (downLedMagnet) downLedMagnet->disconnect();
		if (motor) motor->disconnect();

		sharedSerialPort.reset();
		connected = false;

		ofLogNotice("HourGlass") << name << " disconnected";
	}
}

void HourGlass::setupControllers() {
	// Setup Up LED/Magnet Controller
	upLedMagnet = std::make_unique<LedMagnetController>(sharedSerialPort);
	upLedMagnet->setId(upLedId);

	// Setup Down LED/Magnet Controller
	downLedMagnet = std::make_unique<LedMagnetController>(sharedSerialPort);
	downLedMagnet->setId(downLedId);

	// Setup Motor Controller
	motor = std::make_unique<MotorController>(sharedSerialPort);
	motor->setId(motorId);
}

// Convenience methods
void HourGlass::enableMotor() {
	if (isConnected() && motor) {
		motor->enable();
		ofLogNotice("HourGlass") << name << " - Motor enabled";
	}
}

void HourGlass::disableMotor() {
	if (isConnected() && motor) {
		motor->disable();
		ofLogNotice("HourGlass") << name << " - Motor disabled";
	}
}

void HourGlass::emergencyStop() {
	if (isConnected() && motor) {
		motor->emergencyStop();
		ofLogNotice("HourGlass") << name << " - Emergency stop";
	}
}

void HourGlass::setAllLEDs(uint8_t r, uint8_t g, uint8_t b) {
	// Only update parameters, let OSCController::processLastCommands or UI direct calls handle sending
	// Throttling is removed here as direct sending is removed.
	// The responsibility for throttling and actual hardware send is now with:
	//  - UIWrapper listeners (for direct UI interaction)
	//  - OSCController::processLastCommands (for state synchronization from parameters)

	// bool wasUpdatingFromOSC = updatingFromOSC; // This flag is tricky here.
	// If OSC calls this, updatingFromOSC should be true when params are set.
	// If UI calls this (e.g. a preset button that uses setAllLEDs), it might be false.
	// For now, let the caller manage the updatingFromOSC flag if necessary before calling this.

	// It's crucial that if OSC is the origin of this call, 'updatingFromOSC' is true
	// BEFORE ofParameters are set, so listeners in UIWrapper can ignore these changes if needed.
	// OSCController::handleLedMessage for "/led/all/rgb" does set hg->updatingFromOSC = true;

	upLedColor.set(ofColor(r, g, b));
	downLedColor.set(ofColor(r, g, b));
	// ofLogNotice("HourGlass") << name << " - Parameters set by setAllLEDs: R" << (int)r << " G" << (int)g << " B" << (int)b;
}

void HourGlass::applyMotorParameters() {
	if (!isConnected() || !motor) return;

	motor->setMicrostep(microstep.get());

	if (motorEnabled.get()) {
		motor->enable();
	} else {
		motor->disable();
	}
}

void HourGlass::applyLedParameters() {
	if (!connected || !upLedMagnet || !downLedMagnet) return;

	float currentTime = ofGetElapsedTimeMillis();
	if (currentTime - lastLedCommandSendTime < MIN_LED_COMMAND_INTERVAL_MS) {
		return;
	}

	float indivLum = individualLuminosity.get();

	ofColor upColor = upLedColor.get();
	upLedMagnet->sendLED(upColor.r, upColor.g, upColor.b, 0, 0, 360, indivLum); // Default effect parameters

	ofColor downColor = downLedColor.get();
	downLedMagnet->sendLED(downColor.r, downColor.g, downColor.b, 0, 0, 360, indivLum); // Default effect parameters

	upLedMagnet->sendLED(static_cast<uint8_t>(upMainLed.get()), indivLum);
	downLedMagnet->sendLED(static_cast<uint8_t>(downMainLed.get()), indivLum);

	upLedMagnet->sendPWM(static_cast<uint8_t>(upPwm.get()));
	downLedMagnet->sendPWM(static_cast<uint8_t>(downPwm.get()));

	lastLedCommandSendTime = currentTime;

	ofLogVerbose("HourGlass") << name << " - Applied LED parameters to hardware with individual luminosity=" << indivLum;
}