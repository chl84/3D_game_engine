#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;

layout(push_constant) uniform PushConstants {
    mat4 viewProjection;
    mat4 model;
} push;

layout(location = 0) out vec3 fragmentColor;

void main() {
    gl_Position = push.viewProjection * push.model * vec4(inPosition, 1.0);
    fragmentColor = inColor;
}
