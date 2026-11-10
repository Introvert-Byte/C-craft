#pragma once

#include <memory>

#include <GLFW/glfw3.h>

#include "Camera.h"
#include "Shader.h"
#include "TextureAtlas.h"
#include "World.h"

namespace voxel {

// Engine ties together the window, OpenGL context, shader, camera, world and
// texture atlas. It runs the fixed-timestep game loop and handles input.
class Engine {
public:
    Engine();
    ~Engine();

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    // Initialise GLFW, OpenGL (via GLAD), resources and the starting world.
    bool init(int width, int height, const char* title);

    // Main loop. Returns when the window should close.
    int run();

    void shutdown();

    [[nodiscard]] GLFWwindow* window() const { return m_window; }

    // GLFW callbacks (static so they can be passed to GLFW).
    static void framebufferSizeCallback(GLFWwindow* window, int width, int height);
    static void cursorPosCallback(GLFWwindow* window, double xpos, double ypos);
    static void mouseButtonCallback(GLFWwindow* window, int button, int action,
                                    int mods);
    static void scrollCallback(GLFWwindow* window, double xoffset,
                               double yoffset);

private:
    void processInput(float deltaTime);
    void updateChunks();
    void render();
    void loadChunksAroundPlayer();

    GLFWwindow* m_window = nullptr;
    int m_width = 1280;
    int m_height = 720;

    Shader m_shader;
    TextureAtlas m_atlas;
    World m_world;
    Camera m_camera;

    double m_lastTime = 0.0;

    // Render distance in chunks.
    static constexpr int RENDER_DISTANCE = 6;

    // Mouse state (used by GLFW callbacks).
    bool m_firstMouse = true;
    float m_lastX = 0.0f;
    float m_lastY = 0.0f;
};

} // namespace voxel
