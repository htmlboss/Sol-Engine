#pragma once

#include "ISystem.h"

struct GLFWwindow;
class RenderSystem;

class WindowSystem : ISystem {
	friend class RenderSystem;
public:
	explicit WindowSystem() = default;
	~WindowSystem() = default;

	WindowSystem(const WindowSystem&) = delete;
	WindowSystem& operator=(const WindowSystem&) = delete;
	
	void init() override;
	void update(const float delta) override;
	void shutdown() override;

	bool shouldClose() const;

private:
	static GLFWwindow* m_window;

	static auto* getWindowPtr() noexcept { return m_window; }
};
