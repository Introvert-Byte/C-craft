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
    const float fov = glm::radians(60.0f);
    const float nearPlane = 0.1f;
    const float farPlane = 4096.0f;
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

void Camera::processMouseMovement(float deltaX, float deltaY, bool constrainPitch) {
    m_yaw += deltaX * mouseSensitivity;
    m_pitch -= deltaY * mouseSensitivity; // inverted Y for natural feel

    if (constrainPitch) {
        m_pitch = std::clamp(m_pitch, -89.0f, 89.0f);
    }
    updateVectors();
}

void Camera::setPitch(float degrees) {
    m_pitch = std::clamp(degrees, -89.0f, 89.0f);
    updateVectors();
}

bool Camera::update(float deltaTime, const AABB* worldBounds) {
    if (deltaTime <= 0.0f) return false;

    float speed = moveSpeed * deltaTime;
    if (m_sprint) speed *= sprintMultiplier;

    glm::vec3 displacement(0.0f);
    if (m_moveForward) displacement += m_front * speed;
    if (m_moveBackward) displacement -= m_front * speed;
    if (m_moveRight) displacement += m_right * speed;
    if (m_moveLeft) displacement -= m_right * speed;

    // Keep horizontal movement on the XZ plane for walking.
    displacement.y = 0.0f;

    glm::vec3 gravityAccel(0.0f, -gravity * deltaTime, 0.0f);
    m_verticalVelocity += gravityAccel.y;

    if (m_jump && m_onGround) {
        m_verticalVelocity = jumpVelocity;
        m_onGround = false;
    }

    glm::vec3 movement = displacement + glm::vec3(0.0f, m_verticalVelocity * deltaTime, 0.0f);

    bool moved = glm::length(movement) > 1e-4f;

    // Apply movement then resolve against provided world AABBs.
    glm::vec3 newPosition = m_position + movement;

    if (worldBounds) {
        // Horizontal movement with sliding resolution.
        glm::vec3 horiz = m_position + displacement;
        if (!worldBounds->intersects(AABB{horiz + m_bounds.min, horiz + m_bounds.max})) {
            m_position = horiz;
            moved = true;
        } else {
            // Try sliding along X then Z.
            glm::vec3 tryX = m_position + glm::vec3(displacement.x, 0.0f, 0.0f);
            if (!worldBounds->intersects(AABB{tryX + m_bounds.min, tryX + m_bounds.max})) {
                m_position = tryX;
            }
            glm::vec3 tryZ = m_position + glm::vec3(0.0f, 0.0f, displacement.z);
            if (!worldBounds->intersects(AABB{tryZ + m_bounds.min, tryZ + m_bounds.max})) {
                m_position = tryZ;
            }
        }

        // Vertical movement.
        glm::vec3 tryY = m_position + glm::vec3(0.0f, m_verticalVelocity * deltaTime, 0.0f);
        AABB nextY{tryY + m_bounds.min, tryY + m_bounds.max};
        if (!worldBounds->intersects(nextY)) {
            m_position = tryY;
        } else {
            // Collided vertically: snap to surface and reset velocity.
            if (m_verticalVelocity < 0.0f) {
                // Falling: sit on top of the block.
                m_position.y = std::floor(worldBounds->max.y - m_bounds.min.y) + 0.001f;
            } else {
                // Rising: hit head, push down.
                m_position.y = std::ceil(worldBounds->min.y - m_bounds.max.y) - 0.001f;
            }
            m_verticalVelocity = 0.0f;
            if (m_verticalVelocity == 0.0f && gravityAccel.y < 0.0f) m_onGround = true;
        }
    } else {
        m_position = newPosition;
    }

    // Optional world clamp.
    if (worldBounds) {
        m_position.x = std::clamp(m_position.x, worldBounds->min.x + m_bounds.min.x,
                                          worldBounds->max.x + m_bounds.max.x);
        m_position.z = std::clamp(m_position.z, worldBounds->min.z + m_bounds.min.z,
                                          worldBounds->max.z + m_bounds.max.z);
    }

    updateVectors();
    return moved;
}

void Camera::resolveCollisions(const std::vector<AABB>& solids) {
    AABB current{m_position + m_bounds.min, m_position + m_bounds.max};
    for (const auto& solid : solids) {
        if (!current.intersects(solid)) continue;
        // Compute overlap on each axis and push out along the smallest axis.
        glm::vec3 overlap;
        overlap.x = (current.max.x > solid.max.x) ? (current.max.x - solid.min.x)
                                                    : (solid.max.x - current.min.x);
        overlap.y = (current.max.y > solid.max.y) ? (current.max.y - solid.min.y)
                                                    : (solid.max.y - current.min.y);
        overlap.z = (current.max.z > solid.max.z) ? (current.max.z - solid.min.z)
                                                    : (solid.max.z - current.min.z);

        glm::vec3 push(0.0f);
        if (overlap.x <= overlap.y && overlap.x <= overlap.z) {
            push.x = (current.center().x < solid.center().x) ? -overlap.x : overlap.x;
        } else if (overlap.y <= overlap.z) {
            push.y = (current.center().y < solid.center().y) ? -overlap.y : overlap.y;
        } else {
            push.z = (current.center().z < solid.center().z) ? -overlap.z : overlap.z;
        }
        m_position += push;
        current.min += push;
        current.max += push;
    }
}

} // namespace voxel
