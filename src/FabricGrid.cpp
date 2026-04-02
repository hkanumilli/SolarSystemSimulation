//
// FabricGrid.cpp
//

#include "FabricGrid.h"
#include "Bodies.h"
#include <cmath>

FabricGrid::FabricGrid(int resolution, float extent, float depthScale)
    : resolution(resolution), extent(extent), depthScale(depthScale) {
    vertices.resize(resolution * resolution * 3);
    buildIndices();
}

void FabricGrid::update(const std::vector<CelestialBody>& bodies) {
    float step = (2.0f * extent) / (resolution - 1);

    std::vector<float> potential(resolution * resolution, 0.0f);
    for (int row = 0; row < resolution; ++row) {
        for (int col = 0; col < resolution; ++col) {
            float x = -extent + col * step;
            float z = -extent + row * step;
            float p = 0.0f;
            for (const auto& body : bodies) {
                float dx  = x - body.position.x;
                float dz  = z - body.position.z;
                float dist = std::sqrt(dx * dx + dz * dz + 0.005f);

                // black holes: 1/(r - rs) creates a sharp funnel
                float rs      = body.isBlackHole ? body.schwarzschildRadius : 0.0f;
                float effDist = std::max(dist - rs, 0.002f);
                // compress mass range so small planets show visible curvature
                float visualMass = std::pow(body.mass, 0.3f);
                p += -G * visualMass / effDist;
            }
            potential[row * resolution + col] = p;
        }
    }

    float baseline = potential[0];
    for (int row = 0; row < resolution; ++row) {
        for (int col = 0; col < resolution; ++col) {
            float x = -extent + col * step;
            float z = -extent + row * step;
            float y = depthScale * (potential[row * resolution + col] - baseline);

            int idx = (row * resolution + col) * 3;
            vertices[idx + 0] = x;
            vertices[idx + 1] = y;
            vertices[idx + 2] = z;
        }
    }
}

void FabricGrid::buildIndices() {
    // horizontal lines
    for (int row = 0; row < resolution; ++row) {
        for (int col = 0; col < resolution - 1; ++col) {
            indices.push_back(row * resolution + col);
            indices.push_back(row * resolution + col + 1);
        }
    }
    // vertical lines
    for (int col = 0; col < resolution; ++col) {
        for (int row = 0; row < resolution - 1; ++row) {
            indices.push_back(row * resolution + col);
            indices.push_back((row + 1) * resolution + col);
        }
    }
}
