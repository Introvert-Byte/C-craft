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

    vec3 ambient = 0.35f * uLightColor;
    vec3 norm = normalize(vNormal);
    vec3 lightDir = normalize(uLightPos - vFragPos);
    float diff = max(dot(norm, lightDir), 0.0f);
    vec3 diffuse = diff * uLightColor;

    vec3 lighting = (ambient + diffuse) * texel.rgb * uBlockColor;
    FragColor = vec4(lighting, texel.a);
}
