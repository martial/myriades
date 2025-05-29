#pragma once

#include "SerialPortManager.h"
#include "ofMain.h"
#include <memory>

class LedMagnetController {
public:
	// Control channel types
	enum class ControlType : uint8_t {
		LED = 1,
		PWM = 2,
		DOTSTAR = 3
	};

	// Connection result enum for better error handling
	enum class ConnectionResult {
		SUCCESS,
		DEVICE_NOT_FOUND,
		CONNECTION_FAILED,
		INVALID_INDEX,
		PORT_IN_USE
	};

	// Constructor with dependency injection
	explicit LedMagnetController(std::shared_ptr<ISerialPort> serialPort = nullptr);
	~LedMagnetController();

	// Setup and connection
	ConnectionResult connect(const std::string & portName, int baudRate = 230400);
	ConnectionResult connect(int deviceIndex, int baudRate = 230400);
	bool isConnected() const;
	void disconnect();

	// Device management
	std::vector<std::string> getAvailableDevices();
	std::string getConnectedDeviceName() const;

	// Fluent interface for configuration
	LedMagnetController & setId(int _id);
	LedMagnetController & setExtended(bool extended);
	LedMagnetController & setRemote(bool remote);

	// Control functions with fluent interface
	LedMagnetController & sendLED(uint8_t value);
	LedMagnetController & sendLED(uint8_t r, uint8_t g, uint8_t b, bool enabled = true);
	LedMagnetController & sendPWM(uint8_t value);
	LedMagnetController & sendDotStar(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness = 255);
	LedMagnetController & sendDotStar(const ofColor & color, uint8_t brightness = 255);

	// Generic send method
	bool send(ControlType type, const std::vector<uint8_t> & data);
	bool send(const std::vector<uint8_t> & data);

	// Current protocol state
	int getCurrentId() const { return id; }
	bool isExtended() const { return ext; }
	bool isRemote() const { return rtr; }

private:
	std::shared_ptr<ISerialPort> serialPort;
	std::string connectedPortName;

	// Protocol parameters
	int id = 11;
	bool ext = false;
	bool rtr = false;
	static constexpr uint8_t START_BYTE = 0xE7; // 231
	static constexpr uint8_t END_BYTE = 0x7E; // 126

	// Build protocol packet
	std::vector<uint8_t> buildPacket(const std::vector<uint8_t> & data) const;

	// Helper to clamp values
	template <typename T>
	static T clamp(T value, T min, T max) {
		return value < min ? min : (value > max ? max : value);
	}
};