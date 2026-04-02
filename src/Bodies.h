//
// Bodies.h
// Initial conditions for all bodies in the simulation.
// Units: AU (distance), solar masses (mass), years (time)
// G = 4π² ≈ 39.478 in these units
//
// Circular orbit velocity: v = sqrt(G * M / r)
// Visual radii are exaggerated so bodies are visible on screen.
//

#ifndef BODIES_H
#define BODIES_H

#include <glm/glm.hpp>

struct BodyDef {
    glm::vec3 position;
    glm::vec3 velocity;
    float     mass;
    float     radius;
    float     colorR, colorG, colorB;
    bool      emissive;
};

constexpr float G = 39.478f;

inline const BodyDef SUN = {
    .position = glm::vec3(0.0f),
    .velocity = glm::vec3(0.0f),
    .mass     = 1.0f,
    .radius   = 0.08f,
    .colorR = 1.0f, .colorG = 0.9f, .colorB = 0.3f,
    .emissive = true
};

// position = (r*cos(angle), 0, r*sin(angle))
// velocity = (-v*sin(angle), 0, v*cos(angle))  — always perpendicular to radius

inline const BodyDef MERCURY = {
    .position = glm::vec3(0.387f, 0.0f, 0.0f),
    .velocity = glm::vec3(0.0f, 0.0f, 10.1f),
    .mass     = 1.65e-7f,
    .radius   = 0.02f,
    .colorR = 0.6f, .colorG = 0.4f, .colorB = 0.2f,
    .emissive = false
};

inline const BodyDef VENUS = {
    .position = glm::vec3(0.0f, 0.0f, 0.723f),          // 90 degrees offset
    .velocity = glm::vec3(-7.39f, 0.0f, 0.0f),
    .mass     = 2.45e-6f,
    .radius   = 0.025f,
    .colorR = 0.9f, .colorG = 0.75f, .colorB = 0.4f,
    .emissive = false
};

inline const BodyDef EARTH = {
    .position = glm::vec3(-1.0f, 0.0f, 0.0f),           // 180 degrees offset
    .velocity = glm::vec3(0.0f, 0.0f, -6.28f),
    .mass     = 3.0e-6f,
    .radius   = 0.025f,
    .colorR = 0.2f, .colorG = 0.5f, .colorB = 1.0f,
    .emissive = false
};

// Moon: positioned relative to Earth (-1, 0, 0), offset along Z
// velocity = Earth's velocity + tangential moon orbital velocity
inline const BodyDef MOON = {
    .position = glm::vec3(-1.0f, 0.0f, 0.00257f),       // Earth pos + 0.00257 AU offset
    .velocity = glm::vec3(-0.215f, 0.0f, -6.28f),        // Earth velocity + moon orbital velocity
    .mass     = 3.7e-8f,
    .radius   = 0.008f,
    .colorR = 0.7f, .colorG = 0.7f, .colorB = 0.7f,
    .emissive = false
};

inline const BodyDef MARS = {
    .position = glm::vec3(0.0f, 0.0f, -1.524f),         // 270 degrees offset
    .velocity = glm::vec3(5.09f, 0.0f, 0.0f),
    .mass     = 3.2e-7f,
    .radius   = 0.022f,
    .colorR = 0.8f, .colorG = 0.3f, .colorB = 0.1f,
    .emissive = false
};

inline const BodyDef JUPITER = {
    .position = glm::vec3(5.2f, 0.0f, 0.0f),
    .velocity = glm::vec3(0.0f, 0.0f, 2.76f),   // sqrt(39.478 * 1.0 / 5.2)
    .mass     = 9.54e-4f,
    .radius   = 0.055f,
    .colorR = 0.8f, .colorG = 0.6f, .colorB = 0.4f,
    .emissive = false
};

#endif //BODIES_H
