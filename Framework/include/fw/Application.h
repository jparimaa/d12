#pragma once

namespace fw
{
class Application
{
public:
    Application(){};
    virtual ~Application(){};
    Application(const Application&) = delete;
    Application(Application&&) = delete;
    Application& operator=(const Application&) = delete;
    Application& operator=(Application&&) = delete;

    virtual bool initialize() = 0;
    virtual void update() = 0;
    virtual void onGUI() = 0;
};

} // namespace fw
