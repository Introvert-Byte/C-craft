#include "Engine.h"

#include <glad/glad.h>

#include <algorithm>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace voxel {

static FILE* g_diagFile = nullptr;

static void diag(const char* fmt, ...) {
    if (!g_diagFile) {
        g_diagFile = std::fopen("/tmp/craft_diag.txt", "w");
    }
    if (g_diagFile) {
        va_list args;
        va_start(args, fmt);
        std::vfprintf(g_diagFile, fmt, args);
        va_end(args);
        std::fflush(g_diagFile);
    }
}

static void glCheckError(const char* label) {
    GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR) {
        std::cerr << "[OpenGL Error] " << label << ": 0x" << std::hex << err
                  << std::dec << "\n";
    }
}

Engine::Engine() : m_save("./saves") {}
Engine::~Engine() { shutdown(); }

void Engine::setState(GameState state) {
    m_state = state;
    m_hoveredButton = -1;
    m_newWorldActive = false;
    m_newWorldName.clear();
}

void Engine::setCreative(bool creative) {
    m_creativeMode = creative;
    if (creative) {
        m_camera.moveSpeed = 10.0f;
        m_camera.sprintMultiplier = 1.0f;
        m_camera.gravity = 0.0f;
    } else {
        m_camera.moveSpeed = 4.5f;
        m_camera.sprintMultiplier = 1.8f;
        m_camera.gravity = 22.0f;
    }
}

bool Engine::init(int width, int height, const char* title) {
    m_width = width;
    m_height = height;
    m_save = WorldSave("./saves");

    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    m_window = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (!m_window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return false;
    }
    glfwMakeContextCurrent(m_window);
    glfwSetWindowUserPointer(m_window, this);

    if (!gladLoadGL((GLADloadfunc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD\n";
        return false;
    }

    std::cout << "OpenGL " << glGetString(GL_VERSION) << "\n";
    diag("[diag] init: OpenGL loaded\n");

    glfwSetFramebufferSizeCallback(m_window, &Engine::framebufferSizeCallback);
    glfwSetCursorPosCallback(m_window, &Engine::cursorPosCallback);
    glfwSetMouseButtonCallback(m_window, &Engine::mouseButtonCallback);
    glfwSetScrollCallback(m_window, &Engine::scrollCallback);
    glfwSetKeyCallback(m_window, &Engine::keyCallback);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glClearColor(0.45f, 0.72f, 0.95f, 1.0f);
    glCheckError("init state");

    if (!m_shader.loadFromFiles("shaders/default.vert", "shaders/default.frag")) {
        std::cout << "Using built-in default shaders\n";
    }
    if (!m_shader.valid()) {
        std::cerr << "Failed to compile/link shader program\n";
        return false;
    }

    m_atlas.buildProcedural(16, 16);
    glCheckError("atlas build");

    if (!m_font.init()) {
        std::cerr << "Failed to initialize font renderer\n";
        return false;
    }

    m_camera.setBounds({glm::vec3(-0.3f, 0.0f, -0.3f),
                        glm::vec3(0.3f, 1.8f, 0.3f)});
    m_camera.setPosition(glm::vec3(0.0f, 90.0f, 0.0f));
    m_camera.setPitch(-25.0f);

    m_lastTime = glfwGetTime();
    diag("[diag] init complete\n");

    return true;
}

void Engine::framebufferSizeCallback(GLFWwindow* window, int width,
                                     int height) {
    glViewport(0, 0, width, height);
    if (auto* engine = static_cast<Engine*>(glfwGetWindowUserPointer(window))) {
        engine->m_width = width;
        engine->m_height = height;
    }
}

void Engine::cursorPosCallback(GLFWwindow* window, double xpos,
                               double ypos) {
    auto* engine = static_cast<Engine*>(glfwGetWindowUserPointer(window));
    if (!engine) return;
    if (engine->m_firstMouse) {
        engine->m_lastX = xpos;
        engine->m_lastY = ypos;
        engine->m_firstMouse = false;
    }
    if (engine->m_cursorCaptured) {
        float xoffset = static_cast<float>(xpos - engine->m_lastX);
        float yoffset = static_cast<float>(engine->m_lastY - ypos);
        engine->m_lastX = xpos;
        engine->m_lastY = ypos;
        engine->m_camera.processMouseMovement(xoffset, yoffset);
    }
}

void Engine::mouseButtonCallback(GLFWwindow* window, int button, int action,
                                 int mods) {
    (void)mods;
    auto* engine = static_cast<Engine*>(glfwGetWindowUserPointer(window));
    if (!engine) return;

    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        if (!engine->m_cursorCaptured &&
            engine->m_state == GameState::PLAYING) {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            engine->m_cursorCaptured = true;
        }
        engine->m_lastClickTime = glfwGetTime();
    }
}

void Engine::scrollCallback(GLFWwindow* window, double /*xoffset*/,
                            double yoffset) {
    (void)window;
    auto* engine = static_cast<Engine*>(glfwGetWindowUserPointer(window));
    if (!engine) return;
    engine->m_camera.moveSpeed = std::clamp(
        engine->m_camera.moveSpeed + static_cast<float>(yoffset) * 0.5f, 1.0f,
        30.0f);
}

void Engine::keyCallback(GLFWwindow* window, int key, int scancode, int action,
                         int mods) {
    (void)scancode;
    (void)mods;
    auto* engine = static_cast<Engine*>(glfwGetWindowUserPointer(window));
    if (!engine) return;

    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        if (engine->m_state == GameState::PLAYING ||
            engine->m_state == GameState::CREATIVE) {
            engine->setState(GameState::PAUSED);
            if (engine->m_cursorCaptured) {
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                engine->m_cursorCaptured = false;
            }
        } else if (engine->m_state == GameState::PAUSED) {
            engine->setState(GameState::PLAYING);
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            engine->m_cursorCaptured = true;
        } else if (engine->m_state == GameState::MAIN_MENU) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
    }

    if (key == GLFW_KEY_F3 && action == GLFW_PRESS) {
    }
}

int Engine::run() {
    diag("[diag] run() started\n");
    auto lastFpsTime = glfwGetTime();
    int frames = 0;

    while (!glfwWindowShouldClose(m_window)) {
        double currentTime = glfwGetTime();
        float deltaTime = static_cast<float>(currentTime - m_lastTime);
        m_lastTime = currentTime;
        if (deltaTime > 0.25f) deltaTime = 0.25f;

        switch (m_state) {
            case GameState::MAIN_MENU:
                updateMainMenu(deltaTime);
                break;
            case GameState::WORLD_MENU:
                updateWorldMenu(deltaTime);
                break;
            case GameState::PLAYING:
            case GameState::CREATIVE:
                updatePlaying(deltaTime);
                break;
            case GameState::PAUSED:
                updatePaused(deltaTime);
                break;
        }

        switch (m_state) {
            case GameState::MAIN_MENU:
                renderMainMenu();
                break;
            case GameState::WORLD_MENU:
                renderWorldMenu();
                break;
            case GameState::PLAYING:
            case GameState::CREATIVE:
                renderPlaying();
                break;
            case GameState::PAUSED:
                renderPaused();
                break;
        }

        glfwSwapBuffers(m_window);
        glfwPollEvents();

        ++frames;
        if (currentTime - lastFpsTime >= 1.0) {
            glm::vec3 p = m_camera.position();
            int chunks = 0;
            int verts = 0;
            int px = static_cast<int>(std::floor(p.x / Chunk::WIDTH));
            int pz = static_cast<int>(std::floor(p.z / Chunk::DEPTH));
            for (int z = -RENDER_DISTANCE; z <= RENDER_DISTANCE; ++z) {
                for (int x = -RENDER_DISTANCE; x <= RENDER_DISTANCE; ++x) {
                    Chunk* c = m_world.getChunk(px + x, pz + z);
                    if (c && c->hasMesh()) {
                        ++chunks;
                        verts += static_cast<int>(c->vertexCount());
                    }
                }
            }
            diag("[diag] fps=%d state=%d chunks=%d verts=%d pos=(%.1f,%.1f,%.1f)\n",
                 frames, (int)m_state, chunks, verts, p.x, p.y, p.z);
            frames = 0;
            lastFpsTime = currentTime;
        }
    }

    shutdown();
    return 0;
}

void Engine::shutdown() {
    if (m_window) {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
    }
    glfwTerminate();
    if (g_diagFile) {
        std::fclose(g_diagFile);
        g_diagFile = nullptr;
    }
}

void Engine::updateMainMenu(float deltaTime) {
    // Auto-start into the default world after 3 seconds.
    m_menuAutoStartTimer += deltaTime;
    if (m_menuAutoStartTimer > 3.0f) {
        selectWorld("Default");
    }
}

void Engine::renderMainMenu() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glCheckError("menu clear");

    m_font.begin(m_width, m_height);

    m_font.drawText(m_width * 0.5f - 120.0f, m_height * 0.25f,
                    "C+CRAFT", 2.5f, glm::vec3(0.4f, 0.8f, 1.0f));
    m_font.drawText(m_width * 0.5f - 150.0f, m_height * 0.25f + 40.0f,
                    "Voxel Engine Foundation (C++20)", 1.0f,
                    glm::vec3(0.8f, 0.8f, 0.8f));

    float bw = 220.0f, bh = 40.0f;
    float bx = m_width * 0.5f - bw * 0.5f;
    float by = m_height * 0.45f;

    m_menuButtons.clear();
    m_menuButtons.emplace_back("Singleplayer", bx, by, bw, bh);
    m_menuButtons.emplace_back("Creative Mode", bx, by + bh + 12.0f, bw, bh);
    m_menuButtons.emplace_back("Worlds", bx, by + 2 * (bh + 12.0f), bw, bh);
    m_menuButtons.emplace_back("Options", bx, by + 3 * (bh + 12.0f), bw, bh);
    m_menuButtons.emplace_back("Quit", bx, by + 4 * (bh + 12.0f), bw, bh);

    double clickTime = glfwGetTime();
    bool justClicked = (clickTime - m_lastClickTime < 0.15f);

    for (size_t i = 0; i < m_menuButtons.size(); ++i) {
        auto& b = m_menuButtons[i];
        if (!m_cursorCaptured) {
            double mx, my;
            glfwGetCursorPos(m_window, &mx, &my);
            float uiY = (float)(m_height - my);
            b.hovered = b.contains((float)mx, uiY);
        }
        glm::vec3 col = b.hovered ? glm::vec3(0.6f, 0.9f, 1.0f)
                                  : glm::vec3(0.9f, 0.9f, 0.9f);
        m_font.drawText(b.x + 10.0f, b.y + 8.0f, b.label, 1.2f, col);

        if (b.hovered && justClicked) {
            b.clicked = true;
        }
    }

    if (justClicked) {
        if (m_menuButtons[0].clicked) {
            selectWorld("Default");
        } else if (m_menuButtons[1].clicked) {
            setCreative(true);
            selectWorld("Creative");
        } else if (m_menuButtons[2].clicked) {
            setState(GameState::WORLD_MENU);
        } else if (m_menuButtons[3].clicked) {
        } else if (m_menuButtons[4].clicked) {
            glfwSetWindowShouldClose(m_window, GLFW_TRUE);
        }
    }

    m_font.end();
    glCheckError("menu render");
}

void Engine::updateWorldMenu(float /*deltaTime*/) {}

void Engine::renderWorldMenu() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glCheckError("worldmenu clear");

    m_font.begin(m_width, m_height);

    m_font.drawText(m_width * 0.5f - 100.0f, m_height * 0.15f,
                    "SELECT WORLD", 1.8f, glm::vec3(0.4f, 0.8f, 1.0f));

    auto worlds = m_save.listWorlds();
    float wy = m_height * 0.30f;
    m_worldButtons.clear();

    for (const auto& info : worlds) {
        float bw = 300.0f, bh = 34.0f;
        float bx = m_width * 0.5f - bw * 0.5f;
        m_worldButtons.emplace_back(info.name, bx, wy, bw, bh);
        wy += bh + 10.0f;
    }

    float btnY = wy + 20.0f;
    m_worldButtons.emplace_back("Create New", m_width * 0.5f - 160.0f, btnY,
                                140.0f, 34.0f);
    m_worldButtons.emplace_back("Back", m_width * 0.5f + 20.0f, btnY, 100.0f,
                                34.0f);

    double clickTime = glfwGetTime();
    bool justClicked = (clickTime - m_lastClickTime < 0.15f);

    if (!m_cursorCaptured) {
        double mx, my;
        glfwGetCursorPos(m_window, &mx, &my);
        float uiY = (float)(m_height - my);
        for (auto& b : m_worldButtons) {
            b.hovered = b.contains((float)mx, uiY);
        }
    }

    for (auto& b : m_worldButtons) {
        glm::vec3 col = b.hovered ? glm::vec3(0.6f, 0.9f, 1.0f)
                                  : glm::vec3(0.9f, 0.9f, 0.9f);
        m_font.drawText(b.x + 10.0f, b.y + 8.0f, b.label, 1.1f, col);

        if (b.hovered && justClicked) {
            b.clicked = true;
        }
    }

    if (m_newWorldActive) {
        m_font.drawText(m_width * 0.5f - 120.0f, btnY + 50.0f,
                        "Type world name: " + m_newWorldName + "_", 1.2f,
                        glm::vec3(1.0f, 1.0f, 0.0f));
    }

    if (justClicked) {
        size_t idx = 0;
        for (auto& b : m_worldButtons) {
            if (b.hovered && b.clicked) {
                if (idx < worlds.size()) {
                    selectWorld(worlds[idx].name);
                } else if (b.label == "Create New") {
                    m_newWorldActive = true;
                } else if (b.label == "Back") {
                    setState(GameState::MAIN_MENU);
                }
            }
            ++idx;
        }
    }

    m_font.end();
    glCheckError("worldmenu render");
}

void Engine::selectWorld(const std::string& name) {
    m_save.loadWorld(name, m_world);
    m_camera.setPosition(glm::vec3(0.0f, 90.0f, 0.0f));
    m_camera.setPitch(-25.0f);
    m_cursorCaptured = false;
    glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    loadChunksAroundPlayer();
    setState(m_creativeMode ? GameState::CREATIVE : GameState::PLAYING);
}

void Engine::createWorld(const std::string& name) {
    if (!m_save.createWorld(name)) return;
    selectWorld(name);
}

void Engine::updatePlaying(float deltaTime) {
    std::vector<AABB> solids;
    if (!m_creativeMode) {
        m_world.collectSolidAABBs(solids);
    }
    m_camera.update(deltaTime, m_creativeMode ? nullptr : &solids);

    loadChunksAroundPlayer();
    updateChunks();

    handleBlockInteraction();
}

void Engine::updatePaused(float /*deltaTime*/) {}

void Engine::renderPlaying() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glCheckError("playing clear");

    m_shader.use();

    float aspect = static_cast<float>(m_width) / static_cast<float>(m_height);
    glm::mat4 projection = m_camera.getProjectionMatrix(aspect);
    glm::mat4 view = m_camera.getViewMatrix();
    glm::mat4 model = glm::mat4(1.0f);

    m_shader.setUniformMatrix4fv("uProjection", projection);
    m_shader.setUniformMatrix4fv("uView", view);
    m_shader.setUniformMatrix4fv("uModel", model);

    glm::vec3 lightPos(50.0f, 100.0f, 50.0f);
    m_shader.setUniform3f("uLightPos", lightPos);
    m_shader.setUniformVec3("uLightColor", 1.0f, 1.0f, 0.95f);
    m_shader.setUniform3f("uViewPos", m_camera.position());
    m_shader.setUniformVec3("uBlockColor", 1.0f, 1.0f, 1.0f);

    m_shader.setUniform1i("uTextureAtlas", 0);
    m_atlas.bind(0);

    glm::vec3 p = m_camera.position();
    int px = static_cast<int>(std::floor(p.x / Chunk::WIDTH));
    int pz = static_cast<int>(std::floor(p.z / Chunk::DEPTH));
    float cullRadius = static_cast<float>(RENDER_DISTANCE * Chunk::WIDTH * 2);

    for (int z = -RENDER_DISTANCE; z <= RENDER_DISTANCE; ++z) {
        for (int x = -RENDER_DISTANCE; x <= RENDER_DISTANCE; ++x) {
            Chunk* chunk = m_world.getChunk(px + x, pz + z);
            if (!chunk || !chunk->hasMesh()) continue;
            glm::vec3 o = chunk->origin();
            if (glm::length(p - o) > cullRadius) continue;
            chunk->render();
        }
    }
    glCheckError("playing render");

    m_font.begin(m_width, m_height);
    glm::vec3 pos = m_camera.position();
    char buf[128];
    std::snprintf(buf, sizeof(buf), "XYZ: %.1f, %.1f, %.1f", pos.x, pos.y,
                  pos.z);
    m_font.drawText(10.0f, 10.0f, buf, 1.0f, glm::vec3(1.0f, 1.0f, 1.0f));
    std::snprintf(buf, sizeof(buf), "%s",
                  m_creativeMode ? "CREATIVE MODE" : "SURVIVAL MODE");
    m_font.drawText(10.0f, 30.0f, buf, 1.0f,
                    m_creativeMode ? glm::vec3(0.0f, 1.0f, 0.0f)
                                   : glm::vec3(1.0f, 1.0f, 1.0f));
    m_font.end();
}

void Engine::renderPaused() {
    renderPlaying();

    m_font.begin(m_width, m_height);
    m_font.drawText(m_width * 0.5f - 80.0f, m_height * 0.45f, "PAUSED", 2.0f,
                    glm::vec3(1.0f, 1.0f, 1.0f));
    m_font.end();
}

void Engine::loadChunksAroundPlayer() {
    glm::vec3 p = m_camera.position();
    int px = static_cast<int>(std::floor(p.x / Chunk::WIDTH));
    int pz = static_cast<int>(std::floor(p.z / Chunk::DEPTH));

    for (int z = -RENDER_DISTANCE; z <= RENDER_DISTANCE; ++z) {
        for (int x = -RENDER_DISTANCE; x <= RENDER_DISTANCE; ++x) {
            m_world.generateChunkAsync(px + x, pz + z);
        }
    }
}

void Engine::updateChunks() {
    const int limit = 6;
    int built = 0;
    glm::vec3 p = m_camera.position();
    int px = static_cast<int>(std::floor(p.x / Chunk::WIDTH));
    int pz = static_cast<int>(std::floor(p.z / Chunk::DEPTH));

    for (int z = -RENDER_DISTANCE; z <= RENDER_DISTANCE && built < limit;
         ++z) {
        for (int x = -RENDER_DISTANCE; x <= RENDER_DISTANCE && built < limit;
             ++x) {
            int cx = px + x;
            int cz = pz + z;
            if (m_world.getChunk(cx, cz) && m_world.needsMesh(cx, cz)) {
                m_world.buildChunkMesh(m_atlas, cx, cz);
                ++built;
            }
        }
    }
}

void Engine::handleBlockInteraction() {
    if (!m_cursorCaptured) return;

    glm::vec3 camPos = m_camera.position();
    glm::vec3 camFront = m_camera.front();

    constexpr float MAX_DIST = 6.0f;
    constexpr float STEP = 0.2f;
    glm::vec3 hitBlock(0.0f);
    bool found = false;

    for (float d = 0.0f; d < MAX_DIST; d += STEP) {
        glm::vec3 point = camPos + camFront * d;
        int bx = (int)std::floor(point.x);
        int by = (int)std::floor(point.y);
        int bz = (int)std::floor(point.z);
        BlockID id = m_world.getBlock(bx, by, bz);
        if (id != BLOCK_AIR) {
            hitBlock = glm::vec3((float)bx, (float)by, (float)bz);
            found = true;
            break;
        }
    }

    if (glfwGetMouseButton(m_window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS &&
        found && glfwGetTime() - m_lastClickTime > 0.15) {
        m_world.setBlock((int)hitBlock.x, (int)hitBlock.y, (int)hitBlock.z,
                         BLOCK_AIR);
        int cx = (int)hitBlock.x / Chunk::WIDTH;
        int cz = (int)hitBlock.z / Chunk::DEPTH;
        m_world.markMeshDirty(cx, cz);
        m_lastClickTime = glfwGetTime();
    }

    if (glfwGetMouseButton(m_window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS &&
        found && m_creativeMode &&
        glfwGetTime() - m_lastClickTime > 0.15) {
        m_world.setBlock((int)hitBlock.x, (int)hitBlock.y + 1,
                         (int)hitBlock.z, BLOCK_GRASS);
        int cx = (int)hitBlock.x / Chunk::WIDTH;
        int cz = (int)hitBlock.z / Chunk::DEPTH;
        m_world.markMeshDirty(cx, cz);
        m_lastClickTime = glfwGetTime();
    }
}

} // namespace voxel
