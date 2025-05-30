#pragma once

#include "Effect.h"
#include "ofMain.h" // For ofMap, cos, TWO_PI if needed, or use std::cos

class ArcCosineEffect : public Effect {
public:
	ArcCosineEffect(float minArc = 90.0f, float maxArc = 360.0f, float periodSeconds = 5.0f);

	void apply(EffectParameters & params) override;
	void update(float deltaTime) override;

	void setMinArc(float minArc) { this->minArc = minArc; }
	float getMinArc() const { return minArc; }

	void setMaxArc(float maxArc) { this->maxArc = maxArc; }
	float getMaxArc() const { return maxArc; }

	void setPeriod(float periodSeconds) { this->periodSeconds = periodSeconds; }
	float getPeriod() const { return periodSeconds; }

private:
	float minArc;
	float maxArc;
	float periodSeconds;
	float elapsedTime;
};