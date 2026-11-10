#pragma once

#include <string>
#include <vector>

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class FontRenderer {
public:
    FontRenderer();
    ~FontRenderer();

    FontRenderer(const FontRenderer&) = delete;
    FontRenderer& operator=(const FontRenderer&) = delete;

    bool init();
    void begin(int screenWidth, int screenHeight);
    void drawText(float x, float y, const std::string& text,
                  float scale = 1.0f,
                  const glm::vec3& color = glm::vec3(1.0f));
    void end();

private:
    GLuint m_vao = 0;
    GLuint m_vbo = 0;
    GLuint m_texture = 0;
    GLuint m_shader = 0;
    GLint m_locProj = -1;
    GLint m_locColor = -1;
    GLint m_locTex = -1;

    int m_charW = 8;
    int m_charH = 8;
    int m_atlasW = 0;
    int m_atlasH = 0;

    void buildAtlas();
    GLuint compileProgram();
};

struct UIButton {
    std::string label;
    float x, y, w, h;
    bool hovered = false;
    bool clicked = false;

    UIButton(std::string l, float x_, float y_, float w_, float h_)
        : label(std::move(l)), x(x_), y(y_), w(w_), h(h_) {}

    [[nodiscard]] bool contains(float mx, float my) const {
        return mx >= x && mx <= x + w && my >= y && my <= y + h;
    }
};
