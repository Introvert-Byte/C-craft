#include "Shader.h"

#include <fstream>
#include <iostream>
#include <sstream>

namespace {

const char* DEFAULT_VERTEX_SHADER = R"glsl(
#version 430 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

out vec3 vNormal;
out vec3 vFragPos;
out vec2 vTexCoord;

void main() {
    vFragPos = vec3(uModel * vec4(aPos, 1.0));
    vNormal = mat3(uModel) * aNormal;
    vTexCoord = aTexCoord;
    gl_Position = uProjection * uView * vec4(vFragPos, 1.0);
}
)glsl";

const char* DEFAULT_FRAGMENT_SHADER = R"glsl(
#version 430 core
out vec4 FragColor;

in vec3 vNormal;
in vec3 vFragPos;
in vec2 vTexCoord;

uniform sampler2D uTextureAtlas;
uniform vec3 uLightPos;
uniform vec3 uLightColor;
uniform vec3 uViewPos;
uniform vec3 uBlockColor;

void main() {
    vec4 texel = texture(uTextureAtlas, vTexCoord);

    vec3 ambient = 0.4f * uLightColor;
    vec3 norm = normalize(vNormal);
    vec3 lightDir = normalize(uLightPos - vFragPos);
    float diff = max(dot(norm, lightDir), 0.0f);
    vec3 diffuse = diff * uLightColor;

    vec3 lighting = (ambient + diffuse) * texel.rgb * uBlockColor;
    FragColor = vec4(lighting, texel.a);
}
)glsl";

std::string loadFile(const char* path) {
    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file.is_open()) return {};
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

} // namespace

Shader::Shader() = default;
Shader::~Shader() { if (m_programID) glDeleteProgram(m_programID); }

Shader::Shader(Shader&& other) noexcept
    : m_programID(other.m_programID), m_linked(other.m_linked),
      m_uniformCache(std::move(other.m_uniformCache)) {
    other.m_programID = 0;
    other.m_linked = false;
}

Shader& Shader::operator=(Shader&& other) noexcept {
    if (this != &other) {
        if (m_programID) glDeleteProgram(m_programID);
        m_programID = other.m_programID;
        m_linked = other.m_linked;
        m_uniformCache = std::move(other.m_uniformCache);
        other.m_programID = 0;
        other.m_linked = false;
    }
    return *this;
}

GLuint Shader::compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[1024];
        glGetShaderInfoLog(shader, sizeof(infoLog), nullptr, infoLog);
        std::cerr << "Shader compile error:\n" << infoLog << "\n";
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

bool Shader::linkProgram(GLuint program) {
    glLinkProgram(program);
    GLint success = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[1024];
        glGetProgramInfoLog(program, sizeof(infoLog), nullptr, infoLog);
        std::cerr << "Shader link error:\n" << infoLog << "\n";
        return false;
    }
    return true;
}

bool Shader::loadFromString(const char* vertexSource,
                            const char* fragmentSource) {
    if (m_programID) glDeleteProgram(m_programID);
    m_programID = glCreateProgram();
    m_linked = false;

    GLuint vs = compileShader(GL_VERTEX_SHADER, vertexSource);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragmentSource);
    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return false;
    }

    glAttachShader(m_programID, vs);
    glAttachShader(m_programID, fs);
    m_linked = linkProgram(m_programID);

    glDeleteShader(vs);
    glDeleteShader(fs);
    m_uniformCache.clear();
    return m_linked;
}

bool Shader::loadFromFiles(const char* vertexPath, const char* fragmentPath) {
    std::string vsSrc = loadFile(vertexPath);
    std::string fsSrc = loadFile(fragmentPath);
    if (vsSrc.empty() || fsSrc.empty()) {
        return loadFromString(DEFAULT_VERTEX_SHADER, DEFAULT_FRAGMENT_SHADER);
    }
    return loadFromString(vsSrc.c_str(), fsSrc.c_str());
}

void Shader::use() const { glUseProgram(m_programID); }

GLint Shader::getUniformLocation(const std::string& name) {
    auto it = m_uniformCache.find(name);
    if (it != m_uniformCache.end()) return it->second;
    GLint loc = glGetUniformLocation(m_programID, name.c_str());
    m_uniformCache[name] = loc;
    return loc;
}

void Shader::setUniform1i(const std::string& name, int value) {
    glUniform1i(getUniformLocation(name), value);
}
void Shader::setUniform1f(const std::string& name, float value) {
    glUniform1f(getUniformLocation(name), value);
}
void Shader::setUniformMatrix4fv(const std::string& name,
                                 const glm::mat4& matrix) {
    glUniformMatrix4fv(getUniformLocation(name), 1, GL_FALSE,
                       glm::value_ptr(matrix));
}
void Shader::setUniform3f(const std::string& name, const glm::vec3& value) {
    glUniform3fv(getUniformLocation(name), 1, glm::value_ptr(value));
}
void Shader::setUniformVec3(const std::string& name, float x, float y,
                            float z) {
    glUniform3f(getUniformLocation(name), x, y, z);
}
