#pragma once

#include <algorithm>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace voxel {

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

class Camera {
public:
    Camera();

    [[nodiscard]] glm::mat4 getViewMatrix() const;
    [[nodiscard]] glm::mat4 getProjectionMatrix(float aspect) const;

    [[nodiscard]] glm::vec3 position() const { return m_position; }
    [[nodiscard]] glm::vec3 front() const { return m_front; }
    [[nodiscard]] glm::vec3 up() const { return m_up; }
    [[nodiscard]] glm::vec3 right() const { return m_right; }
    [[nodiscard]] float yaw() const { return m_yaw; }
    [[nodiscard]] float pitch() const { return m_pitch; }

    [[nodiscard]] const AABB& bounds() const { return m_bounds; }
    void setBounds(const AABB& bounds) { m_bounds = bounds; }

    void setPosition(const glm::vec3& pos) { m_position = pos; updateVectors(); }
    void setYaw(float y) { m_yaw = y; updateVectors(); }
    void setPitch(float p) { m_pitch = std::clamp(p, -89.0f, 89.0f); updateVectors(); }

    void setMoveForward(bool v) { m_moveForward = v; }
    void setMoveBackward(bool v) { m_moveBackward = v; }
    void setMoveLeft(bool v) { m_moveLeft = v; }
    void setMoveRight(bool v) { m_moveRight = v; }
    void setJump(bool v) { m_jump = v; }
    void setSprint(bool v) { m_sprint = v; }

    void processMouseMovement(float deltaX, float deltaY);

    bool update(float deltaTime, const std::vector<AABB>* solids = nullptr);

    float moveSpeed = 5.0f;
    float sprintMultiplier = 2.0f;
    float jumpVelocity = 7.5f;
    float gravity = 28.0f;
    float mouseSensitivity = 0.15f;

private:
    void updateVectors();

    glm::vec3 m_position = glm::vec3(0.0f, 80.0f, 0.0f);
    glm::vec3 m_front = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 m_up = glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 m_right = glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 m_worldUp = glm::vec3(0.0f, 1.0f, 0.0f);

    float m_yaw = -90.0f;
    float m_pitch = -15.0f;

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
