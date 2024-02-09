#version 420

layout(binding = 1) uniform sampler2D tx;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

void main() {
    vec4 txColor = texture(tx, fragTexCoord);
    if (txColor.a < 0.1) discard;
    outColor = txColor;
    // outColor = vec4(fragColor, 1.);
    // outColor = vec4(fragTexCoord, 0., 1.);
}
