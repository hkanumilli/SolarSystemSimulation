//
// Camera.h
//

#ifndef CAMERA_H
#define CAMERA_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <GLFW/glfw3.h>
#include <vector>

class CelestialBody;

class Camera {
public:
    Camera(float distance = 3.0f, float yaw = 0.0f, float pitch = 0.4f);

    void onScroll(double yOffset);
    void onMouseButton(int button, int action, double x, double y);
    void onMouseMove(GLFWwindow* window, double x, double y);

    // call each frame to keep target locked onto followed body
    void updateFollow(const std::vector<CelestialBody>& bodies);

    // cast a ray from click position and follow the hit body (-1 = unfollow)
    void trySelect(double mouseX, double mouseY, int windowW, int windowH,
                   const glm::mat4& projection,
                   const std::vector<CelestialBody>& bodies);

    glm::mat4 viewMatrix() const;
    glm::vec3 viewPosition() const;

private:
    float yaw;
    float pitch;
    float distance;
    glm::vec3 target;

    int followIndex = -1;  // index into bodies vector, -1 = free camera

    bool orbiting;
    double orbitLastX;
    double orbitLastY;

    bool panning;
    double panLastX;
    double panLastY;

    glm::vec3 position() const;
};

#endif //CAMERA_H
