#pragma once

#include <memory>
#include <string>
#include <vector>

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include "Camera.h"
#include "GameState.h"
#include "Shader.h"
#include "TextureAtlas.h"
#include "UI.h"
#include "World.h"
#include "WorldSave.h"

namespace voxel {

class Engine {
public:
    Engine();
    ~Engine();

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    bool init(int width, int height, const char* title);
    int run();
    void shutdown();

    [[nodiscard]] GLFWwindow* window() const { return m_window; }
    [[nodiscard]] GameState state() const { return m_state; }
    void setState(GameState state);

    [[nodiscard]] bool isCreative() const { return m_creativeMode; }
    void setCreative(bool creative);

private:
    void updateMainMenu(float deltaTime);
    void updateWorldMenu(float deltaTime);
    void updatePlaying(float deltaTime);
    void updatePaused(float deltaTime);

    void renderMainMenu();
    void renderWorldMenu();
    void renderPlaying();
    void renderPaused();
    void renderDebugOverlay();

    void loadChunksAroundPlayer();
    void updateChunks();
    void selectWorld(const std::string& name);
    void createWorld(const std::string& name);
    void handleBlockInteraction();

    GLFWwindow* m_window = nullptr;
    int m_width = 1280;
    int m_height = 720;

    GameState m_state = GameState::MAIN_MENU;
    bool m_creativeMode = false;
    bool m_debugOverlay = false;

    Shader m_shader;
    TextureAtlas m_atlas;
    FontRenderer m_font;
    World m_world;
    Camera m_camera;
    WorldSave m_save;

    double m_lastTime = 0.0;

    std::vector<UIButton> m_menuButtons;
    std::vector<UIButton> m_worldButtons;
    std::vector<UIButton> m_pausedButtons;
    std::string m_newWorldName;
    bool m_newWorldActive = false;
    int m_hoveredButton = -1;
    double m_lastClickTime = 0.0;

    static constexpr int RENDER_DISTANCE = 6;
    static constexpr int MAX_GEN_PER_FRAME = 2;
    static constexpr int MAX_MESH_PER_FRAME = 2;

    bool m_firstMouse = true;
    double m_lastX = 0.0;
    double m_lastY = 0.0;
    bool m_cursorCaptured = false;
    bool m_f1Held = false;
    bool m_escHeld = false;
    bool m_wireframe = false;

    static void framebufferSizeCallback(GLFWwindow* window, int width, int height);
    static void cursorPosCallback(GLFWwindow* window, double xpos, double ypos);
    static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
    static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);
    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
};

} // namespace voxel
