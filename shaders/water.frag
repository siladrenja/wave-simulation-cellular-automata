#version 330 core
in vec3 vNormal;
in float vHeight;
out vec4 FragColor;
void main() {
    vec3 light = normalize(vec3(-0.4, 1.0, 0.3));
    float diffuse = 0.35 + 0.65 * max(dot(normalize(vNormal), light), 0.0);
    vec3 base = mix(vec3(0.04, 0.20, 0.34), vec3(0.10, 0.58, 0.82), clamp(vHeight * 0.35 + 0.5, 0.0, 1.0));
    FragColor = vec4(base * diffuse, 1.0);
}
