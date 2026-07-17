#include "VezerPlayer.h"

// Fire frame-0 keys on the first update after play()/seek: the playhead starts
// half a frame before the region so "(from, to]" catches them.
static constexpr float kPreRoll = 0.5f;

bool VezerPlayer::load(const std::string & absolutePath) {
	ofXml xml;
	if (!xml.load(absolutePath)) {
		ofLogError("VezerPlayer") << "Cannot load XML: " << absolutePath;
		return false;
	}

	auto compsNode = xml.findFirst("//compositions");
	if (!compsNode) {
		ofLogError("VezerPlayer") << "Not a Vezér export (no <compositions>): " << absolutePath;
		return false;
	}

	std::vector<Composition> parsed;
	for (auto & compNode : compsNode.getChildren("composition")) {
		Composition comp;
		comp.enabled = compNode.getChild("state").getValue() != "off";
		comp.name = compNode.getChild("name").getValue();
		comp.fps = std::max(1.0f, compNode.getChild("fps").getFloatValue());
		comp.lengthFrames = compNode.getChild("length").getIntValue();
		comp.loop = compNode.getChild("loop").getValue() == "on";
		comp.startFrame = compNode.getChild("start").getIntValue();
		comp.endFrame = compNode.getChild("end").getIntValue();

		auto tracksNode = compNode.getChild("tracks");
		if (tracksNode) {
			for (auto & trackNode : tracksNode.getChildren("track")) {
				Track track;
				if (!parseTrack(trackNode, track)) continue;

				if (track.type == Track::Type::Audio) {
					comp.audioTrackCount++;
				} else if (!track.enabled) {
					comp.disabledTrackCount++;
				} else if (!track.keys.empty()) {
					comp.playableTrackCount++;
					comp.tracks.push_back(std::move(track));
				}
			}
		}
		parsed.push_back(std::move(comp));
	}

	if (parsed.empty()) {
		ofLogError("VezerPlayer") << "No compositions found in: " << absolutePath;
		return false;
	}

	compositions = std::move(parsed);
	xmlPath = absolutePath;
	playing = false;
	selectComposition(0);

	int totalTracks = 0;
	for (const auto & c : compositions)
		totalTracks += c.playableTrackCount;
	ofLogNotice("VezerPlayer") << "Loaded " << compositions.size() << " compositions, "
							   << totalTracks << " playable OSC tracks from " << ofFilePath::getFileName(absolutePath);
	return true;
}

bool VezerPlayer::parseTrack(const ofXml & trackNode, Track & track) {
	track.enabled = trackNode.getChild("state").getValue() != "off";
	track.name = trackNode.getChild("name").getValue();

	std::string type = trackNode.getChild("type").getValue();
	if (type == "OSCFlag")
		track.type = Track::Type::Flag;
	else if (type == "OSCValue/float")
		track.type = Track::Type::Value;
	else if (type == "OSCColor/floatarray")
		track.type = Track::Type::Color;
	else if (type == "audio")
		track.type = Track::Type::Audio;
	else {
		ofLogWarning("VezerPlayer") << "Skipping track '" << track.name << "' with unsupported type: " << type;
		return false;
	}

	if (track.type == Track::Type::Audio) return true; // counted, never played

	auto targetNode = trackNode.getChild("target");
	if (targetNode) {
		// Vezér files can carry stray whitespace in hand-typed addresses
		track.address = ofTrim(targetNode.getChild("address").getValue());
	}
	if (track.type != Track::Type::Flag && track.address.empty()) {
		ofLogWarning("VezerPlayer") << "Skipping track '" << track.name << "': no target address";
		return false;
	}

	auto keyframesNode = trackNode.getChild("keyframes");
	if (keyframesNode) {
		for (auto & keyNode : keyframesNode.getChildren("keyframe")) {
			Keyframe key;
			key.frame = keyNode.getChild("time").getIntValue();
			key.step = keyNode.getChild("interpolation").getValue() == "none";

			std::string value = keyNode.getChild("value").getValue();
			if (track.type == Track::Type::Flag) {
				value = ofTrim(value);
				if (value.empty() || value[0] != '/') continue; // not an OSC address
				key.flagAddress = value;
			} else if (track.type == Track::Type::Color) {
				auto parts = ofSplitString(value, ",");
				for (size_t i = 0; i < 3 && i < parts.size(); i++) {
					key.v[i] = ofToFloat(parts[i]);
				}
			} else {
				key.v[0] = ofToFloat(value);
			}
			track.keys.push_back(std::move(key));
		}
	}

	std::stable_sort(track.keys.begin(), track.keys.end(),
		[](const Keyframe & a, const Keyframe & b) { return a.frame < b.frame; });
	return true;
}

const VezerPlayer::Composition * VezerPlayer::getCurrent() const {
	if (compositions.empty()) return nullptr;
	return &compositions[compIndex];
}

void VezerPlayer::selectComposition(int index) {
	if (compositions.empty()) return;
	compIndex = ofClamp(index, 0, (int)compositions.size() - 1);
	Composition & comp = compositions[compIndex];
	looping = comp.loop; // XML loop flag is the default; GUI can override
	playFrame = comp.startFrame - kPreRoll;
	resetRuntimeState(comp);
}

void VezerPlayer::resetRuntimeState(Composition & comp) {
	for (auto & track : comp.tracks) {
		track.hasSent = false;
	}
}

void VezerPlayer::play() {
	if (!isLoaded()) return;
	Composition & comp = compositions[compIndex];
	if (playFrame >= comp.playEndFrame()) {
		playFrame = comp.startFrame - kPreRoll; // restart when at the end
		resetRuntimeState(comp);
	}
	playing = true;
}

void VezerPlayer::stop() {
	playing = false;
}

void VezerPlayer::seekNormalized(float t) {
	if (!isLoaded()) return;
	Composition & comp = compositions[compIndex];
	t = ofClamp(t, 0.0f, 1.0f);
	playFrame = comp.startFrame + t * (comp.playEndFrame() - comp.startFrame) - kPreRoll;
	resetRuntimeState(comp); // continuous values re-send at the new position
}

float VezerPlayer::getPositionSeconds() const {
	const Composition * comp = getCurrent();
	if (!comp) return 0.0f;
	return std::max(0.0f, (playFrame - comp->startFrame)) / comp->fps;
}

float VezerPlayer::getPositionNormalized() const {
	const Composition * comp = getCurrent();
	if (!comp) return 0.0f;
	float range = comp->playEndFrame() - comp->startFrame;
	if (range <= 0) return 0.0f;
	return ofClamp((playFrame - comp->startFrame) / range, 0.0f, 1.0f);
}

void VezerPlayer::update(float deltaTime) {
	if (!playing || !isLoaded()) return;

	Composition & comp = compositions[compIndex];
	float endFrame = comp.playEndFrame();
	float newFrame = playFrame + deltaTime * comp.fps;

	if (newFrame < endFrame) {
		step(comp, playFrame, newFrame);
		playFrame = newFrame;
		return;
	}

	// Reached the end of the play region: flush remaining events
	step(comp, playFrame, endFrame);

	if (looping) {
		float wrapped = comp.startFrame + fmodf(newFrame - comp.startFrame, endFrame - comp.startFrame);
		resetRuntimeState(comp);
		step(comp, comp.startFrame - kPreRoll, wrapped);
		playFrame = wrapped;
	} else {
		playFrame = endFrame;
		playing = false;
	}
}

void VezerPlayer::step(Composition & comp, float fromFrame, float toFrame) {
	for (auto & track : comp.tracks) {
		if (track.type == Track::Type::Flag) {
			for (const auto & key : track.keys) {
				if (key.frame > fromFrame && key.frame <= toFrame) {
					ofxOscMessage msg;
					msg.setAddress(key.flagAddress);
					send(msg);
				} else if (key.frame > toFrame) {
					break; // keys are sorted
				}
			}
		} else {
			float value[3];
			evalContinuous(track, toFrame, value);
			int components = (track.type == Track::Type::Color) ? 3 : 1;

			bool changed = !track.hasSent;
			for (int i = 0; i < components && !changed; i++) {
				changed = value[i] != track.lastSent[i];
			}
			if (!changed) continue;

			ofxOscMessage msg;
			msg.setAddress(track.address);
			for (int i = 0; i < components; i++) {
				msg.addFloatArg(value[i]);
				track.lastSent[i] = value[i];
			}
			track.hasSent = true;
			send(msg);
		}
	}
}

void VezerPlayer::evalContinuous(const Track & track, float frame, float * out) const {
	const auto & keys = track.keys;
	// Hold first/last value outside the keyframe range
	if (frame <= keys.front().frame) {
		for (int i = 0; i < 3; i++)
			out[i] = keys.front().v[i];
		return;
	}
	if (frame >= keys.back().frame) {
		for (int i = 0; i < 3; i++)
			out[i] = keys.back().v[i];
		return;
	}

	// Find the segment [k0, k1] containing frame (keys sorted by frame)
	auto it = std::upper_bound(keys.begin(), keys.end(), frame,
		[](float f, const Keyframe & k) { return f < k.frame; });
	const Keyframe & k1 = *it;
	const Keyframe & k0 = *(it - 1);

	if (k0.step || k1.frame == k0.frame) {
		for (int i = 0; i < 3; i++)
			out[i] = k0.v[i];
		return;
	}

	float t = (frame - k0.frame) / float(k1.frame - k0.frame);
	for (int i = 0; i < 3; i++) {
		out[i] = ofLerp(k0.v[i], k1.v[i], t);
	}
}

void VezerPlayer::send(ofxOscMessage & msg) {
	if (messageSink) messageSink(msg);
}
