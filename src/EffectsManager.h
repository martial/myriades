#pragma once

#include "Effect.h"
#include "EffectParameters.h"
#include <memory>
#include <vector>

class EffectsManager {
public:
	EffectsManager() = default;

	void addEffect(std::unique_ptr<Effect> effect);
	void removeEffect(Effect * effectToRemove); // Or by ID/name if effects have them
	void clearEffects();

	void update(float deltaTime);
	void processEffects(EffectParameters & params); // For a single set of parameters (e.g., one LED controller)

	const std::vector<std::unique_ptr<Effect>> & getEffects() const { return effects; }

private:
	std::vector<std::unique_ptr<Effect>> effects;
};