#include "EffectsManager.h"
#include <algorithm> // For std::remove_if

void EffectsManager::addEffect(std::unique_ptr<Effect> effect) {
	if (effect) {
		effects.push_back(std::move(effect));
	}
}

void EffectsManager::removeEffect(Effect * effectToRemove) {
	effects.erase(
		std::remove_if(effects.begin(), effects.end(),
			[&](const std::unique_ptr<Effect> & effect) {
				return effect.get() == effectToRemove;
			}),
		effects.end());
}

void EffectsManager::clearEffects() {
	effects.clear();
}

void EffectsManager::update(float deltaTime) {
	for (auto & effect : effects) {
		if (effect && effect->isEnabled()) {
			effect->update(deltaTime);
		}
	}
}

void EffectsManager::processEffects(EffectParameters & params) {
	// Pass a copy of deltaTime to params, or ensure it's set before calling this
	// params.deltaTime = ...; // Should be set by the caller (e.g., HourGlass)

	for (auto & effect : effects) {
		if (effect && effect->isEnabled()) {
			effect->apply(params);
		}
	}
}