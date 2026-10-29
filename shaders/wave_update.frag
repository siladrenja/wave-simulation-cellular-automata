#version 330 core
in vec2 vUv;
layout(location = 0) out vec2 outState;
uniform sampler2D uState;
uniform sampler2D uObstacles;
uniform vec2 uTexel;
uniform float uDamping;
uniform bool uHasRipple;
uniform vec2 uRipple;
uniform float uRippleAmplitude;
uniform float uRippleRadius;

float obstacle(vec2 uv) { return texture(uObstacles, uv).r; }
float neighbor(vec2 uv, float center) {
    return obstacle(uv) > 0.5 ? center : texture(uState, uv).r;
}
void main() {
    if (obstacle(vUv) > 0.5) { outState = vec2(0.0); return; }
    vec2 old = texture(uState, vUv).rg;
    float sum = neighbor(vUv + vec2(uTexel.x, 0), old.r)
              + neighbor(vUv - vec2(uTexel.x, 0), old.r)
              + neighbor(vUv + vec2(0, uTexel.y), old.r)
              + neighbor(vUv - vec2(0, uTexel.y), old.r);
    // Courant factor 0.48: stable for the 4-neighbor 2D wave stencil.
    float next = (2.0 * old.r - old.g + 0.48 * (sum - 4.0 * old.r)) * uDamping;
    if (uHasRipple) {
        float d = distance(vUv, uRipple);
        next += uRippleAmplitude * exp(-0.5 * d * d / (uRippleRadius * uRippleRadius));
    }
    outState = vec2(next, old.r);
}
