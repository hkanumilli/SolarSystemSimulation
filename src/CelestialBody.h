//
// Created by Sriharsha Kanumilli on 1/4/26.
//

#ifndef CELESTIALBODY_H
#define CELESTIALBODY_H

#include <vector>
#include <deque>
#include <string>
#include <glm/glm.hpp>

class CelestialBody {
public:
    CelestialBody(glm::vec3 position, glm::vec3 velocity, float mass, float radius,
                  float r = 1.0f, float g = 1.0f, float b = 1.0f, bool emissive = false);

    void applyAcceleration(glm::vec3 acc, float deltaTime);
    void updateVertices();
    void updateDiskVertices();
    void recordTrail();

    std::string name;

    glm::vec3 position;
    glm::vec3 velocity;
    glm::vec3 acceleration;
    float     mass;
    float     radius;
    float     colorR, colorG, colorB;
    bool      isEmissive;
    bool      isBlackHole       = false;
    float     schwarzschildRadius = 0.0f;  // exaggerated, in AU

    std::vector<float> vertices;
    std::vector<float> diskVertices;
    std::deque<glm::vec3> trail;
    static constexpr int kTrailMax = 500;

private:
    std::vector<float> makeSphereVertices(int stacks, int slices) const;
};

#endif //CELESTIALBODY_H
