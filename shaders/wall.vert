#version 330 core
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec2 aInstance;
out float vShade;
uniform mat4 uVP;
uniform float uCellSize;
uniform float uWallHeight;
void main() {
    vec3 p = vec3(aInstance.x + aPosition.x * uCellSize,
                  aPosition.y * uWallHeight - 0.12,
                  aInstance.y + aPosition.z * uCellSize);
    vShade = 0.65 + 0.35 * aPosition.y;
    gl_Position = uVP * vec4(p, 1.0);
}
