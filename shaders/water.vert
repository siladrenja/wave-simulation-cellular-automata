#version 330 core
layout(location = 0) in vec2 aUv;
out vec3 vNormal;
out float vHeight;
uniform mat4 uVP;
uniform sampler2D uState;
uniform sampler2D uObstacles;
uniform vec2 uTexel;
uniform float uWorldSize;
void main() {
    float h = texture(uState, aUv).r;
    float l = texture(uState, aUv - vec2(uTexel.x, 0)).r;
    float r = texture(uState, aUv + vec2(uTexel.x, 0)).r;
    float d = texture(uState, aUv - vec2(0, uTexel.y)).r;
    float u = texture(uState, aUv + vec2(0, uTexel.y)).r;
    float cell = uWorldSize * uTexel.x;
    vNormal = normalize(vec3(l - r, 2.0 * cell, d - u));
    vHeight = h;
    vec3 world = vec3((aUv.x - 0.5) * uWorldSize, h, (aUv.y - 0.5) * uWorldSize);
    gl_Position = uVP * vec4(world, 1.0);
}
