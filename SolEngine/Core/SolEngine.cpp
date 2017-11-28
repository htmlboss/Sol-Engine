#include "SolEngine.h"

#include "Graphics/Mesh.h"

#include <GLFW/glfw3.h>

/***********************************************************************************/
void SolEngine::init() {

	const auto mesh = Mesh::loadModel("Data/chalet.obj", "Data/chalet.jpg");

	m_renderSystem.addMeshes({ mesh });

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