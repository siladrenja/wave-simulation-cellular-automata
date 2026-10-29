#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 800;
constexpr int kSimulationSize = 192;
constexpr float kWorldSize = 40.0f;
constexpr float kWallHeight = 2.8f;

std::filesystem::path findShaderDirectory(const char* executable) {
    const auto executableDir = std::filesystem::absolute(executable).parent_path();
    const std::array candidates{
        executableDir / "shaders",
        std::filesystem::current_path() / "shaders",
        std::filesystem::path(WAVE_CA_SHADER_DIR),
    };
    for (const auto& candidate : candidates) {
        if (std::filesystem::exists(candidate / "fullscreen.vert")) return candidate;
    }
    throw std::runtime_error("Could not find the shaders directory beside the executable or in the working directory");
}

std::string readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file) throw std::runtime_error("Could not open shader: " + path);
    std::ostringstream text;
    text << file.rdbuf();
    return text.str();
}

GLuint compileShader(GLenum type, const std::string& source) {
    const GLuint shader = glCreateShader(type);
    const char* ptr = source.c_str();
    glShaderSource(shader, 1, &ptr, nullptr);
    glCompileShader(shader);
    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint length = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
        std::string log(static_cast<size_t>(length), '\0');
        glGetShaderInfoLog(shader, length, nullptr, log.data());
        glDeleteShader(shader);
        throw std::runtime_error("Shader compilation failed:\n" + log);
    }
    return shader;
}

GLuint makeProgram(const std::string& vertexPath, const std::string& fragmentPath) {
    const GLuint vs = compileShader(GL_VERTEX_SHADER, readFile(vertexPath));
    const GLuint fs = compileShader(GL_FRAGMENT_SHADER, readFile(fragmentPath));
    const GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    glDeleteShader(vs);
    glDeleteShader(fs);
    GLint ok = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint length = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
        std::string log(static_cast<size_t>(length), '\0');
        glGetProgramInfoLog(program, length, nullptr, log.data());
        glDeleteProgram(program);
        throw std::runtime_error("Program link failed:\n" + log);
    }
    return program;
}

void setInt(GLuint p, const char* name, int v) { glUniform1i(glGetUniformLocation(p, name), v); }
void setFloat(GLuint p, const char* name, float v) { glUniform1f(glGetUniformLocation(p, name), v); }
void setVec2(GLuint p, const char* name, const glm::vec2& v) { glUniform2fv(glGetUniformLocation(p, name), 1, glm::value_ptr(v)); }
void setMat4(GLuint p, const char* name, const glm::mat4& v) { glUniformMatrix4fv(glGetUniformLocation(p, name), 1, GL_FALSE, glm::value_ptr(v)); }

class WaveSimulation {
public:
    explicit WaveSimulation(const std::string& shaderDir)
        : obstacles_(static_cast<size_t>(kSimulationSize * kSimulationSize), 0) {
        updateProgram_ = makeProgram(shaderDir + "/fullscreen.vert", shaderDir + "/wave_update.frag");
        glGenVertexArrays(1, &quadVao_);
        glGenFramebuffers(2, fbos_.data());
        glGenTextures(2, states_.data());
        const std::vector<float> zeroState(static_cast<size_t>(kSimulationSize * kSimulationSize * 2), 0.0f);
        for (int i = 0; i < 2; ++i) {
            glBindTexture(GL_TEXTURE_2D, states_[i]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RG32F, kSimulationSize, kSimulationSize, 0, GL_RG, GL_FLOAT, zeroState.data());
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glBindFramebuffer(GL_FRAMEBUFFER, fbos_[i]);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, states_[i], 0);
            if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
                throw std::runtime_error("Wave framebuffer is incomplete");
        }
        glGenTextures(1, &obstacleTexture_);
        glBindTexture(GL_TEXTURE_2D, obstacleTexture_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, kSimulationSize, kSimulationSize, 0, GL_RED, GL_UNSIGNED_BYTE, obstacles_.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    ~WaveSimulation() {
        glDeleteProgram(updateProgram_);
        glDeleteTextures(2, states_.data());
        glDeleteTextures(1, &obstacleTexture_);
        glDeleteFramebuffers(2, fbos_.data());
        glDeleteVertexArrays(1, &quadVao_);
    }

    void update() {
        const int next = 1 - current_;
        glBindFramebuffer(GL_FRAMEBUFFER, fbos_[next]);
        glViewport(0, 0, kSimulationSize, kSimulationSize);
        glDisable(GL_DEPTH_TEST);
        glUseProgram(updateProgram_);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, states_[current_]);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, obstacleTexture_);
        setInt(updateProgram_, "uState", 0);
        setInt(updateProgram_, "uObstacles", 1);
        setVec2(updateProgram_, "uTexel", glm::vec2(1.0f / kSimulationSize));
        setFloat(updateProgram_, "uDamping", 0.9965f);
        setInt(updateProgram_, "uHasRipple", ripplePending_ ? 1 : 0);
        setVec2(updateProgram_, "uRipple", ripple_);
        setFloat(updateProgram_, "uRippleAmplitude", rippleAmplitude_);
        setFloat(updateProgram_, "uRippleRadius", 3.2f / kSimulationSize);
        glBindVertexArray(quadVao_);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        current_ = next;
        ripplePending_ = false;
    }

    void addRipple(glm::vec2 uv, float amplitude = 0.85f) {
        ripple_ = glm::clamp(uv, glm::vec2(0.0f), glm::vec2(1.0f));
        rippleAmplitude_ = amplitude;
        ripplePending_ = true;
    }

    bool paint(glm::vec2 uv, int radius = 3) {
        const int cx = std::clamp(static_cast<int>(uv.x * kSimulationSize), 0, kSimulationSize - 1);
        const int cy = std::clamp(static_cast<int>(uv.y * kSimulationSize), 0, kSimulationSize - 1);
        bool changed = false;
        for (int y = std::max(0, cy - radius); y <= std::min(kSimulationSize - 1, cy + radius); ++y) {
            for (int x = std::max(0, cx - radius); x <= std::min(kSimulationSize - 1, cx + radius); ++x) {
                if ((x - cx) * (x - cx) + (y - cy) * (y - cy) > radius * radius) continue;
                auto& cell = obstacles_[static_cast<size_t>(y * kSimulationSize + x)];
                if (!cell) { cell = 255; changed = true; }
            }
        }
        if (changed) {
            glBindTexture(GL_TEXTURE_2D, obstacleTexture_);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, kSimulationSize, kSimulationSize, GL_RED, GL_UNSIGNED_BYTE, obstacles_.data());
            obstacleRevision_++;
        }
        return changed;
    }

    GLuint stateTexture() const { return states_[current_]; }
    GLuint obstacleTexture() const { return obstacleTexture_; }
    const std::vector<unsigned char>& obstacles() const { return obstacles_; }
    unsigned obstacleRevision() const { return obstacleRevision_; }

private:
    GLuint updateProgram_ = 0, quadVao_ = 0, obstacleTexture_ = 0;
    std::array<GLuint, 2> states_{}, fbos_{};
    int current_ = 0;
    std::vector<unsigned char> obstacles_;
    glm::vec2 ripple_{0.5f};
    float rippleAmplitude_ = 0.85f;
    bool ripplePending_ = false;
    unsigned obstacleRevision_ = 0;
};

class Camera {
public:
    glm::mat4 view() const { return glm::lookAt(position(), glm::vec3(0), glm::vec3(0, 1, 0)); }
    glm::vec3 position() const {
        const float cp = std::cos(pitch_);
        return distance_ * glm::vec3(cp * std::sin(yaw_), std::sin(pitch_), cp * std::cos(yaw_));
    }
    void orbit(float dx, float dy) {
        yaw_ -= dx * 0.006f;
        pitch_ = std::clamp(pitch_ + dy * 0.006f, 0.12f, 1.45f);
    }
    void zoom(float amount) { distance_ = std::clamp(distance_ * std::pow(0.88f, amount), 18.0f, 90.0f); }
private:
    float yaw_ = 0.75f, pitch_ = 0.65f, distance_ = 52.0f;
};

class Renderer {
public:
    explicit Renderer(const std::string& shaderDir) {
        waterProgram_ = makeProgram(shaderDir + "/water.vert", shaderDir + "/water.frag");
        wallProgram_ = makeProgram(shaderDir + "/wall.vert", shaderDir + "/wall.frag");
        mapProgram_ = makeProgram(shaderDir + "/fullscreen.vert", shaderDir + "/map.frag");
        glGenVertexArrays(1, &quadVao_);
        createWaterMesh();
        createCube();
    }
    ~Renderer() {
        glDeleteProgram(waterProgram_); glDeleteProgram(wallProgram_); glDeleteProgram(mapProgram_);
        glDeleteVertexArrays(1, &waterVao_); glDeleteVertexArrays(1, &wallVao_); glDeleteVertexArrays(1, &quadVao_);
        glDeleteBuffers(1, &waterVbo_); glDeleteBuffers(1, &waterEbo_); glDeleteBuffers(1, &cubeVbo_); glDeleteBuffers(1, &instanceVbo_);
    }

    void draw3D(const WaveSimulation& sim, const glm::mat4& view, const glm::mat4& projection) {
        const glm::mat4 vp = projection * view;
        glEnable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
        glUseProgram(waterProgram_);
        setMat4(waterProgram_, "uVP", vp);
        setFloat(waterProgram_, "uWorldSize", kWorldSize);
        setVec2(waterProgram_, "uTexel", glm::vec2(1.0f / kSimulationSize));
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, sim.stateTexture());
        glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, sim.obstacleTexture());
        setInt(waterProgram_, "uState", 0); setInt(waterProgram_, "uObstacles", 1);
        glBindVertexArray(waterVao_);
        glDrawElements(GL_TRIANGLES, waterIndexCount_, GL_UNSIGNED_INT, nullptr);

        refreshInstances(sim);
        if (instanceCount_) {
            glUseProgram(wallProgram_);
            setMat4(wallProgram_, "uVP", vp);
            setFloat(wallProgram_, "uCellSize", kWorldSize / kSimulationSize);
            setFloat(wallProgram_, "uWallHeight", kWallHeight);
            glBindVertexArray(wallVao_);
            glDrawArraysInstanced(GL_TRIANGLES, 0, 36, instanceCount_);
        }
        glBindVertexArray(0);
    }

    void draw2D(const WaveSimulation& sim) {
        glDisable(GL_DEPTH_TEST);
        glUseProgram(mapProgram_);
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, sim.stateTexture());
        glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, sim.obstacleTexture());
        setInt(mapProgram_, "uState", 0); setInt(mapProgram_, "uObstacles", 1);
        glBindVertexArray(quadVao_);
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }

private:
    void createWaterMesh() {
        std::vector<glm::vec2> vertices;
        std::vector<unsigned> indices;
        vertices.reserve(static_cast<size_t>(kSimulationSize * kSimulationSize));
        for (int y = 0; y < kSimulationSize; ++y)
            for (int x = 0; x < kSimulationSize; ++x)
                vertices.emplace_back((x + 0.5f) / kSimulationSize, (y + 0.5f) / kSimulationSize);
        for (int y = 0; y < kSimulationSize - 1; ++y) for (int x = 0; x < kSimulationSize - 1; ++x) {
            const unsigned a = static_cast<unsigned>(y * kSimulationSize + x), b = a + 1;
            const unsigned c = a + kSimulationSize, d = c + 1;
            indices.insert(indices.end(), {a, c, b, b, c, d});
        }
        waterIndexCount_ = static_cast<GLsizei>(indices.size());
        glGenVertexArrays(1, &waterVao_); glGenBuffers(1, &waterVbo_); glGenBuffers(1, &waterEbo_);
        glBindVertexArray(waterVao_);
        glBindBuffer(GL_ARRAY_BUFFER, waterVbo_); glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(glm::vec2), vertices.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(glm::vec2), nullptr); glEnableVertexAttribArray(0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, waterEbo_); glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned), indices.data(), GL_STATIC_DRAW);
    }

    void createCube() {
        constexpr float v[] = {
            -0.5f,0,-0.5f, 0.5f,0,-0.5f, 0.5f,1,-0.5f, -0.5f,0,-0.5f, 0.5f,1,-0.5f,-0.5f,1,-0.5f,
             0.5f,0, 0.5f,-0.5f,0, 0.5f,-0.5f,1, 0.5f, 0.5f,0, 0.5f,-0.5f,1, 0.5f, 0.5f,1, 0.5f,
            -0.5f,0, 0.5f,-0.5f,0,-0.5f,-0.5f,1,-0.5f,-0.5f,0, 0.5f,-0.5f,1,-0.5f,-0.5f,1, 0.5f,
             0.5f,0,-0.5f, 0.5f,0, 0.5f, 0.5f,1, 0.5f, 0.5f,0,-0.5f, 0.5f,1, 0.5f, 0.5f,1,-0.5f,
            -0.5f,1,-0.5f, 0.5f,1,-0.5f, 0.5f,1, 0.5f,-0.5f,1,-0.5f, 0.5f,1, 0.5f,-0.5f,1, 0.5f,
            -0.5f,0, 0.5f, 0.5f,0, 0.5f, 0.5f,0,-0.5f,-0.5f,0, 0.5f, 0.5f,0,-0.5f,-0.5f,0,-0.5f};
        glGenVertexArrays(1, &wallVao_); glGenBuffers(1, &cubeVbo_); glGenBuffers(1, &instanceVbo_);
        glBindVertexArray(wallVao_);
        glBindBuffer(GL_ARRAY_BUFFER, cubeVbo_); glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr); glEnableVertexAttribArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, instanceVbo_); glBufferData(GL_ARRAY_BUFFER, kSimulationSize * kSimulationSize * sizeof(glm::vec2), nullptr, GL_DYNAMIC_DRAW);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(glm::vec2), nullptr); glEnableVertexAttribArray(1); glVertexAttribDivisor(1, 1);
    }

    void refreshInstances(const WaveSimulation& sim) {
        if (cachedRevision_ == sim.obstacleRevision()) return;
        std::vector<glm::vec2> positions;
        const auto& mask = sim.obstacles();
        for (int y = 0; y < kSimulationSize; ++y) for (int x = 0; x < kSimulationSize; ++x)
            if (mask[static_cast<size_t>(y * kSimulationSize + x)])
                positions.emplace_back((x + 0.5f) / kSimulationSize * kWorldSize - kWorldSize * 0.5f,
                                       (y + 0.5f) / kSimulationSize * kWorldSize - kWorldSize * 0.5f);
        instanceCount_ = static_cast<GLsizei>(positions.size());
        glBindBuffer(GL_ARRAY_BUFFER, instanceVbo_);
        glBufferSubData(GL_ARRAY_BUFFER, 0, positions.size() * sizeof(glm::vec2), positions.data());
        cachedRevision_ = sim.obstacleRevision();
    }

    GLuint waterProgram_ = 0, wallProgram_ = 0, mapProgram_ = 0;
    GLuint waterVao_ = 0, waterVbo_ = 0, waterEbo_ = 0, wallVao_ = 0, cubeVbo_ = 0, instanceVbo_ = 0, quadVao_ = 0;
    GLsizei waterIndexCount_ = 0, instanceCount_ = 0;
    unsigned cachedRevision_ = ~0u;
};

struct App {
    Camera camera;
    bool mapMode = false, spaceWasDown = false, rightDragging = false, leftWasDown = false;
    double lastX = 0, lastY = 0;
};

glm::vec2 cursorUv(GLFWwindow* window, double x, double y) {
    int ww = 1, wh = 1;
    glfwGetWindowSize(window, &ww, &wh);
    return {static_cast<float>(x / ww), 1.0f - static_cast<float>(y / wh)};
}

bool pickWater(GLFWwindow* window, const Camera& camera, double x, double y, glm::vec2& uv) {
    int ww = 1, wh = 1;
    glfwGetWindowSize(window, &ww, &wh);
    const float nx = 2.0f * static_cast<float>(x) / ww - 1.0f;
    const float ny = 1.0f - 2.0f * static_cast<float>(y) / wh;
    const glm::mat4 projection = glm::perspective(glm::radians(45.0f), static_cast<float>(ww) / wh, 0.1f, 200.0f);
    const glm::mat4 inv = glm::inverse(projection * camera.view());
    glm::vec4 nearPoint = inv * glm::vec4(nx, ny, -1, 1), farPoint = inv * glm::vec4(nx, ny, 1, 1);
    nearPoint /= nearPoint.w; farPoint /= farPoint.w;
    const glm::vec3 origin(nearPoint), direction = glm::normalize(glm::vec3(farPoint - nearPoint));
    if (std::abs(direction.y) < 1e-5f) return false;
    const float t = -origin.y / direction.y;
    if (t <= 0) return false;
    const glm::vec3 hit = origin + t * direction;
    uv = glm::vec2(hit.x, hit.z) / kWorldSize + 0.5f;
    return uv.x >= 0 && uv.x <= 1 && uv.y >= 0 && uv.y <= 1;
}
} // namespace

int main(int argc, char** argv) {
    GLFWwindow* window = nullptr;
    try {
        glfwSetErrorCallback([](int code, const char* message) { std::cerr << "GLFW error " << code << ": " << message << '\n'; });
        if (!glfwInit()) throw std::runtime_error("GLFW initialization failed");
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3); glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        window = glfwCreateWindow(kWindowWidth, kWindowHeight, "Wave CA - 3D", nullptr, nullptr);
        if (!window) throw std::runtime_error("Could not create an OpenGL 3.3 window. Check the graphics driver and OpenGL support.");
        glfwMakeContextCurrent(window); glfwSwapInterval(1);
        glewExperimental = GL_TRUE;
        const GLenum glewStatus = glewInit();
        if (glewStatus != GLEW_OK)
            throw std::runtime_error("Could not initialize GLEW: " + std::string(reinterpret_cast<const char*>(glewGetErrorString(glewStatus))));
        // GLEW can emit GL_INVALID_ENUM while probing core-profile extensions.
        while (glGetError() != GL_NO_ERROR) {}
        std::cout << "OpenGL: " << glGetString(GL_VERSION) << "\nRenderer: " << glGetString(GL_RENDERER) << '\n';
        {
            const std::string shaders = findShaderDirectory(argc > 0 ? argv[0] : "wave_ca").string();
            WaveSimulation simulation(shaders); Renderer renderer(shaders); App app;
            glfwSetWindowUserPointer(window, &app);
            glfwSetScrollCallback(window, [](GLFWwindow* w, double, double dy) { static_cast<App*>(glfwGetWindowUserPointer(w))->camera.zoom(static_cast<float>(dy)); });
            glClearColor(0.035f, 0.055f, 0.085f, 1.0f);
            double accumulator = 0.0, previousTime = glfwGetTime();

            while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
            if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(window, true);
            const bool space = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;
            if (space && !app.spaceWasDown) {
                app.mapMode = !app.mapMode;
                glfwSetWindowTitle(window, app.mapMode ? "Wave CA - 2D obstacle editor" : "Wave CA - 3D");
            }
            app.spaceWasDown = space;
            double mx, my; glfwGetCursorPos(window, &mx, &my);
            const bool right = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
            if (right && app.rightDragging && !app.mapMode) app.camera.orbit(static_cast<float>(mx - app.lastX), static_cast<float>(my - app.lastY));
            app.rightDragging = right;
            const bool left = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
            if (left) {
                if (app.mapMode) simulation.paint(cursorUv(window, mx, my));
                else if (!app.leftWasDown) { glm::vec2 uv; if (pickWater(window, app.camera, mx, my, uv)) simulation.addRipple(uv); }
            }
            app.leftWasDown = left;
            app.lastX = mx; app.lastY = my;

            const double now = glfwGetTime();
            accumulator = std::min(accumulator + now - previousTime, 0.1); previousTime = now;
            while (accumulator >= 1.0 / 120.0) { simulation.update(); accumulator -= 1.0 / 120.0; }

            int width, height; glfwGetFramebufferSize(window, &width, &height);
            if (width == 0 || height == 0) { glfwWaitEventsTimeout(0.05); continue; }
            glBindFramebuffer(GL_FRAMEBUFFER, 0); glViewport(0, 0, width, height);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            if (app.mapMode) renderer.draw2D(simulation);
            else renderer.draw3D(simulation, app.camera.view(), glm::perspective(glm::radians(45.0f), static_cast<float>(width) / height, 0.1f, 200.0f));
                glfwSwapBuffers(window);
            }
        }
        glfwDestroyWindow(window); glfwTerminate();
        return EXIT_SUCCESS;
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << '\n';
        if (window) glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }
}
