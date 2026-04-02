//
// Physics.h
//

#ifndef PHYSICS_H
#define PHYSICS_H

#include <vector>
#include "CelestialBody.h"
#include "Bodies.h"

class Physics {
public:
    Physics(float gravitationalConstant = 39.478f, float softening = 0.001f);

    void step(std::vector<CelestialBody>& bodies, float deltaTime);

private:
    float G;          // gravitational constant
    float softening;  // prevents infinite force at zero distance
};

#endif //PHYSICS_H
