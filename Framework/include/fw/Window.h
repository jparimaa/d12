#pragma once

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <unordered_map>

namespace fw
{
class Window
{
public:
    Window(){};
    ~Window();
    Window(const Window&) = delete;
    Window(Window&&) = delete;
    Window& operator=(const Window&) = delete;
    Window& operator=(Window&&) = delete;

    bool initialize();
    bool shouldClose() const;
    void update();
    void clearKeyStatus();

    int getWidth() const;
    int getHeight() const;

    GLFWwindow* getWindow();
    HWND getNativeHandle();

    bool isKeyPressed(int key) const;
    bool isKeyDown(int key) const;
    bool isKeyReleased(int key) const;
    float getDeltaX() const;
    float getDeltaY() const;

private:
    int m_width = 1200;
    int m_height = 960;
    float m_aspectRatio = static_cast<float>(m_width) / static_cast<float>(m_height);

    GLFWwindow* m_window = nullptr;

    double m_x = 0.0;
    double m_y = 0.0;
    double m_deltaX = 0.0;
    double m_deltaY = 0.0;
    std::unordered_map<int, bool> m_pressed;
    std::unordered_map<int, bool> m_down;
    std::unordered_map<int, bool> m_released;

    void handleKey(GLFWwindow* window, int key, int scancode, int action, int mods);
};

} // namespace fw
