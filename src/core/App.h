#pragma once
#include <cstdint>

struct AppConfig {
    int windowWidth = 1280;
    int windowHeight = 720;
    const char* title = "Mini Engine - Day 1";
};

class App {
public:
    bool Init(const AppConfig& cfg);
    void Run();
    void Shutdown();

private:
    bool m_running = false;
};
