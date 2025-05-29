#pragma once

#include "ofMain.h"
#include <deque>
#include <memory>
#include <string>
#include <unordered_map>

// Abstract interface for serial communication
class ISerialPort {
public:
	virtual ~ISerialPort() = default;

	// Core serial operations
	virtual bool setup(int deviceIndex, int baudRate) = 0;
	virtual bool isInitialized() const = 0;
	virtual void writeByte(uint8_t byte) = 0;
	virtual void writeBytes(const uint8_t * data, size_t length) = 0;
	virtual void close() = 0;

	// Device information
	virtual std::string getDeviceName() const = 0;
	virtual int getBaudRate() const = 0;
};

// Concrete implementation wrapping ofSerial
class OfSerialPort : public ISerialPort {
public:
	OfSerialPort();
	~OfSerialPort() override;

	bool setup(int deviceIndex, int baudRate) override;
	bool isInitialized() const override;
	void writeByte(uint8_t byte) override;
	void writeBytes(const uint8_t * data, size_t length) override;
	void close() override;

	std::string getDeviceName() const override;
	int getBaudRate() const override;

private:
	ofSerial serial;
	std::string deviceName;
	int baudRate;
	bool initialized;
};

// Manages shared access to serial ports
class SerialPortManager {
public:
	struct SerialStats {
		int callsThisFrame = 0;
		int bytesThisFrame = 0;
		int totalCalls = 0;
		int totalBytes = 0;
		float avgCallsPerSecond = 0.0f;
		float avgBytesPerSecond = 0.0f;
		float lastUpdateTime = 0.0f;

		// Rolling averages
		float avgCallsPerSecond_1s = 0.0f; // 1 second average
		float avgBytesPerSecond_1s = 0.0f;
		float avgCallsPerSecond_5s = 0.0f; // 5 second average
		float avgBytesPerSecond_5s = 0.0f;
		float avgCallsPerFrame_60f = 0.0f; // 60 frame average
		float avgBytesPerFrame_60f = 0.0f;
	};

	static SerialPortManager & getInstance();

	// Get shared access to a port
	std::shared_ptr<ISerialPort> getPort(const std::string & portName, int baudRate = 230400);

	// Release port (automatically handled by shared_ptr)
	void releasePort(const std::string & portName);

	// Utility functions
	std::vector<std::string> getAvailablePorts();
	bool isPortInUse(const std::string & portName) const;

	// Statistics
	SerialStats getStats() const;
	void updateStats(); // Call once per frame to update averages
	void trackWrite(size_t bytes); // Track a write operation

private:
	SerialPortManager() = default;
	~SerialPortManager() = default;

	// Non-copyable
	SerialPortManager(const SerialPortManager &) = delete;
	SerialPortManager & operator=(const SerialPortManager &) = delete;

	std::unordered_map<std::string, std::weak_ptr<ISerialPort>> activePorts;
	mutable std::mutex portsMutex;

	// Statistics tracking
	mutable SerialStats stats;
	mutable std::mutex statsMutex;

	// Rolling average data
	mutable std::deque<std::pair<float, int>> callHistory; // time, calls
	mutable std::deque<std::pair<float, int>> byteHistory; // time, bytes
	mutable std::deque<int> frameCallHistory; // calls per frame (last 60)
	mutable std::deque<int> frameByteHistory; // bytes per frame (last 60)

	void updateRollingAverages(); // Helper to calculate rolling averages
};