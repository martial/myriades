#pragma once

#include "ofMain.h"
#include "ofxOsc.h"
#include <functional>
#include <string>
#include <vector>

// Plays OSC sequences exported from Vezér as XML.
//
// The export contains multiple <composition>s (scenes), each with its own fps
// and a start/end play region in frames. Tracks are:
//   OSCFlag             - keyframe value is a complete OSC address (args in the
//                         path), fired once when the playhead crosses it
//   OSCValue/float      - numeric value interpolated between keyframes, sent to
//                         the track's target address (1 float arg)
//   OSCColor/floatarray - "r,g,b" interpolated per component (3 float args)
//   audio               - file reference, ignored
// Tracks or compositions with <state>off</state> are parsed but never played.
//
// Messages are delivered through the sink callback (wired to
// OSCController::processMessage) so playback drives the exact same pipeline as
// network OSC.
class VezerPlayer {
public:
	struct Keyframe {
		int frame = 0;
		float v[3] = { 0, 0, 0 }; // value (v[0]) or color (r,g,b)
		std::string flagAddress; // OSCFlag only
		bool step = false; // interpolation "none": hold value until next key
	};

	struct Track {
		enum class Type { Flag,
			Value,
			Color,
			Audio,
			Unknown };
		Type type = Type::Unknown;
		std::string name;
		std::string address; // target address (Value/Color)
		bool enabled = true;
		std::vector<Keyframe> keys; // sorted by frame

		// Runtime dedup so continuous tracks only send on change
		float lastSent[3] = { 0, 0, 0 };
		bool hasSent = false;
	};

	struct Composition {
		std::string name;
		float fps = 60.0f;
		int startFrame = 0;
		int endFrame = 0; // end of play region; falls back to lengthFrames
		int lengthFrames = 0;
		bool loop = false;
		bool enabled = true;
		std::vector<Track> tracks;
		int playableTrackCount = 0;
		int disabledTrackCount = 0;
		int audioTrackCount = 0;

		int playEndFrame() const { return endFrame > startFrame ? endFrame : lengthFrames; }
		float durationSeconds() const { return (playEndFrame() - startFrame) / fps; }
	};

	// Loading
	bool load(const std::string & absolutePath);
	bool isLoaded() const { return !compositions.empty(); }
	const std::string & getPath() const { return xmlPath; }

	// Compositions
	const std::vector<Composition> & getCompositions() const { return compositions; }
	int getCompositionCount() const { return (int)compositions.size(); }
	void selectComposition(int index); // clamps; resets playhead to region start
	int getCompositionIndex() const { return compIndex; }
	const Composition * getCurrent() const;

	// Transport
	void play();
	void stop();
	bool isPlaying() const { return playing; }
	void setLoop(bool enabled) { looping = enabled; }
	bool getLoop() const { return looping; }
	void seekNormalized(float t); // 0..1 within the play region
	float getPositionSeconds() const;
	float getPositionNormalized() const;

	// Message delivery
	void setMessageSink(std::function<void(ofxOscMessage &)> sink) { messageSink = sink; }

	// Advance playback; call every frame
	void update(float deltaTime);

private:
	std::vector<Composition> compositions;
	std::string xmlPath;
	int compIndex = 0;
	bool playing = false;
	bool looping = false;
	float playFrame = 0.0f; // fractional frame position within current composition

	std::function<void(ofxOscMessage &)> messageSink;

	bool parseTrack(const ofXml & trackNode, Track & track);
	void resetRuntimeState(Composition & comp);
	// Fire flags in (fromFrame, toFrame] and send continuous values at toFrame
	void step(Composition & comp, float fromFrame, float toFrame);
	void evalContinuous(const Track & track, float frame, float * out) const;
	void send(ofxOscMessage & msg);
};
