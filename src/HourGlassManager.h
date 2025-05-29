#pragma once

#include "HourGlass.h"
#include "ofMain.h"
#include <memory>
#include <vector>

class HourGlassManager {
public:
	HourGlassManager();
	~HourGlassManager();

	// Configuration management
	bool loadConfiguration(const std::string & configFile = "hourglasses.json");
	bool saveConfiguration(const std::string & configFile = "hourglasses.json");
	void createDefaultConfiguration();

	// HourGlass management
	void addHourGlass(const std::string & name, int upLedId, int downLedId, int motorId);
	bool removeHourGlass(const std::string & name);
	HourGlass * getHourGlass(const std::string & name);
	HourGlass * getHourGlass(size_t index);

	// Connection management
	bool connectAll();
	bool connectHourGlass(const std::string & name);
	void disconnectAll();
	void disconnectHourGlass(const std::string & name);

	// Bulk operations
	void enableAllMotors();
	void disableAllMotors();
	void emergencyStopAll();
	void setAllLEDs(uint8_t r, uint8_t g, uint8_t b);

	// Status and info
	size_t getHourGlassCount() const { return hourglasses.size(); }
	std::vector<std::string> getHourGlassNames() const;
	std::vector<std::string> getAvailableSerialPorts() const;

	// Configuration access
	const std::vector<std::unique_ptr<HourGlass>> & getHourGlasses() const { return hourglasses; }

private:
	std::vector<std::unique_ptr<HourGlass>> hourglasses;
	std::string configFilePath;

	// Shared serial port configuration
	std::string sharedSerialPort;
	int sharedBaudRate;

	// JSON helpers
	ofJson createHourGlassJson(const HourGlass & hourglass) const;
	bool parseHourGlassJson(const ofJson & json);
};