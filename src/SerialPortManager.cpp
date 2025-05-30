#include "SerialPortManager.h"
#include <mutex>

// OfSerialPort Implementation
OfSerialPort::OfSerialPort()
	: baudRate(0)
	, initialized(false) {
}

OfSerialPort::~OfSerialPort() {
	close();
}

bool OfSerialPort::setup(int deviceIndex, int baudRate) {
	this->baudRate = baudRate;

	// Get device list to find device name
	serial.listDevices();
	auto devices = serial.getDeviceList();

	if (deviceIndex < 0 || deviceIndex >= devices.size()) {
		ofLogError("OfSerialPort") << "Invalid device index: " << deviceIndex;
		return false;
	}

	deviceName = devices[deviceIndex].getDeviceName();

	// Setup the serial connection
	bool success = serial.setup(deviceIndex, baudRate);
	if (success) {
		initialized = true;
		// ofLogNotice("OfSerialPort") << "Connected to: " << deviceName << " at " << baudRate << " baud";
	} else {
		ofLogError("OfSerialPort") << "Failed to connect to: " << deviceName;
		initialized = false;
	}

	return success;
}

bool OfSerialPort::isInitialized() const {
	return initialized && serial.isInitialized();
}

void OfSerialPort::writeByte(uint8_t byte) {
	if (isInitialized()) {
		serial.writeByte(byte);
	}
}

void OfSerialPort::writeBytes(const uint8_t * data, size_t length) {
	if (isInitialized()) {
		// Track statistics
		SerialPortManager::getInstance().trackWrite(length);

		serial.writeBytes(data, length);
	}
}

void OfSerialPort::close() {
	if (initialized) {
		serial.close();
		initialized = false;
		// ofLogNotice("OfSerialPort") << "Closed connection to: " << deviceName;
	}
}

std::string OfSerialPort::getDeviceName() const {
	return deviceName;
}

int OfSerialPort::getBaudRate() const {
	return baudRate;
}

// SerialPortManager Implementation
SerialPortManager & SerialPortManager::getInstance() {
	static SerialPortManager instance;
	return instance;
}

std::shared_ptr<ISerialPort> SerialPortManager::getPort(const std::string & portName, int baudRate) {
	std::lock_guard<std::mutex> lock(portsMutex);

	ofLogNotice("SerialPortManager") << "🔍 Port request: " << portName << " @ " << baudRate << " baud";

	// Check if port is already active
	auto it = activePorts.find(portName);
	if (it != activePorts.end()) {
		auto existingPort = it->second.lock();
		if (existingPort) {
			ofLogNotice("SerialPortManager") << "✅ Reusing existing connection to: " << portName;
			return existingPort;
		} else {
			// Weak pointer expired, remove it
			activePorts.erase(it);
			ofLogWarning("SerialPortManager") << "⚠️  Cleaned up expired connection to: " << portName;
		}
	}

	// Create new port connection
	ofLogNotice("SerialPortManager") << "🔌 Creating NEW connection to: " << portName;
	auto newPort = std::make_shared<OfSerialPort>();

	// Find device index by name
	ofSerial tempSerial;
	tempSerial.listDevices();
	auto devices = tempSerial.getDeviceList();

	int deviceIndex = -1;
	for (int i = 0; i < devices.size(); i++) {
		if (devices[i].getDeviceName() == portName) {
			deviceIndex = i;
			break;
		}
	}

	if (deviceIndex == -1) {
		ofLogError("SerialPortManager") << "Device not found: " << portName;
		return nullptr;
	}

	// Setup the connection
	if (!newPort->setup(deviceIndex, baudRate)) {
		ofLogError("SerialPortManager") << "Failed to setup port: " << portName;
		return nullptr;
	}

	// Store weak reference
	activePorts[portName] = newPort;

	// ofLogNotice("SerialPortManager") << "Created new connection to: " << portName;
	return newPort;
}

void SerialPortManager::releasePort(const std::string & portName) {
	std::lock_guard<std::mutex> lock(portsMutex);

	auto it = activePorts.find(portName);
	if (it != activePorts.end()) {
		activePorts.erase(it);
		// ofLogNotice("SerialPortManager") << "Released port: " << portName;
	}
}

std::vector<std::string> SerialPortManager::getAvailablePorts() {
	ofSerial tempSerial;
	tempSerial.listDevices();
	auto devices = tempSerial.getDeviceList();

	std::vector<std::string> portNames;
	for (auto & device : devices) {
		portNames.push_back(device.getDeviceName());
	}

	return portNames;
}

bool SerialPortManager::isPortInUse(const std::string & portName) const {
	std::lock_guard<std::mutex> lock(portsMutex);

	auto it = activePorts.find(portName);
	if (it != activePorts.end()) {
		return !it->second.expired();
	}

	return false;
}

void SerialPortManager::trackWrite(size_t bytes) {
	std::lock_guard<std::mutex> lock(statsMutex);
	stats.callsThisFrame++;
	stats.bytesThisFrame += bytes;
	stats.totalCalls++;
	stats.totalBytes += bytes;
}

SerialPortManager::SerialStats SerialPortManager::getStats() const {
	std::lock_guard<std::mutex> lock(statsMutex);
	return stats;
}

void SerialPortManager::updateStats() {
	std::lock_guard<std::mutex> lock(statsMutex);
	float currentTime = ofGetElapsedTimef();
	float deltaTime = currentTime - stats.lastUpdateTime;

	if (deltaTime > 0.0f) {
		stats.avgCallsPerSecond = stats.callsThisFrame / deltaTime;
		stats.avgBytesPerSecond = stats.bytesThisFrame / deltaTime;
	}

	// Store frame data for rolling averages
	frameCallHistory.push_back(stats.callsThisFrame);
	frameByteHistory.push_back(stats.bytesThisFrame);
	callHistory.push_back({ currentTime, stats.callsThisFrame });
	byteHistory.push_back({ currentTime, stats.bytesThisFrame });

	// Keep frame history to 60 frames
	if (frameCallHistory.size() > 60) frameCallHistory.pop_front();
	if (frameByteHistory.size() > 60) frameByteHistory.pop_front();

	// Clean old time-based history (keep last 5 seconds)
	while (!callHistory.empty() && currentTime - callHistory.front().first > 5.0f) {
		callHistory.pop_front();
	}
	while (!byteHistory.empty() && currentTime - byteHistory.front().first > 5.0f) {
		byteHistory.pop_front();
	}

	// Calculate rolling averages
	updateRollingAverages();

	// Reset frame counters
	stats.callsThisFrame = 0;
	stats.bytesThisFrame = 0;
	stats.lastUpdateTime = currentTime;
}

void SerialPortManager::updateRollingAverages() {
	float currentTime = ofGetElapsedTimef();

	// Calculate 1-second average
	int calls1s = 0, bytes1s = 0;
	for (const auto & entry : callHistory) {
		if (currentTime - entry.first <= 1.0f) {
			calls1s += entry.second;
		}
	}
	for (const auto & entry : byteHistory) {
		if (currentTime - entry.first <= 1.0f) {
			bytes1s += entry.second;
		}
	}
	stats.avgCallsPerSecond_1s = calls1s; // Already per second
	stats.avgBytesPerSecond_1s = bytes1s;

	// Calculate 5-second average
	int calls5s = 0, bytes5s = 0;
	for (const auto & entry : callHistory) {
		calls5s += entry.second;
	}
	for (const auto & entry : byteHistory) {
		bytes5s += entry.second;
	}
	float duration = callHistory.empty() ? 1.0f : std::max(1.0f, currentTime - callHistory.front().first);
	stats.avgCallsPerSecond_5s = calls5s / duration;
	stats.avgBytesPerSecond_5s = bytes5s / duration;

	// Calculate 60-frame average
	int totalFrameCalls = 0, totalFrameBytes = 0;
	for (int calls : frameCallHistory)
		totalFrameCalls += calls;
	for (int bytes : frameByteHistory)
		totalFrameBytes += bytes;

	int frameCount = std::max(1, (int)frameCallHistory.size());
	stats.avgCallsPerFrame_60f = (float)totalFrameCalls / frameCount;
	stats.avgBytesPerFrame_60f = (float)totalFrameBytes / frameCount;
}