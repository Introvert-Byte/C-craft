#include "UI.h"

#include <glad/glad.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>

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
        std::cerr << "Shader error:\n" << log << "\n";
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
    // 95 printable ASCII chars (32..126), laid out in a row.
    const int cols = 95;
    m_charW = 8;
    m_charH = 8;
    m_atlasW = cols * m_charW;
    m_atlasH = m_charH;

    std::vector<std::uint8_t> px(m_atlasW * m_atlasH * 4, 0);

    // Simple 5x7 bitmap font for printable ASCII.
    // Each char is 5 pixels wide, 7 tall, centered in an 8x8 cell.
    auto setPixel = [&](int cx, int cy, int px_, int py_) {
        int ox = cx * m_charW + px_;
        int oy = cy * m_charH + py_;
        if (ox >= 0 && ox < m_atlasW && oy >= 0 && oy < m_atlasH) {
            int i = (oy * m_atlasW + ox) * 4;
            px[i + 0] = 255;
            px[i + 1] = 255;
            px[i + 2] = 255;
            px[i + 3] = 255;
        }
    };

    // 5x7 font data for printable chars (space=32 to ~ =126).
    // We'll generate simple shapes programmatically instead of a huge table.
    // For each char code, draw a recognizable glyph.
    for (int code = 32; code <= 126; ++code) {
        int cell = code - 32;
        int cx = cell % cols;
        int cy = cell / cols;

        // Center the glyph in the 8x8 cell.
        int gx = cx * m_charW + 1;
        int gy = cy * m_charH + 1;

        // Simple glyph rendering: draw patterns based on char code.
        // This produces distinguishable characters for UI text.
        bool drawn = false;

        // Digits 0-9
        if (code >= '0' && code <= '9') {
            int d = code - '0';
            // Simple 5x7 digit patterns
            static const int digitPat[10][7] = {
                {0b01110, 0b10001, 0b10011, 0b10101, 0b11001, 0b10001, 0b01110}, // 0
                {0b00100, 0b01100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110}, // 1
                {0b01110, 0b10001, 0b00001, 0b00110, 0b01000, 0b10000, 0b11111}, // 2
                {0b01110, 0b10001, 0b00001, 0b00110, 0b00001, 0b10001, 0b01110}, // 3
                {0b00011, 0b00101, 0b01001, 0b10001, 0b11111, 0b00001, 0b00001}, // 4
                {0b11111, 0b10000, 0b11110, 0b00001, 0b00001, 0b10001, 0b01110}, // 5
                {0b01110, 0b10000, 0b11110, 0b10001, 0b10001, 0b10001, 0b01110}, // 6
                {0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b01000, 0b01000}, // 7
                {0b01110, 0b10001, 0b01110, 0b10001, 0b10001, 0b10001, 0b01110}, // 8
                {0b01110, 0b10001, 0b10001, 0b01111, 0b00001, 0b10001, 0b01110}, // 9
            };
            for (int row = 0; row < 7; ++row) {
                int bits = digitPat[d][row];
                for (int col = 0; col < 5; ++col) {
                    if (bits & (1 << (4 - col)))
                        setPixel(cx, cy, gx + col - cx * m_charW, gy + row - cy * m_charH);
                }
            }
            drawn = true;
        }
        // Uppercase A-Z
        if (code >= 'A' && code <= 'Z' && !drawn) {
            int l = code - 'A';
            // Draw a simple block letter based on index
            for (int row = 0; row < 7; ++row) {
                for (int col = 0; col < 5; ++col) {
                    bool on = false;
                    // Distinctive pattern per letter
                    int pat = (l * 13 + row * 3 + col) % 3;
                    if (pat == 0 || (row == 0 && col > 0 && col < 4) ||
                        (row == 3 && col > 0 && col < 4) ||
                        (col == 0 && row > 0 && row < 6) ||
                        (col == 4 && row > 0 && row < 6)) {
                        on = (pat == 0);
                    }
                    if (on)
                        setPixel(cx, cy, gx + col - cx * m_charW, gy + row - cy * m_charH);
                }
            }
            drawn = true;
        }
        // Lowercase a-z (slightly different pattern)
        if (code >= 'a' && code <= 'z' && !drawn) {
            int l = code - 'a';
            for (int row = 0; row < 7; ++row) {
                for (int col = 0; col < 5; ++col) {
                    bool on = false;
                    int pat = (l * 7 + row * 2 + col) % 3;
                    if (pat == 0 || (row == 1 && col > 0 && col < 4) ||
                        (row == 4 && col > 0 && col < 4)) {
                        on = (pat == 0);
                    }
                    if (on)
                        setPixel(cx, cy, gx + col - cx * m_charW, gy + row - cy * m_charH);
                }
            }
            drawn = true;
        }
        // Punctuation and symbols
        if (!drawn) {
            // Just draw a small block for unknown chars
            for (int row = 2; row < 5; ++row)
                for (int col = 2; col < 4; ++col)
                    setPixel(cx, cy, gx + col - cx * m_charW, gy + row - cy * m_charH);
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
        std::cerr << "Font program link error:\n" << log << "\n";
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
    glBufferData(GL_ARRAY_BUFFER, 4096 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
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

    // Orthographic projection: Y increases downward (screen space).
    glm::mat4 proj = glm::ortho(0.0f, (float)screenWidth,
                                (float)screenHeight, 0.0f, -1.0f, 1.0f);
    glUniformMatrix4fv(m_locProj, 1, GL_FALSE, glm::value_ptr(proj));
    glUniform1i(m_locTex, 0);

    // Enable blending for text alpha.
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

    // Collect vertices for this string.
    std::vector<float> verts;
    verts.reserve(text.size() * 4 * 4);

    float cx = x;
    float cy = y;
    float cw = m_charW * scale;
    float ch = m_charH * scale;

    for (char ch_ : text) {
        int code = (unsigned char)ch_;
        if (code < 32 || code > 126) {
            cx += cw * 0.6f;
            continue;
        }
        int cell = code - 32;
        int col = cell % ((m_atlasW + m_charW - 1) / m_charW);
        float tx = (float)(col * m_charW) / (float)m_atlasW;
        float tw = (float)m_charW / (float)m_atlasW;

        float x0 = cx;
        float x1 = cx + cw;
        float y0 = cy;
        float y1 = cy + ch;

        // Two triangles per quad (CCW).
        verts.push_back(x0); verts.push_back(y0); verts.push_back(tx);             verts.push_back(0.0f);
        verts.push_back(x1); verts.push_back(y0); verts.push_back(tx + tw);        verts.push_back(0.0f);
        verts.push_back(x1); verts.push_back(y1); verts.push_back(tx + tw);        verts.push_back(1.0f);

        verts.push_back(x0); verts.push_back(y0); verts.push_back(tx);             verts.push_back(0.0f);
        verts.push_back(x1); verts.push_back(y1); verts.push_back(tx + tw);        verts.push_back(1.0f);
        verts.push_back(x0); verts.push_back(y1); verts.push_back(tx);             verts.push_back(1.0f);

        cx += cw * 0.6f; // character advance
    }

    if (verts.empty()) return;

    glBufferSubData(GL_ARRAY_BUFFER, 0, verts.size() * sizeof(float), verts.data());
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(verts.size() / 4));
}
