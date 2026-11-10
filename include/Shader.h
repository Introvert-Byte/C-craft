#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <string>
#include <unordered_map>

// OpenGL shader program wrapper: compiles a vertex and fragment shader,
// links them into a program and provides type-safe uniform setters.
class Shader {
public:
    Shader();
    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
    Shader(Shader&& other) noexcept;
    Shader& operator=(Shader&& other) noexcept;

    // Build from source strings. Returns false on compile/link failure.
    bool loadFromString(const char* vertexSource, const char* fragmentSource);

    // Load from files. Falls back to embedded default sources if missing.
    bool loadFromFiles(const char* vertexPath, const char* fragmentPath);

    void use() const;
    GLuint id() const { return m_programID; }
    bool valid() const { return m_programID != 0 && m_linked; }

    // Uniform setters (program must be bound via use()).
    void setUniform1i(const std::string& name, int value);
    void setUniform1f(const std::string& name, float value);
    void setUniformMatrix4fv(const std::string& name, const glm::mat4& matrix);
    void setUniform3f(const std::string& name, const glm::vec3& value);
    void setUniformVec3(const std::string& name, float x, float y, float z);

private:
    GLuint m_programID = 0;
    bool m_linked = false;
    std::unordered_map<std::string, GLint> m_uniformCache;

    GLint getUniformLocation(const std::string& name);
    static GLuint compileShader(GLenum type, const char* source);
    static bool linkProgram(GLuint program);
};
