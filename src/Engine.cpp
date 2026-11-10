#include "Engine.h"

#include <glad/glad.h>

#include <algorithm>
#include <iostream>

namespace voxel {

namespace {

} // namespace

Engine::Engine() = default;
Engine::~Engine() { shutdown(); }

void Engine::framebufferSizeCallback(GLFWwindow* window, int width,
                                     int height) {
    glViewport(0, 0, width, height);
    if (auto* engine = static_cast<Engine*>(glfwGetWindowUserPointer(window))) {
        engine->m_width = width;
        engine->m_height = height;
    }
}

void Engine::cursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
    auto* engine = static_cast<Engine*>(glfwGetWindowUserPointer(window));
    if (!engine) return;
    if (engine->m_firstMouse) {
        engine->m_lastX = static_cast<float>(xpos);
        engine->m_lastY = static_cast<float>(ypos);
        engine->m_firstMouse = false;
    }
    float xoffset = static_cast<float>(xpos) - engine->m_lastX;
    float yoffset = engine->m_lastY - static_cast<float>(ypos);
    engine->m_lastX = static_cast<float>(xpos);
    engine->m_lastY = static_cast<float>(ypos);
    engine->m_camera.processMouseMovement(xoffset, yoffset);
}

void Engine::mouseButtonCallback(GLFWwindow* window, int button, int action,
                                 int mods) {
    (void)mods;
    auto* engine = static_cast<Engine*>(glfwGetWindowUserPointer(window));
    if (!engine) return;
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    }
}

void Engine::scrollCallback(GLFWwindow* window, double /*xoffset*/,
                            double yoffset) {
    (void)window;
    auto* engine = static_cast<Engine*>(glfwGetWindowUserPointer(window));
    if (!engine) return;
    engine->m_camera.moveSpeed = std::clamp(
        engine->m_camera.moveSpeed + static_cast<float>(yoffset) * 0.5f, 1.0f,
        20.0f);
}

bool Engine::init(int width, int height, const char* title) {
    m_width = width;
    m_height = height;

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

    glfwSetFramebufferSizeCallback(m_window, &Engine::framebufferSizeCallback);
    glfwSetCursorPosCallback(m_window, &Engine::cursorPosCallback);
    glfwSetMouseButtonCallback(m_window, &Engine::mouseButtonCallback);
    glfwSetScrollCallback(m_window, &Engine::scrollCallback);

    // Enable depth testing and back-face culling.
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glClearColor(0.45f, 0.72f, 0.95f, 1.0f); // sky colour

    // Shader + texture atlas.
    if (!m_shader.loadFromFiles("shaders/default.vert", "shaders/default.frag")) {
        std::cout << "Using built-in default shaders\n";
    }
    m_atlas.buildProcedural(16, 16);

    // Camera starting bounds (player AABB).
    m_camera.setBounds({glm::vec3(-0.3f, 0.0f, -0.3f),
                        glm::vec3(0.3f, 1.8f, 0.3f)});

    m_lastTime = glfwGetTime();

    // Generate initial chunks around spawn.
    loadChunksAroundPlayer();

    return true;
}

int Engine::run() {
    while (!glfwWindowShouldClose(m_window)) {
        double currentTime = glfwGetTime();
        float deltaTime = static_cast<float>(currentTime - m_lastTime);
        m_lastTime = currentTime;
        if (deltaTime > 0.25f) deltaTime = 0.25f; // spiral-of-death clamp

        processInput(deltaTime);
        m_camera.update(deltaTime);

        updateChunks();
        render();

        glfwSwapBuffers(m_window);
        glfwPollEvents();
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
}

void Engine::processInput(float deltaTime) {
    (void)deltaTime;

    if (glfwGetKey(m_window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(m_window, GLFW_TRUE);
    }

    // WASD movement.
    m_camera.setMoveForward(
        glfwGetKey(m_window, GLFW_KEY_W) == GLFW_PRESS);
    m_camera.setMoveBackward(
        glfwGetKey(m_window, GLFW_KEY_S) == GLFW_PRESS);
    m_camera.setMoveLeft(
        glfwGetKey(m_window, GLFW_KEY_A) == GLFW_PRESS);
    m_camera.setMoveRight(
        glfwGetKey(m_window, GLFW_KEY_D) == GLFW_PRESS);
    m_camera.setJump(glfwGetKey(m_window, GLFW_KEY_SPACE) == GLFW_PRESS);
    m_camera.setSprint(glfwGetKey(m_window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS);

    // Toggle wireframe with F1.
    static bool wireframe = false;
    if (glfwGetKey(m_window, GLFW_KEY_F1) == GLFW_PRESS && !wireframe) {
        wireframe = true;
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    } else if (glfwGetKey(m_window, GLFW_KEY_F1) == GLFW_RELEASE) {
        wireframe = false;
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
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
    loadChunksAroundPlayer();

    // Build meshes for freshly generated chunks on the main thread (GL calls).
    // Generation itself is already offloaded to std::async.
    const int limit = 4; // meshes per frame to spread the cost
    int built = 0;
    glm::vec3 p = m_camera.position();
    int px = static_cast<int>(std::floor(p.x / Chunk::WIDTH));
    int pz = static_cast<int>(std::floor(p.z / Chunk::DEPTH));

    for (int z = -RENDER_DISTANCE; z <= RENDER_DISTANCE && built < limit; ++z) {
        for (int x = -RENDER_DISTANCE; x <= RENDER_DISTANCE && built < limit; ++x) {
            int cx = px + x;
            int cz = pz + z;
            if (m_world.getChunk(cx, cz) && m_world.needsMesh(cx, cz)) {
                m_world.buildChunkMesh(m_atlas, cx, cz);
                ++built;
            }
        }
    }
}

void Engine::render() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    m_shader.use();

    // Projection / view / model.
    float aspect = static_cast<float>(m_width) / static_cast<float>(m_height);
    glm::mat4 projection = m_camera.getProjectionMatrix(aspect);
    glm::mat4 view = m_camera.getViewMatrix();
    glm::mat4 model = glm::mat4(1.0f);

    m_shader.setUniformMatrix4fv("uProjection", projection);
    m_shader.setUniformMatrix4fv("uView", view);
    m_shader.setUniformMatrix4fv("uModel", model);

    // Lighting.
    glm::vec3 lightPos(50.0f, 100.0f, 50.0f);
    m_shader.setUniform3f("uLightPos", lightPos);
    m_shader.setUniformVec3("uLightColor", 1.0f, 1.0f, 0.95f);
    m_shader.setUniform3f("uViewPos", m_camera.position());
    m_shader.setUniformVec3("uBlockColor", 1.0f, 1.0f, 1.0f);

    // Bind the texture atlas once for all chunks.
    m_shader.setUniform1i("uTextureAtlas", 0);
    m_atlas.bind(0);

    // Render all loaded chunks.
    glm::vec3 p = m_camera.position();
    int px = static_cast<int>(std::floor(p.x / Chunk::WIDTH));
    int pz = static_cast<int>(std::floor(p.z / Chunk::DEPTH));

    for (int z = -RENDER_DISTANCE; z <= RENDER_DISTANCE; ++z) {
        for (int x = -RENDER_DISTANCE; x <= RENDER_DISTANCE; ++x) {
            Chunk* chunk = m_world.getChunk(px + x, pz + z);
            if (!chunk || !chunk->hasMesh()) continue;
            // Simple view frustum-ish distance cull: skip far chunks.
            glm::vec3 o = chunk->origin();
            if (glm::length(p - o) >
                static_cast<float>(RENDER_DISTANCE * Chunk::WIDTH + 8))
                continue;
            chunk->render();
        }
    }
}

} // namespace voxel
