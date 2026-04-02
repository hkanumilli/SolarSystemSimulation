//
// Created by Sriharsha Kanumilli on 1/4/26.
//

#include "CelestialBody.h"
#include <cmath>

CelestialBody::CelestialBody(glm::vec3 position, glm::vec3 velocity, float mass, float radius,
                              float r, float g, float b, bool emissive)
    : position(position),
      velocity(velocity),
      acceleration(0.0f),
      mass(mass),
      radius(radius),
      colorR(r), colorG(g), colorB(b),
      isEmissive(emissive) {
    updateVertices();
}

void CelestialBody::applyAcceleration(glm::vec3 acc, float deltaTime) {
    glm::vec3 newAcceleration = acc;
    position += velocity * deltaTime + 0.5f * acceleration * deltaTime * deltaTime;
    velocity += 0.5f * (acceleration + newAcceleration) * deltaTime;
    acceleration = newAcceleration;
}

void CelestialBody::recordTrail() {
    trail.push_back(position);
    if ((int)trail.size() > kTrailMax) trail.pop_front();
}

void CelestialBody::updateVertices() {
    vertices = makeSphereVertices(16, 16);
}

void CelestialBody::updateDiskVertices() {
    diskVertices.clear();

    const int   N_SEGS  = 64;
    const int   N_RINGS = 20;
    const float innerR  = radius * 2.5f;
    const float outerR  = radius * 10.0f;

    // color gradient: t=0 (inner) = white-hot, t=1 (outer) = dark red
    auto diskColor = [](float t, float& r, float& g, float& b) {
        if (t < 0.3f) {
            // white -> yellow-orange
            float s = t / 0.3f;
            r = 1.0f;
            g = 1.0f - 0.5f * s;
            b = 1.0f - s;
        } else {
            // orange -> dark red
            float s = (t - 0.3f) / 0.7f;
            r = 1.0f - 0.7f * s;
            g = 0.5f - 0.5f * s;
            b = 0.0f;
        }
    };

    float cx = position.x, cy = position.y, cz = position.z;

    for (int ring = 0; ring < N_RINGS - 1; ++ring) {
        float t0 = static_cast<float>(ring)     / (N_RINGS - 1);
        float t1 = static_cast<float>(ring + 1) / (N_RINGS - 1);
        float r0 = innerR + t0 * (outerR - innerR);
        float r1 = innerR + t1 * (outerR - innerR);

        float cr0, cg0, cb0, cr1, cg1, cb1;
        diskColor(t0, cr0, cg0, cb0);
        diskColor(t1, cr1, cg1, cb1);

        for (int seg = 0; seg < N_SEGS; ++seg) {
            float a0 = 2.0f * M_PI * seg       / N_SEGS;
            float a1 = 2.0f * M_PI * (seg + 1) / N_SEGS;

            // four corners of the quad
            float ax = cx + r0 * std::cos(a0), az = cz + r0 * std::sin(a0);
            float bx = cx + r1 * std::cos(a0), bz = cz + r1 * std::sin(a0);
            float ex = cx + r0 * std::cos(a1), ez = cz + r0 * std::sin(a1);
            float fx = cx + r1 * std::cos(a1), fz = cz + r1 * std::sin(a1);

            // triangle 1: a, b, e
            diskVertices.insert(diskVertices.end(), {ax, cy, az, cr0, cg0, cb0});
            diskVertices.insert(diskVertices.end(), {bx, cy, bz, cr1, cg1, cb1});
            diskVertices.insert(diskVertices.end(), {ex, cy, ez, cr0, cg0, cb0});

            // triangle 2: e, b, f
            diskVertices.insert(diskVertices.end(), {ex, cy, ez, cr0, cg0, cb0});
            diskVertices.insert(diskVertices.end(), {bx, cy, bz, cr1, cg1, cb1});
            diskVertices.insert(diskVertices.end(), {fx, cy, fz, cr1, cg1, cb1});
        }
    }
}

std::vector<float> CelestialBody::makeSphereVertices(int stacks, int slices) const {
    std::vector<float> verts;

    float x = position.x;
    float y = position.y;
    float z = position.z;

    for (int i = 0; i < stacks; ++i) {
        float phi1 = M_PI * i / stacks;
        float phi2 = M_PI * (i + 1) / stacks;

        for (int j = 0; j < slices; ++j) {
            float theta1 = 2.0f * M_PI * j / slices;
            float theta2 = 2.0f * M_PI * (j + 1) / slices;

            float ax = x + radius * std::sin(phi1) * std::cos(theta1);
            float ay = y + radius * std::cos(phi1);
            float az = z + radius * std::sin(phi1) * std::sin(theta1);

            float bx = x + radius * std::sin(phi1) * std::cos(theta2);
            float by = y + radius * std::cos(phi1);
            float bz = z + radius * std::sin(phi1) * std::sin(theta2);

            float cx = x + radius * std::sin(phi2) * std::cos(theta1);
            float cy = y + radius * std::cos(phi2);
            float cz = z + radius * std::sin(phi2) * std::sin(theta1);

            float dx = x + radius * std::sin(phi2) * std::cos(theta2);
            float dy = y + radius * std::cos(phi2);
            float dz = z + radius * std::sin(phi2) * std::sin(theta2);

            auto pushVertex = [&](float vx, float vy, float vz) {
                verts.push_back(vx); verts.push_back(vy); verts.push_back(vz);
                verts.push_back((vx - x) / radius);
                verts.push_back((vy - y) / radius);
                verts.push_back((vz - z) / radius);
            };

            pushVertex(ax, ay, az);
            pushVertex(cx, cy, cz);
            pushVertex(bx, by, bz);

            pushVertex(bx, by, bz);
            pushVertex(cx, cy, cz);
            pushVertex(dx, dy, dz);
        }
    }

    return verts;
}
