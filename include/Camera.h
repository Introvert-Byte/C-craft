#pragma once

#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace voxel {

// Axis-Aligned Bounding Box used for player vs world collision.
struct AABB {
    glm::vec3 min;
    glm::vec3 max;

    [[nodiscard]] glm::vec3 center() const { return (min + max) * 0.5f; }
    [[nodiscard]] glm::vec3 size() const { return max - min; }

    [[nodiscard]] bool intersects(const AABB& other) const {
        return min.x < other.max.x && max.x > other.min.x &&
               min.y < other.max.y && max.y > other.min.y &&
               min.z < other.max.z && max.z > other.min.z;
    }
};

// First-person camera with smooth WASD movement, mouse look (Euler angles)
// and optional AABB collision against the voxel world.
class Camera {
public:
    Camera();

    // View matrix.
    [[nodiscard]] glm::mat4 getViewMatrix() const;
    // Projection matrix (recompute when aspect ratio changes).
    [[nodiscard]] glm::mat4 getProjectionMatrix(float aspect) const;

    [[nodiscard]] glm::vec3 position() const { return m_position; }
    [[nodiscard]] glm::vec3 front() const { return m_front; }
    [[nodiscard]] glm::vec3 up() const { return m_up; }
    [[nodiscard]] glm::vec3 right() const { return m_right; }
    [[nodiscard]] float yaw() const { return m_yaw; }
    [[nodiscard]] float pitch() const { return m_pitch; }

    void setPosition(const glm::vec3& position) { m_position = position; }
    void setPitch(float degrees);

    [[nodiscard]] const AABB& bounds() const { return m_bounds; }
    void setBounds(const AABB& bounds) { m_bounds = bounds; }

    // Movement flags updated from input polling.
    void setMoveForward(bool v) { m_moveForward = v; }
    void setMoveBackward(bool v) { m_moveBackward = v; }
    void setMoveLeft(bool v) { m_moveLeft = v; }
    void setMoveRight(bool v) { m_moveRight = v; }
    void setJump(bool v) { m_jump = v; }
    void setSprint(bool v) { m_sprint = v; }

    // Mouse look. `pitch`/`yaw` are deltas in degrees.
    void processMouseMovement(float deltaX, float deltaY, bool constrainPitch = true);

    // Update camera position / orientation over a time step. Returns true if
    // the camera moved (useful to trigger chunk updates).
    bool update(float deltaTime, const AABB* worldBounds = nullptr);

    // Resolve the camera AABB against a set of solid world AABBs and push the
    // camera out of any overlaps (simple positional resolution).
    void resolveCollisions(const std::vector<AABB>& solids);

    float moveSpeed = 4.0f;
    float sprintMultiplier = 1.8f;
    float jumpVelocity = 6.5f;
    float gravity = 22.0f;
    float mouseSensitivity = 0.12f;

private:
    void updateVectors();

    glm::vec3 m_position = glm::vec3(0.0f, 100.0f, 0.0f);
    glm::vec3 m_front = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 m_up = glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 m_right = glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 m_worldUp = glm::vec3(0.0f, 1.0f, 0.0f);

    float m_yaw = -90.0f;   // looking along -Z initially
    float m_pitch = 0.0f;

    float m_verticalVelocity = 0.0f;
    bool m_onGround = false;

    bool m_moveForward = false;
    bool m_moveBackward = false;
    bool m_moveLeft = false;
    bool m_moveRight = false;
    bool m_jump = false;
    bool m_sprint = false;

    AABB m_bounds = {glm::vec3(-0.3f, 0.0f, -0.3f),
                     glm::vec3(0.3f, 1.8f, 0.3f)};
};

} // namespace voxel
