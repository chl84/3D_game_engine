#version 450
layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
layout(location=2) in vec4 tangent;
layout(location=3) in vec2 uv;
layout(push_constant) uniform Push {
    mat4 vp;
    vec4 row0;
    vec4 row1;
    vec4 row2;
    uint material;
    uint encodeSrgb;
} push;
layout(location=0) out vec3 worldPosition;
layout(location=1) out vec3 worldNormal;
layout(location=2) out vec4 worldTangent;
layout(location=3) out vec2 texcoord;
layout(location=4) flat out uint materialId;
layout(location=5) flat out float windingSign;
void main() {
    vec4 p=vec4(position,1);
    worldPosition=vec3(dot(push.row0,p),dot(push.row1,p),dot(push.row2,p));
    mat3 linear=transpose(mat3(push.row0.xyz,push.row1.xyz,push.row2.xyz));
    // Remove uniform scale before inverse/determinant to preserve tiny objects.
    vec3 maximum=max(max(abs(linear[0]),abs(linear[1])),abs(linear[2]));
    linear/=max(max(maximum.x,maximum.y),maximum.z);
    windingSign=sign(determinant(linear));
    worldNormal=normalize(transpose(inverse(linear))*normal);
    worldTangent=vec4(normalize(linear*tangent.xyz),tangent.w*sign(determinant(linear)));
    texcoord=uv; materialId=push.material;
    gl_Position=push.vp*vec4(worldPosition,1);
}
