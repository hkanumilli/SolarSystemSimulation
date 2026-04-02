//
// FabricGrid.h
// Spacetime fabric — a grid mesh that dips under gravitational potential.
// Units match simulation: AU, solar masses, years.
//

#ifndef FABRICGRID_H
#define FABRICGRID_H

#include <vector>
#include <glm/glm.hpp>
#include "CelestialBody.h"

class FabricGrid {
public:
    FabricGrid(int resolution = 80, float extent = 4.0f, float depthScale = 0.3f);

    void update(const std::vector<CelestialBody>& bodies);

    std::vector<float>    vertices;  // x, y, z per vertex
    std::vector<unsigned int> indices;   // line indices for GL_LINES

private:
    int   resolution;   // grid lines per side
    float extent;       // half-width in AU (grid goes -extent to +extent)
    float depthScale;   // how much to exaggerate the dip visually

    void buildIndices();
};

#endif //FABRICGRID_H
