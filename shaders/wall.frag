#version 330 core
in float vShade;
out vec4 FragColor;
void main() { FragColor = vec4(vec3(0.46, 0.43, 0.37) * vShade, 1.0); }
