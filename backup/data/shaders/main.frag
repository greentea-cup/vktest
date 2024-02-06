#version 420

layout(location = 0) in vec3 fragColor;

layout(location = 0) out vec4 outColor;

// uniform ivec2 screen;

void main() {
    outColor = vec4(fragColor, 1.0);
    // ivec2 screen = ivec2(800, 600);
    // vec2 uv = gl_FragCoord.xy / screen;
    // outColor = vec4(uv, 1., 1.);
}
