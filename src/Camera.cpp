//
// Camera.cpp
//

#include "Camera.h"
#include "CelestialBody.h"
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

Camera::Camera(float distance, float yaw, float pitch)
    : distance(distance),
      yaw(yaw),
      pitch(pitch),
      target(0.0f),
      followIndex(-1),
      orbiting(false), orbitLastX(0.0), orbitLastY(0.0),
      panning(false),  panLastX(0.0),   panLastY(0.0) {}

glm::vec3 Camera::position() const {
    return target + glm::vec3(
        distance * std::cos(pitch) * std::cos(yaw),
        distance * std::sin(pitch),
        distance * std::cos(pitch) * std::sin(yaw)
    );
}

glm::mat4 Camera::viewMatrix() const {
    return glm::lookAt(position(), target, glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::vec3 Camera::viewPosition() const {
    return position();
}

void Camera::onScroll(double yOffset) {
    distance -= static_cast<float>(yOffset) * 0.3f;
    if (distance < 0.05f) distance = 0.05f;
}

void Camera::onMouseButton(int button, int action, double x, double y) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        orbiting = (action == GLFW_PRESS);
        orbitLastX = x;
        orbitLastY = y;
    }
    if (button == GLFW_MOUSE_BUTTON_MIDDLE) {
        panning = (action == GLFW_PRESS);
        panLastX = x;
        panLastY = y;
    }
}

void Camera::onMouseMove(GLFWwindow* window, double x, double y) {
    orbiting = (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT)   == GLFW_PRESS);
    panning  = (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS);

    if (orbiting) {
        float dx = static_cast<float>(x - orbitLastX) * 0.005f;
        float dy = static_cast<float>(y - orbitLastY) * 0.005f;
        yaw   += dx;
        pitch -= dy;
        if (pitch >  1.5f) pitch =  1.5f;
        if (pitch < -1.5f) pitch = -1.5f;
        orbitLastX = x;
        orbitLastY = y;
    }

    if (panning) {
        float dx = static_cast<float>(x - panLastX) * 0.002f;
        float dy = static_cast<float>(y - panLastY) * 0.002f;

        glm::vec3 forward = glm::normalize(target - position());
        glm::vec3 right   = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
        glm::vec3 up      = glm::cross(right, forward);

        target -= right * dx;
        target += up    * dy;

        panLastX = x;
        panLastY = y;
    }
}

void Camera::updateFollow(const std::vector<CelestialBody>& bodies) {
    if (followIndex >= (int)bodies.size()) followIndex = -1;  // body was deleted
    if (followIndex >= 0) target = bodies[followIndex].position;
}

void Camera::trySelect(double mouseX, double mouseY, int windowW, int windowH,
                       const glm::mat4& projection,
                       const std::vector<CelestialBody>& bodies) {
    // convert mouse to normalized device coordinates (-1 to 1)
    float ndcX =  (2.0f * static_cast<float>(mouseX)) / windowW - 1.0f;
    float ndcY = -(2.0f * static_cast<float>(mouseY)) / windowH + 1.0f;

    // unproject through inverse projection and view to get world-space ray
    glm::mat4 invProj = glm::inverse(projection);
    glm::mat4 invView = glm::inverse(viewMatrix());

    glm::vec4 rayEye = invProj * glm::vec4(ndcX, ndcY, -1.0f, 1.0f);
    rayEye = glm::vec4(rayEye.x, rayEye.y, -1.0f, 0.0f);
    glm::vec3 rayDir = glm::normalize(glm::vec3(invView * rayEye));
    glm::vec3 rayOrigin = position();

    // find closest body the ray hits (use larger click radius for easier selection)
    int   bestIndex = -1;
    float bestDist  = 1e9f;

    for (int i = 0; i < (int)bodies.size(); ++i) {
        glm::vec3 oc = rayOrigin - bodies[i].position;
        float clickRadius = bodies[i].radius * 3.0f;  // forgiving click area

        float a = glm::dot(rayDir, rayDir);
        float b = 2.0f * glm::dot(oc, rayDir);
        float c = glm::dot(oc, oc) - clickRadius * clickRadius;
        float discriminant = b * b - 4 * a * c;

        if (discriminant >= 0.0f) {
            float t = (-b - std::sqrt(discriminant)) / (2.0f * a);
            if (t > 0.0f && t < bestDist) {
                bestDist  = t;
                bestIndex = i;
            }
        }
    }

    if (bestIndex == followIndex) {
        followIndex = -1;  // click same body again to unfollow
    } else {
        followIndex = bestIndex;
    }
}
