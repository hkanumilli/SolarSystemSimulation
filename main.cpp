#define GL_SILENCE_DEPRECATION

#include <iostream>
#include <random>
#include <cmath>

#include <OpenGL/gl3.h>
#include <GLFW/glfw3.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>

#include "src/CelestialBody.h"
#include "src/Camera.h"
#include "src/Physics.h"
#include "src/Bodies.h"
#include "src/FabricGrid.h"


static GLuint compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cerr << "Shader compile error:\n" << infoLog << "\n";
    }

    return shader;
}

static GLuint createShaderProgram(const char* vertexSrc, const char* fragmentSrc) {
    GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexSrc);
    GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSrc);

    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    GLint success = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &success);

    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(program, 512, nullptr, infoLog);
        std::cerr << "Program link error:\n" << infoLog << "\n";
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return program;
}

static Camera camera(8.0f, 0.0f, 0.25f);  // far enough to see Jupiter at 5.2 AU
static std::vector<CelestialBody>* gBodies     = nullptr;
static glm::mat4*                  gProjection = nullptr;

// placement mode state
static bool  gPlacing           = false;
static float gPlaceMass         = 1.0f;
static float gPlaceRadius       = 0.08f;
static float gPlaceColorR       = 1.0f, gPlaceColorG = 0.5f, gPlaceColorB = 0.2f;
static bool  gPlaceEmissive     = true;
static bool  gPaused            = false;
static bool  gPlaceIsBlackHole  = false;
static float gPlaceSchwarzschild = 0.0f;
static bool  gPlaceBinary       = false;
static float gBinarySep         = 0.5f;  // AU between the two stars
static bool  gPlaceStationary   = false; // spawn with zero velocity

static void scrollCallback(GLFWwindow*, double, double yOffset) {
    camera.onScroll(yOffset);
}

static void mouseButtonCallback(GLFWwindow* window, int button, int action, int) {
    // let ImGui consume clicks on its own panels first
    if (ImGui::GetIO().WantCaptureMouse) return;

    double x, y;
    glfwGetCursorPos(window, &x, &y);

    // placement mode: left-click drops a body on the orbital plane (y = 0)
    if (gPlacing && button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS
            && gBodies && gProjection) {
        int w, h;
        glfwGetWindowSize(window, &w, &h);

        // unproject click to world-space ray (same as trySelect)
        float ndcX =  (2.0f * static_cast<float>(x)) / w - 1.0f;
        float ndcY = -(2.0f * static_cast<float>(y)) / h + 1.0f;
        glm::mat4 invProj = glm::inverse(*gProjection);
        glm::mat4 invView = glm::inverse(camera.viewMatrix());
        glm::vec4 rayEye  = invProj * glm::vec4(ndcX, ndcY, -1.0f, 1.0f);
        rayEye = glm::vec4(rayEye.x, rayEye.y, -1.0f, 0.0f);
        glm::vec3 rayDir    = glm::normalize(glm::vec3(invView * rayEye));
        glm::vec3 rayOrigin = camera.viewPosition();

        // ray-plane intersection with y = 0
        if (std::abs(rayDir.y) > 1e-4f) {
            float t = -rayOrigin.y / rayDir.y;
            if (t > 0.0f) {
                glm::vec3 pos = rayOrigin + t * rayDir;

                // find dominant attractor: body with highest G*m/dist² at placement point
                int   domIdx   = -1;
                float maxForce = 0.0f;
                for (int k = 0; k < (int)gBodies->size(); ++k) {
                    glm::vec3 dr = (*gBodies)[k].position - pos;
                    float dist   = glm::length(dr) + 0.001f;
                    float force  = G * (*gBodies)[k].mass / (dist * dist);
                    if (force > maxForce) { maxForce = force; domIdx = k; }
                }

                // shared spawn helper
                auto spawnBody = [&](glm::vec3 p, glm::vec3 v) {
                    gBodies->emplace_back(p, v, gPlaceMass, gPlaceRadius,
                                          gPlaceColorR, gPlaceColorG, gPlaceColorB,
                                          gPlaceEmissive);
                    auto& b = gBodies->back();
                    b.isBlackHole         = gPlaceIsBlackHole;
                    b.schwarzschildRadius = gPlaceSchwarzschild;
                    if (b.isBlackHole) b.updateDiskVertices();
                };

                if (domIdx < 0 || gPlaceStationary) {
                    // sandbox (no attractor) or stationary — orbit around each other only
                    if (gPlaceBinary) {
                        float half     = gBinarySep * 0.5f;
                        float binSpeed = std::sqrt(G * gPlaceMass / (2.0f * gBinarySep));
                        // lay them out along X, orbit along Z
                        spawnBody(pos + glm::vec3( half, 0, 0), glm::vec3(0, 0, -binSpeed));
                        spawnBody(pos + glm::vec3(-half, 0, 0), glm::vec3(0, 0,  binSpeed));
                    } else {
                        spawnBody(pos, glm::vec3(0.0f));
                    }
                } else {
                    // circular orbit around dominant attractor
                    const auto& dom = (*gBodies)[domIdx];
                    glm::vec3 toDOM = dom.position - pos;
                    float r = std::max(glm::length(toDOM), 0.01f);
                    glm::vec3 dir  = toDOM / r;
                    glm::vec3 perp = glm::normalize(glm::cross(glm::vec3(0,1,0), dir));

                    if (gPlaceBinary) {
                        float half     = gBinarySep * 0.5f;
                        float binSpeed = std::sqrt(G * gPlaceMass / (2.0f * gBinarySep));
                        glm::vec3 baseVel = dom.velocity + perp * std::sqrt(G * dom.mass / r);
                        spawnBody(pos + perp * half, baseVel + (-dir) * binSpeed);
                        spawnBody(pos - perp * half, baseVel + ( dir) * binSpeed);
                    } else {
                        float speed = std::sqrt(G * dom.mass / r);
                        spawnBody(pos, dom.velocity + perp * speed);
                    }
                }
            }
        }
        return;  // don't pass click to camera
    }

    camera.onMouseButton(button, action, x, y);

    // right-click: pick a body to follow (click same body again to unfollow)
    if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS
            && gBodies && gProjection) {
        int w, h;
        glfwGetWindowSize(window, &w, &h);
        camera.trySelect(x, y, w, h, *gProjection, *gBodies);
    }
}

static void cursorPosCallback(GLFWwindow* window, double x, double y) {
    camera.onMouseMove(window, x, y);
}


int main() {
    if (!glfwInit()) {
        std::cerr << "Failed to init GLFW\n";
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    GLFWwindow* window = glfwCreateWindow(1200, 800, "Solar Simulator", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create window\n";
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    glfwSetScrollCallback(window, scrollCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetCursorPosCallback(window, cursorPosCallback);

    // init ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 150");

    const char* vertexShaderSource = R"(
        #version 150 core
        in vec3 aPos;
        in vec3 aNormal;
        uniform mat4 uMVP;
        out vec3 vNormal;
        out vec3 vFragPos;
        void main() {
            gl_Position = uMVP * vec4(aPos, 1.0);
            vNormal   = aNormal;
            vFragPos  = aPos;
        }
    )";

    const char* fragmentShaderSource = R"(
        #version 150 core
        in vec3 vNormal;
        in vec3 vFragPos;
        uniform vec3 uColor;
        uniform vec3 uLightPos;
        uniform bool uEmissive;
        out vec4 FragColor;
        void main() {
            if (uEmissive) {
                // overbright so the star blooms against dark space
                FragColor = vec4(uColor * 2.5, 1.0);
            } else {
                vec3 lightDir = normalize(uLightPos - vFragPos);
                float diff    = max(dot(normalize(vNormal), lightDir), 0.0);
                // smoothstep: dark side stays dark, lit side pops
                float lit     = smoothstep(0.0, 1.0, diff);
                float ambient = 0.04;
                FragColor = vec4(uColor * (ambient + lit), 1.0);
            }
        }
    )";

    GLuint shaderProgram = createShaderProgram(vertexShaderSource, fragmentShaderSource);

    // --- fabric shader (no normals, just position + flat color) ---
    const char* fabricVertSrc = R"(
        #version 150 core
        in vec3 aPos;
        uniform mat4 uMVP;
        void main() {
            gl_Position = uMVP * vec4(aPos, 1.0);
        }
    )";
    const char* fabricFragSrc = R"(
        #version 150 core
        out vec4 FragColor;
        void main() {
            FragColor = vec4(0.2f, 0.4f, 0.6f, 1.0);
        }
    )";
    GLuint fabricShader = createShaderProgram(fabricVertSrc, fabricFragSrc);
    GLint  fabricMVP    = glGetUniformLocation(fabricShader, "uMVP");

    // disk shader: per-vertex color (xyz + rgb, no normals)
    const char* diskVertSrc = R"(
        #version 150 core
        in vec3 aPos;
        in vec3 aColor;
        uniform mat4 uMVP;
        out vec3 vColor;
        void main() {
            gl_Position = uMVP * vec4(aPos, 1.0);
            vColor = aColor;
        }
    )";
    const char* diskFragSrc = R"(
        #version 150 core
        in vec3 vColor;
        out vec4 FragColor;
        void main() {
            FragColor = vec4(vColor, 1.0);
        }
    )";
    GLuint diskShader   = createShaderProgram(diskVertSrc, diskFragSrc);
    GLint  diskMVPLoc   = glGetUniformLocation(diskShader, "uMVP");

    GLuint diskVAO, diskVBO;
    glGenVertexArrays(1, &diskVAO);
    glGenBuffers(1, &diskVBO);

    glBindVertexArray(diskVAO);
    glBindBuffer(GL_ARRAY_BUFFER, diskVBO);
    GLint diskPosAttrib   = glGetAttribLocation(diskShader, "aPos");
    GLint diskColorAttrib = glGetAttribLocation(diskShader, "aColor");
    glEnableVertexAttribArray(diskPosAttrib);
    glVertexAttribPointer(diskPosAttrib,   3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(diskColorAttrib);
    glVertexAttribPointer(diskColorAttrib, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));

    FabricGrid fabric(80, 6.0f, 0.015f);

    GLuint fabricVAO, fabricVBO, fabricEBO;
    glGenVertexArrays(1, &fabricVAO);
    glGenBuffers(1, &fabricVBO);
    glGenBuffers(1, &fabricEBO);

    glBindVertexArray(fabricVAO);
    glBindBuffer(GL_ARRAY_BUFFER, fabricVBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, fabricEBO);

    // upload indices once — they never change
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(fabric.indices.size() * sizeof(unsigned int)),
                 fabric.indices.data(), GL_STATIC_DRAW);

    GLint fabricPos = glGetAttribLocation(fabricShader, "aPos");
    glEnableVertexAttribArray(fabricPos);
    glVertexAttribPointer(fabricPos, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

    auto makeBody = [](const BodyDef& d) {
        return CelestialBody(d.position, d.velocity, d.mass, d.radius,
                             d.colorR, d.colorG, d.colorB, d.emissive);
    };

    auto resetSolarSystem = [&](std::vector<CelestialBody>& bodies) {
        bodies.clear();
        bodies.push_back(makeBody(SUN));     bodies.back().name = "Sun";
        bodies.push_back(makeBody(MERCURY)); bodies.back().name = "Mercury";
        bodies.push_back(makeBody(VENUS));   bodies.back().name = "Venus";
        bodies.push_back(makeBody(EARTH));   bodies.back().name = "Earth";
        bodies.push_back(makeBody(MOON));    bodies.back().name = "Moon";
        bodies.push_back(makeBody(MARS));    bodies.back().name = "Mars";
        bodies.push_back(makeBody(JUPITER)); bodies.back().name = "Jupiter";
    };

    std::vector<CelestialBody> bodies;
    gBodies = &bodies;
    resetSolarSystem(bodies);

    Physics physics;

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    GLuint vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    GLuint vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);

    // vertex layout is now: x y z nx ny nz (6 floats per vertex)
    GLint posAttrib = glGetAttribLocation(shaderProgram, "aPos");
    glEnableVertexAttribArray(posAttrib);
    glVertexAttribPointer(posAttrib, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);

    GLint normAttrib = glGetAttribLocation(shaderProgram, "aNormal");
    glEnableVertexAttribArray(normAttrib);
    glVertexAttribPointer(normAttrib, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));

    glm::mat4 projection = glm::perspective(
        glm::radians(45.0f),
        1200.0f / 800.0f,
        0.1f,
        100.0f
    );
    gProjection = &projection;

    GLint uMVP      = glGetUniformLocation(shaderProgram, "uMVP");
    GLint uColor    = glGetUniformLocation(shaderProgram, "uColor");
    GLint uLightPos = glGetUniformLocation(shaderProgram, "uLightPos");
    GLint uEmissive = glGetUniformLocation(shaderProgram, "uEmissive");

    float lastTime = static_cast<float>(glfwGetTime());

    while (!glfwWindowShouldClose(window)) {
        float currentTime = static_cast<float>(glfwGetTime());
        float deltaTime = currentTime - lastTime;
        lastTime = currentTime;

        const float timeScale = 0.25f;
        const int substeps = 100;
        if (!gPaused) {
            float subDt = deltaTime * timeScale / substeps;
            for (int s = 0; s < substeps; ++s) {
                physics.step(bodies, subDt);
            }
            for (auto& body : bodies) body.recordTrail();
        }

        camera.updateFollow(bodies);
        fabric.update(bodies);

        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // draw fabric
        glm::mat4 mvp = projection * camera.viewMatrix() * glm::mat4(1.0f);
        glUseProgram(fabricShader);
        glUniformMatrix4fv(fabricMVP, 1, GL_FALSE, glm::value_ptr(mvp));
        glBindVertexArray(fabricVAO);
        glBindBuffer(GL_ARRAY_BUFFER, fabricVBO);
        glBufferData(GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(fabric.vertices.size() * sizeof(float)),
                     fabric.vertices.data(), GL_DYNAMIC_DRAW);
        glDrawElements(GL_LINES, static_cast<GLsizei>(fabric.indices.size()), GL_UNSIGNED_INT, 0);

        glUseProgram(shaderProgram);
        glUniformMatrix4fv(uMVP, 1, GL_FALSE, glm::value_ptr(mvp));
        glUniform3f(uLightPos, 0.0f, 0.0f, 0.0f);  // light at origin (sun)

        for (auto& body : bodies) {
            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(body.vertices.size() * sizeof(float)), body.vertices.data(), GL_DYNAMIC_DRAW);
            glBindVertexArray(vao);
            glUniform3f(uColor, body.colorR, body.colorG, body.colorB);
            glUniform1i(uEmissive, body.isEmissive ? 1 : 0);
            glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(body.vertices.size() / 6));
        }

        // draw accretion disks for black holes
        glUseProgram(diskShader);
        glUniformMatrix4fv(diskMVPLoc, 1, GL_FALSE, glm::value_ptr(mvp));
        glBindVertexArray(diskVAO);
        glBindBuffer(GL_ARRAY_BUFFER, diskVBO);
        for (auto& body : bodies) {
            if (!body.isBlackHole || body.diskVertices.empty()) continue;
            glBufferData(GL_ARRAY_BUFFER,
                         static_cast<GLsizeiptr>(body.diskVertices.size() * sizeof(float)),
                         body.diskVertices.data(), GL_DYNAMIC_DRAW);
            glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(body.diskVertices.size() / 6));
        }

        // draw trails (reuse diskVAO/diskVBO — same xyz+rgb layout)
        glUseProgram(diskShader);
        glUniformMatrix4fv(diskMVPLoc, 1, GL_FALSE, glm::value_ptr(mvp));
        glBindVertexArray(diskVAO);
        glBindBuffer(GL_ARRAY_BUFFER, diskVBO);
        for (auto& body : bodies) {
            int n = (int)body.trail.size();
            if (n < 2) continue;
            std::vector<float> tv;
            tv.reserve(n * 6);
            for (int i = 0; i < n; ++i) {
                float t = (float)i / (n - 1);  // 0=oldest (dim), 1=newest (bright)
                tv.push_back(body.trail[i].x);
                tv.push_back(body.trail[i].y);
                tv.push_back(body.trail[i].z);
                tv.push_back(body.colorR * t);
                tv.push_back(body.colorG * t);
                tv.push_back(body.colorB * t);
            }
            glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(tv.size() * sizeof(float)),
                         tv.data(), GL_DYNAMIC_DRAW);
            glDrawArrays(GL_LINE_STRIP, 0, n);
        }

        // --- ImGui UI ---
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // body labels — project 3D position to screen
        {
            int winW, winH;
            glfwGetWindowSize(window, &winW, &winH);
            auto* dl = ImGui::GetBackgroundDrawList();
            for (const auto& body : bodies) {
                if (body.name.empty()) continue;
                glm::vec4 clip = projection * camera.viewMatrix()
                               * glm::vec4(body.position + glm::vec3(0, body.radius * 1.5f, 0), 1.0f);
                if (clip.w <= 0.0f) continue;
                glm::vec3 ndc = glm::vec3(clip) / clip.w;
                if (ndc.z > 1.0f) continue;  // behind camera
                float sx = (ndc.x + 1.0f) * 0.5f * winW;
                float sy = (1.0f - ndc.y) * 0.5f * winH;
                dl->AddText(ImVec2(sx, sy),
                            IM_COL32(255, 255, 255, 200),
                            body.name.c_str());
            }
        }

        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(220, 0), ImGuiCond_Always);
        ImGui::Begin("Spawn Body", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

        // pause button at top
        if (gPaused) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
            if (ImGui::Button("Resume  (Space)")) gPaused = false;
            ImGui::PopStyleColor();
        } else {
            if (ImGui::Button("Pause   (Space)")) gPaused = true;
        }
        ImGui::Separator();

        // preset buttons
        ImGui::Text("Presets:");
        if (ImGui::Button("Asteroid")) {
            gPlaceMass=5e-10f; gPlaceRadius=0.008f;
            gPlaceColorR=0.5f; gPlaceColorG=0.4f; gPlaceColorB=0.3f;
            gPlaceEmissive=false; gPlaceIsBlackHole=false; gPlaceSchwarzschild=0.0f;
        }
        ImGui::SameLine();
        if (ImGui::Button("Planet")) {
            gPlaceMass=3e-6f; gPlaceRadius=0.025f;
            gPlaceColorR=0.3f; gPlaceColorG=0.6f; gPlaceColorB=0.9f;
            gPlaceEmissive=false; gPlaceIsBlackHole=false; gPlaceSchwarzschild=0.0f;
        }
        if (ImGui::Button("Gas Giant")) {
            gPlaceMass=1e-3f; gPlaceRadius=0.055f;
            gPlaceColorR=0.8f; gPlaceColorG=0.6f; gPlaceColorB=0.4f;
            gPlaceEmissive=false; gPlaceIsBlackHole=false; gPlaceSchwarzschild=0.0f;
        }
        ImGui::SameLine();
        if (ImGui::Button("Star")) {
            gPlaceMass=1.0f; gPlaceRadius=0.08f;
            gPlaceColorR=1.0f; gPlaceColorG=0.9f; gPlaceColorB=0.3f;
            gPlaceEmissive=true; gPlaceIsBlackHole=false; gPlaceSchwarzschild=0.0f;
        }
        if (ImGui::Button("Black Hole")) {
            gPlaceMass=5.0f; gPlaceRadius=0.04f;
            gPlaceColorR=0.02f; gPlaceColorG=0.02f; gPlaceColorB=0.02f;
            gPlaceEmissive=false; gPlaceIsBlackHole=true; gPlaceSchwarzschild=0.18f;
            gPlaceBinary=false;
        }
        if (ImGui::Button("Binary Stars")) {
            gPlaceMass=1.0f; gPlaceRadius=0.08f;
            gPlaceColorR=1.0f; gPlaceColorG=0.9f; gPlaceColorB=0.3f;
            gPlaceEmissive=true; gPlaceIsBlackHole=false; gPlaceSchwarzschild=0.0f;
            gPlaceBinary=true;
        }
        if (gPlaceBinary) ImGui::SliderFloat("Separation", &gBinarySep, 0.1f, 2.0f);

        ImGui::Separator();
        ImGui::SliderFloat("Mass", &gPlaceMass, 1e-10f, 2.0f, "%.2e");
        ImGui::SliderFloat("Radius", &gPlaceRadius, 0.005f, 0.12f);
        ImGui::ColorEdit3("Color", &gPlaceColorR);
        ImGui::Checkbox("Emissive", &gPlaceEmissive);
        ImGui::SameLine();
        ImGui::Checkbox("Stationary", &gPlaceStationary);

        ImGui::Separator();
        if (gPlacing) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
            if (ImGui::Button("Cancel Placement")) gPlacing = false;
            ImGui::PopStyleColor();
            ImGui::TextColored(ImVec4(1,1,0,1), "Click in space to place");
        } else {
            if (ImGui::Button("Place Body")) gPlacing = true;
        }

        ImGui::Separator();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.1f, 0.1f, 1.0f));
        if (ImGui::Button("Clear All")) {
            bodies.clear();
            gPlacing = false;
            camera = Camera(8.0f, 0.0f, 0.25f);
        }
        ImGui::PopStyleColor();
        ImGui::SameLine();
        if (ImGui::Button("Reset Solar System")) {
            resetSolarSystem(bodies);
            gPlacing = false;
            camera = Camera(8.0f, 0.0f, 0.25f);
        }

        ImGui::End();
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        // --- end ImGui ---

        glfwSwapBuffers(window);
        glfwPollEvents();

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, GLFW_TRUE);

        static bool spaceWasDown = false;
        bool spaceIsDown = (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS);
        if (spaceIsDown && !spaceWasDown) gPaused = !gPaused;
        spaceWasDown = spaceIsDown;

        // press R to spawn a random body
        static bool rWasDown = false;
        bool rIsDown = (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS);
        if (rIsDown && !rWasDown) {
            static std::mt19937 rng(std::random_device{}());
            std::uniform_real_distribution<float> angleDist(0.0f, 6.2832f);
            std::uniform_real_distribution<float> radiusDist(0.5f, 5.5f);
            std::uniform_real_distribution<float> colorDist(0.2f, 1.0f);
            std::uniform_real_distribution<float> massDist(1e-7f, 5e-4f);

            float angle  = angleDist(rng);
            float radius = radiusDist(rng);
            float speed  = std::sqrt(G * 1.0f / radius);  // circular orbit speed

            glm::vec3 pos(radius * std::cos(angle), 0.0f, radius * std::sin(angle));
            glm::vec3 vel(-speed * std::sin(angle), 0.0f, speed * std::cos(angle));
            float mass   = massDist(rng);
            float visual = 0.01f + mass * 50.0f;  // rough visual size
            if (visual > 0.06f) visual = 0.06f;

            bodies.emplace_back(pos, vel, mass,
                                visual,
                                colorDist(rng), colorDist(rng), colorDist(rng),
                                false);
        }
        rWasDown = rIsDown;
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glDeleteBuffers(1, &diskVBO);
    glDeleteVertexArrays(1, &diskVAO);
    glDeleteProgram(diskShader);

    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
    glDeleteProgram(shaderProgram);
    glDeleteBuffers(1, &fabricVBO);
    glDeleteBuffers(1, &fabricEBO);
    glDeleteVertexArrays(1, &fabricVAO);
    glDeleteProgram(fabricShader);

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
