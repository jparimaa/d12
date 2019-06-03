#include "Window.h"

namespace
{
bool isKeyTrueInMap(const std::unordered_map<int, bool>& container, int key)
{
    auto it = container.find(key);
    return it != container.end() && it->second;
}
} // namespace

namespace fw
{
Window::~Window()
{
    glfwDestroyWindow(m_window);
    glfwTerminate();
}

bool Window::initialize()
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    m_window = glfwCreateWindow(m_width, m_height, "DX12", nullptr, nullptr);
    glfwSetWindowPos(m_window, 1200, 200);

    auto keyCallback = [](GLFWwindow* win, int key, int scancode, int action, int mods) {
        static_cast<Window*>(glfwGetWindowUserPointer(win))->handleKey(win, key, scancode, action, mods);
    };

    glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetWindowUserPointer(m_window, this);
    glfwSetKeyCallback(m_window, keyCallback);
    glfwGetCursorPos(m_window, &m_x, &m_y);

    return true;
}

bool Window::shouldClose() const
{
    return glfwWindowShouldClose(m_window);
}

void Window::update()
{
    glfwPollEvents();
    double newX, newY;
    glfwGetCursorPos(m_window, &newX, &newY);
    m_deltaX = m_x - newX;
    m_deltaY = m_y - newY;
    m_x = newX;
    m_y = newY;
}

void Window::clearKeyStatus()
{
    m_pressed.clear();
    m_released.clear();
}

int Window::getWidth() const
{
    return m_width;
}

int Window::getHeight() const
{
    return m_height;
}

GLFWwindow* Window::getWindow()
{
    return m_window;
}

HWND Window::getNativeHandle()
{
    return glfwGetWin32Window(m_window);
}

bool Window::isKeyPressed(int key) const
{
    return isKeyTrueInMap(m_pressed, key);
}

bool Window::isKeyDown(int key) const
{
    return isKeyTrueInMap(m_down, key);
}

bool Window::isKeyReleased(int key) const
{
    return isKeyTrueInMap(m_released, key);
}

float Window::getDeltaX() const
{
    return static_cast<float>(m_deltaX);
}

float Window::getDeltaY() const
{
    return static_cast<float>(m_deltaY);
}

void Window::handleKey(GLFWwindow* /*window*/, int key, int /*scancode*/, int action, int /*mods*/)
{
    if (action == GLFW_PRESS)
    {
        m_pressed[key] = true;
        m_down[key] = true;
    }
    if (action == GLFW_RELEASE)
    {
        m_released[key] = true;
        m_down[key] = false;
    }
    if (action == GLFW_REPEAT)
    {
        m_down[key] = true;
    }
}
} // namespace fw
