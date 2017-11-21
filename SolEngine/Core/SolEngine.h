#pragma once

#include "WindowSystem.h"
#include "RenderSystem.h"

class SolEngine {
	
public:
	explicit SolEngine() = default;

	SolEngine(const SolEngine&) = delete;
	SolEngine& operator=(const SolEngine&) = delete;

	void init();
	void update();
	void shutdown();

private:
	WindowSystem m_windowSystem;
	RenderSystem m_renderSystem;
};
