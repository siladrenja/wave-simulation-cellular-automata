#version 330 core
in vec2 vUv;
out vec4 FragColor;
uniform sampler2D uState;
uniform sampler2D uObstacles;
void main() {
    if (texture(uObstacles, vUv).r > 0.5) { FragColor = vec4(0.95, 0.72, 0.14, 1.0); return; }
    float h = texture(uState, vUv).r;
    vec3 neutral = vec3(0.035, 0.055, 0.085);
    vec3 positive = vec3(0.10, 0.62, 0.95);
    vec3 negative = vec3(0.84, 0.13, 0.28);
    FragColor = vec4(mix(neutral, h >= 0.0 ? positive : negative, clamp(abs(h) * 1.4, 0.0, 1.0)), 1.0);
}
