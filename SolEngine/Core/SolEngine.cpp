#include "SolEngine.h"

#include <GLFW/glfw3.h>

/***********************************************************************************/
void SolEngine::init() {
	m_windowSystem.init();
	m_renderSystem.init();
}

/***********************************************************************************/
void SolEngine::update() {

	while (!m_windowSystem.shouldClose()) {

		glfwPollEvents();

		m_renderSystem.update(0.0);
	}

	m_renderSystem.waitDeviceIdle();
}

/***********************************************************************************/
void SolEngine::shutdown() {
	m_renderSystem.shutdown();
	m_windowSystem.shutdown();
}