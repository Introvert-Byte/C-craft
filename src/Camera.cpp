#include "Camera.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace voxel {

Camera::Camera() {
    updateVectors();
}

glm::mat4 Camera::getViewMatrix() const {
    return glm::lookAt(m_position, m_position + m_front, m_up);
}

glm::mat4 Camera::getProjectionMatrix(float aspect) const {
    const float fov = glm::radians(70.0f);
    const float nearPlane = 0.1f;
    const float farPlane = 1024.0f;
    return glm::perspective(fov, aspect, nearPlane, farPlane);
}

void Camera::updateVectors() {
    glm::vec3 front;
    front.x = cos(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
    front.y = sin(glm::radians(m_pitch));
    front.z = sin(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
    m_front = glm::normalize(front);
    m_right = glm::normalize(glm::cross(m_front, m_worldUp));
    m_up = glm::normalize(glm::cross(m_right, m_front));
}

void Camera::setPosition(const glm::vec3& pos) {
    m_position = pos;
    updateVectors();
}

void Camera::processMouseMovement(float deltaX, float deltaY) {
    m_yaw += deltaX * mouseSensitivity;
    m_pitch -= deltaY * mouseSensitivity;
    m_pitch = std::clamp(m_pitch, -89.0f, 89.0f);
    updateVectors();
}

bool Camera::update(float deltaTime, const std::vector<AABB>* solids) {
    if (deltaTime <= 0.0f) return false;

    float speed = moveSpeed * deltaTime;
    if (m_sprint) speed *= sprintMultiplier;

    glm::vec3 displacement(0.0f);
    if (m_moveForward) displacement += m_front * speed;
    if (m_moveBackward) displacement -= m_front * speed;
    if (m_moveRight) displacement += m_right * speed;
    if (m_moveLeft) displacement -= m_right * speed;
    displacement.y = 0.0f;

    glm::vec3 gravityAccel(0.0f, -gravity * deltaTime, 0.0f);
    m_verticalVelocity += gravityAccel.y;

    if (m_jump && m_onGround) {
        m_verticalVelocity = jumpVelocity;
        m_onGround = false;
    }

    bool hasSolids = solids && !solids->empty();
    bool moved = false;

    // Horizontal movement with sliding collision.
    glm::vec3 newPos = m_position + displacement;
    if (hasSolids) {
        // Try full movement.
        AABB box{newPos + m_bounds.min, newPos + m_bounds.max};
        bool hit = false;
        for (const auto& s : *solids) {
            if (box.intersects(s)) { hit = true; break; }
        }
        if (!hit) {
            m_position = newPos;
            moved = true;
        } else {
            // Slide along X only.
            glm::vec3 tryX = m_position + glm::vec3(displacement.x, 0.0f, 0.0f);
            AABB bx{tryX + m_bounds.min, tryX + m_bounds.max};
            bool hx = false;
            for (const auto& s : *solids) {
                if (bx.intersects(s)) { hx = true; break; }
            }
            if (!hx) { m_position = tryX; moved = true; }

            // Slide along Z only.
            glm::vec3 tryZ = m_position + glm::vec3(0.0f, 0.0f, displacement.z);
            AABB bz{tryZ + m_bounds.min, tryZ + m_bounds.max};
            bool hz = false;
            for (const auto& s : *solids) {
                if (bz.intersects(s)) { hz = true; break; }
            }
            if (!hz) { m_position = tryZ; moved = true; }
        }
    } else {
        m_position = newPos;
        moved = true;
    }

    // Vertical movement with collision.
    if (hasSolids) {
        glm::vec3 tryY = m_position + glm::vec3(0.0f, m_verticalVelocity * deltaTime, 0.0f);
        AABB nextY{tryY + m_bounds.min, tryY + m_bounds.max};
        bool hit = false;
        const AABB* hitBlock = nullptr;
        for (const auto& s : *solids) {
            if (nextY.intersects(s)) { hit = true; hitBlock = &s; break; }
        }
        if (!hit) {
            m_position = tryY;
            moved = true;
        } else {
            if (m_verticalVelocity < 0.0f) {
                // Land on top of block.
                m_position.y = std::floor(hitBlock->max.y - m_bounds.min.y) + 0.001f;
            } else {
                // Hit head.
                m_position.y = std::ceil(hitBlock->min.y - m_bounds.max.y) - 0.001f;
            }
            m_verticalVelocity = 0.0f;
            m_onGround = true;
        }
    } else {
        m_position.y += m_verticalVelocity * deltaTime;
        moved = true;
    }

    updateVectors();
    return moved;
}

} // namespace voxel
