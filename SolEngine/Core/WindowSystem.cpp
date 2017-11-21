#include "WindowSystem.h"
#include "Input.h"

#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>

GLFWwindow* WindowSystem::m_window = nullptr;

/***********************************************************************************/
void WindowSystem::init() {
#define genericInputCallback(functionName)\
	[](GLFWwindow* window, const auto... args) {\
		const auto ptr = static_cast<Input*>(glfwGetWindowUserPointer(window));\
		if (ptr->functionName) { ptr->functionName(args...); }\
	}

	if (!glfwInit()) {
		spdlog::get("console")->error("Failed to start GLFW.");
		return;
	}
	glfwSetErrorCallback([](const auto errorCode, const auto* message) 
	{
		spdlog::get("console")->error(message);
	});

#ifdef _DEBUG
	spdlog::get("console")->info(glfwGetVersionString());
#endif
	glfwWindowHint(GLFW_RESIZABLE, GL_TRUE);
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	m_window = glfwCreateWindow(1280, 720, "Vulkan Window", nullptr, nullptr);
	glfwFocusWindow(m_window);

	glfwSetWindowSizeCallback(m_window, genericInputCallback(Input::GetInstance().windowResized));
	
	// Center window
	const auto mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
	glfwSetWindowPos(m_window, (mode->width / 2) - 1280 / 2, (mode->height / 2) - 720 / 2);
}

/***********************************************************************************/
void WindowSystem::update(const float delta) {
}

/***********************************************************************************/
void WindowSystem::shutdown() {
	glfwDestroyWindow(m_window);
	glfwTerminate();
}

/***********************************************************************************/
bool WindowSystem::shouldClose() const {
	return glfwWindowShouldClose(m_window);
}
