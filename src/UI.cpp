#include "UI.h"

#include <glad/glad.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>

#include <glm/gtc/type_ptr.hpp>

namespace {

const char* VERT_SHADER = R"glsl(
#version 430 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;

uniform mat4 uProj;

out vec2 vTexCoord;

void main() {
    gl_Position = uProj * vec4(aPos, 0.0, 1.0);
    vTexCoord = aTexCoord;
}
)glsl";

const char* FRAG_SHADER = R"glsl(
#version 430 core
in vec2 vTexCoord;
out vec4 FragColor;

uniform sampler2D uFontAtlas;
uniform vec3 uTextColor;

void main() {
    float alpha = texture(uFontAtlas, vTexCoord).r;
    if (alpha < 0.05f) discard;
    FragColor = vec4(uTextColor, alpha);
}
)glsl";

GLuint compileShader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(s, sizeof(log), nullptr, log);
        std::cerr << "Font shader error:\n" << log << "\n";
        glDeleteShader(s);
        return 0;
    }
    return s;
}

} // namespace

FontRenderer::FontRenderer() = default;
FontRenderer::~FontRenderer() {
    if (m_vao) glDeleteVertexArrays(1, &m_vao);
    if (m_vbo) glDeleteBuffers(1, &m_vbo);
    if (m_texture) glDeleteTextures(1, &m_texture);
    if (m_shader) glDeleteProgram(m_shader);
}

void FontRenderer::buildAtlas() {
    const int cols = 96;
    m_charW = 8;
    m_charH = 8;
    m_atlasW = cols * m_charW;
    m_atlasH = m_charH;

    std::vector<std::uint8_t> px(m_atlasW * m_atlasH * 4, 0);

    auto setPixel = [&](int cx, int cy, int px_, int py_) {
        int ox = cx * m_charW + px_;
        int oy = cy * m_charH + py_;
        if (ox >= 0 && ox < m_atlasW && oy >= 0 && oy < m_atlasH) {
            int i = (oy * m_atlasW + ox) * 4;
            px[i + 3] = 255;
            px[i + 0] = 200;
            px[i + 1] = 220;
            px[i + 2] = 255;
        }
    };

    // Draw a simple character glyph for each printable ASCII char.
    for (int code = 32; code <= 127; ++code) {
        int cell = code - 32;
        int cx = cell % cols;
        int cy = cell / cols;
        int gx = cx * m_charW + 1;
        int gy = cy * m_charH + 1;

        // Render character using simple dot-matrix patterns.
        // Each char gets a unique pattern so they're distinguishable.
        int seed = code * 31 + 7;
        for (int row = 0; row < 7; ++row) {
            int rowSeed = seed + row * 17;
            for (int col = 0; col < 5; ++col) {
                rowSeed = rowSeed * 1103515245 + 12345;
                bool on = ((rowSeed >> 16) & 1) == 1 ||
                          (row == 0 && col > 0 && col < 4) ||
                          (row == 6 && col > 0 && col < 4) ||
                          (col == 0 && row > 0 && row < 6) ||
                          (col == 4 && row > 0 && row < 6);
                if (on)
                    setPixel(cx, cy, gx + col, gy + row);
            }
        }
    }

    glGenTextures(1, &m_texture);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_atlasW, m_atlasH, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    glBindTexture(GL_TEXTURE_2D, 0);
}

GLuint FontRenderer::compileProgram() {
    GLuint vs = compileShader(GL_VERTEX_SHADER, VERT_SHADER);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, FRAG_SHADER);
    if (!vs || !fs) return 0;
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
        std::cerr << "Font link error:\n" << log << "\n";
        glDeleteProgram(prog);
        return 0;
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}

bool FontRenderer::init() {
    m_shader = compileProgram();
    if (!m_shader) return false;
    m_locProj = glGetUniformLocation(m_shader, "uProj");
    m_locColor = glGetUniformLocation(m_shader, "uTextColor");
    m_locTex = glGetUniformLocation(m_shader, "uFontAtlas");

    buildAtlas();

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, 8192 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          reinterpret_cast<void*>(2 * sizeof(float)));
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    return true;
}

void FontRenderer::begin(int screenWidth, int screenHeight) {
    glUseProgram(m_shader);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

    glm::mat4 proj = glm::ortho(0.0f, (float)screenWidth,
                                (float)screenHeight, 0.0f, -1.0f, 1.0f);
    glUniformMatrix4fv(m_locProj, 1, GL_FALSE, glm::value_ptr(proj));
    glUniform1i(m_locTex, 0);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
}

void FontRenderer::end() {
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glUseProgram(0);
}

void FontRenderer::drawText(float x, float y, const std::string& text,
                            float scale, const glm::vec3& color) {
    if (text.empty()) return;

    glUniform3f(m_locColor, color.r, color.g, color.b);

    std::vector<float> verts;
    verts.reserve(text.size() * 4 * 4);

    float cx = x;
    float cy = y;
    float cw = m_charW * scale;
    float ch = m_charH * scale;

    int cols = m_atlasW / m_charW;

    for (char ch_ : text) {
        int code = (unsigned char)ch_;
        if (code < 32 || code > 127) {
            cx += cw * 0.6f;
            continue;
        }
        int cell = code - 32;
        int col = cell % cols;
        float tx = (float)(col * m_charW) / (float)m_atlasW;
        float tw = (float)m_charW / (float)m_atlasW;

        float x0 = cx;
        float x1 = cx + cw;
        float y0 = cy;
        float y1 = cy + ch;

        verts.push_back(x0); verts.push_back(y0); verts.push_back(tx);             verts.push_back(0.0f);
        verts.push_back(x1); verts.push_back(y0); verts.push_back(tx + tw);        verts.push_back(0.0f);
        verts.push_back(x1); verts.push_back(y1); verts.push_back(tx + tw);        verts.push_back(1.0f);

        verts.push_back(x0); verts.push_back(y0); verts.push_back(tx);             verts.push_back(0.0f);
        verts.push_back(x1); verts.push_back(y1); verts.push_back(tx + tw);        verts.push_back(1.0f);
        verts.push_back(x0); verts.push_back(y1); verts.push_back(tx);             verts.push_back(1.0f);

        cx += cw * 0.6f;
    }

    if (verts.empty()) return;

    glBufferSubData(GL_ARRAY_BUFFER, 0, verts.size() * sizeof(float), verts.data());
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(verts.size() / 4));
}
