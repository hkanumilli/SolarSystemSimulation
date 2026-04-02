//
// Physics.cpp
//

#include "Physics.h"
#include <glm/glm.hpp>

Physics::Physics(float gravitationalConstant, float softening)
    : G(gravitationalConstant), softening(softening) {}


void Physics::step(std::vector<CelestialBody>& bodies, float deltaTime) {
    // compute acceleration on each body from all other bodies
    std::vector<glm::vec3> accelerations(bodies.size(), glm::vec3(0.0f));

    for (int i = 0; i < (int)bodies.size(); ++i) {
        for (int j = 0; j < (int)bodies.size(); ++j) {
            if (i == j) continue;

            glm::vec3 r = bodies[j].position - bodies[i].position;
            float dist  = std::sqrt(glm::dot(r, r)) + softening;

            // Paczyński-Wiita pseudo-Newtonian potential for black holes:
            //   F = GM / (r - rs)²  instead of GM / r²
            // Normal bodies use rs = 0, so effDist = dist (standard gravity)
            float rs      = bodies[j].isBlackHole ? bodies[j].schwarzschildRadius : 0.0f;
            float effDist = std::max(dist - rs, softening);

            // a = G * m_j * direction / effDist²
            // direction = r / dist (unit vector toward j)
            accelerations[i] += G * bodies[j].mass * (r / dist) / (effDist * effDist);
        }
    }

    // apply acceleration and integrate position/velocity
    for (int i = 0; i < (int)bodies.size(); ++i) {
        bodies[i].applyAcceleration(accelerations[i], deltaTime);
        bodies[i].updateVertices();
        if (bodies[i].isBlackHole) bodies[i].updateDiskVertices();
    }

    // collision detection — merge overlapping bodies
    for (int i = 0; i < (int)bodies.size(); ++i) {
        for (int j = i + 1; j < (int)bodies.size(); ++j) {
            float dist = glm::length(bodies[j].position - bodies[i].position);
            if (dist < bodies[i].radius + bodies[j].radius) {
                // larger body absorbs smaller — figure out which is which
                int big = (bodies[i].mass >= bodies[j].mass) ? i : j;
                int sml = (big == i) ? j : i;

                float totalMass = bodies[big].mass + bodies[sml].mass;

                // conserve momentum
                bodies[big].velocity = (bodies[big].mass * bodies[big].velocity
                                      + bodies[sml].mass * bodies[sml].velocity) / totalMass;

                // center of mass position
                bodies[big].position = (bodies[big].mass * bodies[big].position
                                      + bodies[sml].mass * bodies[sml].position) / totalMass;

                bodies[big].mass = totalMass;

                // volume conservation: new_r = cbrt(r1³ + r2³)
                float r1 = bodies[big].radius, r2 = bodies[sml].radius;
                bodies[big].radius = std::cbrt(r1*r1*r1 + r2*r2*r2);

                bodies[big].updateVertices();

                bodies.erase(bodies.begin() + sml);
                // sml < big means big's index just shifted down
                if (sml < big) --i;
                --j;
            }
        }
    }
}
