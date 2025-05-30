#pragma once

#include "EffectParameters.h"

class Effect {
public:
	Effect()
		: enabled(true) { }
	virtual ~Effect() = default;

	virtual void apply(EffectParameters & params) = 0;
	virtual void update(float deltaTime) { /* Optional for effects to implement */ }

	void setEnabled(bool enable) { enabled = enable; }
	bool isEnabled() const { return enabled; }

protected:
	bool enabled;
};