#pragma once

#include "Effect.h"
#include "EffectParameters.h"
#include "EffectsManager.h"
#include "LedMagnetController.h"
#include "MotorController.h"
#include "OSCOutController.h"

#include "ofMain.h"
#include "ofParameter.h"
#include <memory>
#include <optional>
#include <string>
#include <vector>

class HourGlass {
public:
	// Constructor
	explicit HourGlass(const std::string & name = "HourGlass");
	~HourGlass();

	// Configuration
	void configure(const std::string & serialPort, int baudRate,
		int upLedId, int downLedId, int motorId);

	// Connection management
	bool connect();
	bool isConnected() const;
	void disconnect();

	// OSC Out configuration
	void setupOSCOut(const std::string & configPath = "");
	void setupOSCOutFromJson(const ofJson & oscConfig);
	void enableOSCOut(bool enabled = true);
	bool isOSCOutEnabled() const;
	OSCOutController * getOSCOut() const { return oscOutController.get(); }

	// Access to controllers
	LedMagnetController * getUpLedMagnet() { return upLedMagnet.get(); }
	LedMagnetController * getDownLedMagnet() { return downLedMagnet.get(); }
	MotorController * getMotor() { return motor.get(); }

	// Convenience methods for common operations
	void enableMotor();
	void disableMotor();
	void emergencyStop();
	void setMotorZero();
	void setAllLEDs(uint8_t r, uint8_t g, uint8_t b);

	// Invalidate LED last-sent caches so next applyLedParameters re-sends
	// (needed after global/individual luminosity changes)
	void refreshLedState();

	// Parameter-driven methods for OSC/GUI sync
	void applyMotorParameters();
	void applyLedParameters();

	// Status
	std::string getName() const { return name; }
	std::string getSerialPort() const { return serialPortName; }
	int getBaudRate() const { return baudRate; }
	int getUpLedId() const { return upLedId; }
	int getDownLedId() const { return downLedId; }
	int getMotorId() const { return motorId; }

	// Motor parameters (display names — XML persistence uses its own attribute names)
	ofParameter<bool> motorEnabled { "Enabled", false };
	ofParameter<int> microstep { "Microstep", 16, 1, 256 };
	ofParameter<int> motorSpeed { "Speed (steps/s)", 100, 0, 500 };
	ofParameter<int> motorAcceleration { "Acceleration", 128, 0, 255 };
	ofParameter<float> gearRatio { "Gear Ratio", 15.0f, 0.01f, 1000.0f };
	ofParameter<float> calibrationFactor { "Calibration", 1.0f, 0.01f, 1000.0f };

	// LED parameters
	ofParameter<ofColor> upLedColor { "upLedColor", ofColor::black };
	ofParameter<ofColor> downLedColor { "downLedColor", ofColor::black };
	ofParameter<int> upMainLed { "upMainLed", 0, 0, 255 };
	ofParameter<int> downMainLed { "downMainLed", 0, 0, 255 };
	ofParameter<int> upPwm { "upPwm", 0, 0, 255 };
	ofParameter<int> downPwm { "downPwm", 0, 0, 255 };
	ofParameter<float> individualLuminosity { "individualLuminosity", 1.0f, 0.0f, 1.0f };

	// LED effects base values
	ofParameter<int> upLedBlend { "upLedBlend", 0, 0, 768 };
	ofParameter<int> upLedOrigin { "upLedOrigin", 0, 0, 360 };
	ofParameter<int> upLedArc { "upLedArc", 360, 0, 360 }; // Default to full arc

	ofParameter<int> downLedBlend { "downLedBlend", 0, 0, 768 };
	ofParameter<int> downLedOrigin { "downLedOrigin", 0, 0, 360 };
	ofParameter<int> downLedArc { "downLedArc", 360, 0, 360 }; // Default to full arc

	// OSC update flag
	bool updatingFromOSC;

	// Dirty flags for optimization
	mutable bool ledParametersDirty = true;
	mutable bool motorParametersDirty = true;

	// Methods to mark parameters as dirty
	void markLedParametersDirty() const { ledParametersDirty = true; }
	void markMotorParametersDirty() const { motorParametersDirty = true; }

	// Effects Management
	void updateEffects(float deltaTime);
	void addUpEffect(std::unique_ptr<Effect> effect);
	void addDownEffect(std::unique_ptr<Effect> effect);
	void clearUpEffects();
	void clearDownEffects();

	// New Motor Command Intent Methods
	void commandRelativeMove(int steps, std::optional<int> speed = std::nullopt, std::optional<int> accel = std::nullopt);
	void commandAbsoluteMove(int position, std::optional<int> speed = std::nullopt, std::optional<int> accel = std::nullopt);
	void commandRelativeAngle(float degrees, std::optional<int> speed = std::nullopt, std::optional<int> accel = std::nullopt);
	void commandAbsoluteAngle(float degrees, std::optional<int> speed = std::nullopt, std::optional<int> accel = std::nullopt);

	// New method for minimal view drawing
	void drawMinimal(float x, float y);

private:
	std::string name;
	std::string serialPortName;
	int baudRate;

	// Controllers
	std::unique_ptr<LedMagnetController> upLedMagnet;
	std::unique_ptr<LedMagnetController> downLedMagnet;
	std::unique_ptr<MotorController> motor;
	std::unique_ptr<OSCOutController> oscOutController;

	// Configuration IDs
	int upLedId;
	int downLedId;
	int motorId;

	bool connected;

	EffectsManager upEffectsManager;
	EffectsManager downEffectsManager;

	// Motor Command Intent State
	bool executeRelativeMove = false;
	int targetRelativeSteps = 0;
	std::optional<int> pendingMoveSpeed;
	std::optional<int> pendingMoveAccel;

	bool executeAbsoluteMove = false;
	int targetAbsolutePosition = 0;

	bool executeRelativeAngle = false;
	float targetRelativeDegrees = 0.0f;

	bool executeAbsoluteAngle = false;
	float targetAbsoluteDegrees = 0.0f;

	// Helper methods
	void setupControllers();

	// Last values mirrored to OSC-out, to prevent spam (one struct per side)
	struct LedSideState {
		ofColor color;
		int origin = -1, arc = -1, pwm = -1, mainLed = -1;
		float luminosity = -1.0f;
	};
	LedSideState lastUpSent, lastDownSent;

	// Shared UP/DOWN pipeline: effects -> controller -> change-tracked OSC out
	void applyLedSide(LedMagnetController * controller, EffectsManager & effectsManager,
		const char * oscPosition,
		const ofParameter<ofColor> & colorParam, const ofParameter<int> & mainLedParam,
		const ofParameter<int> & blendParam, const ofParameter<int> & originParam,
		const ofParameter<int> & arcParam, const ofParameter<int> & pwmParam,
		LedSideState & lastSent, float dt);

	// Helper for minimal view
	ofRectangle drawSingleLedControllerMinimal(float x, float y, const std::string & label,
		const ofColor & color, int blend, int origin, int arc,
		float globalLum, float individualLum, bool isUpController);

	// Radii for minimal view drawing
	static constexpr float MINIMAL_CIRCLE_1_RADIUS = 30.0f;
	static constexpr float MINIMAL_CIRCLE_2_RADIUS = 45.0f;
	static constexpr float MINIMAL_CIRCLE_3_RADIUS = 60.0f;
};
