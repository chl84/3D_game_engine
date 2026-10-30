#version 450
#extension GL_EXT_nonuniform_qualifier : require
struct Material {
    vec4 base;
    vec4 factors; // metallic, roughness, normal scale, occlusion strength
    vec4 emissive; // alpha cutoff in w
    uvec4 textures; // base, metallic-roughness, normal, occlusion
    uvec4 flags; // emissive image, alpha mask, double sided, unused
};
struct TextureInfo { uvec4 samplerData; }; // mag, min, wrapS, wrapT
layout(set=0,binding=0,std140) uniform Frame {
    vec4 direction;
    vec4 radiance;
    vec4 ambient;
} frame;
layout(set=1,binding=0,std430) readonly buffer Resources {
    Material materials[4096];
    TextureInfo textureInfo[4096];
} resources;
layout(set=1,binding=1) uniform sampler filters[2];
layout(set=1,binding=2) uniform texture2D images[];
layout(push_constant) uniform Push {
    mat4 vp; vec4 row0; vec4 row1; vec4 row2; uint material; uint encodeSrgb;
} push;
layout(location=0) in vec3 worldPosition;
layout(location=1) in vec3 worldNormal;
layout(location=2) in vec4 worldTangent;
layout(location=3) in vec2 texcoord;
layout(location=4) flat in uint materialId;
layout(location=5) flat in float windingSign;
layout(location=0) out vec4 color;
const uint NONE=0xffffffffu;
const float PI=3.141592653589793;
vec2 uvDx,uvDy;
float wrapped(float uv,uint mode,float edge) {
    if(mode==10497u) return uv;
    if(mode==33648u) uv=1.0-abs(mod(uv,2.0)-1.0);
    return clamp(uv,edge,1.0-edge);
}
vec4 levelSample(uint id, vec2 uv, uvec4 data, int level, bool linearFilter) {
    vec2 size=vec2(textureSize(sampler2D(images[nonuniformEXT(id)],filters[0]),level));
    uv=vec2(wrapped(uv.x,data.z,0.5/size.x),wrapped(uv.y,data.w,0.5/size.y));
    if(linearFilter) return textureLod(sampler2D(images[nonuniformEXT(id)],filters[1]),uv,float(level));
    return textureLod(sampler2D(images[nonuniformEXT(id)],filters[0]),uv,float(level));
}
vec4 sampleImage(uint id,vec2 uv) {
    uvec4 data=resources.textureInfo[id].samplerData;
    vec2 size=vec2(textureSize(sampler2D(images[nonuniformEXT(id)],filters[0]),0));
    vec2 dx=uvDx*size,dy=uvDy*size;
    float lod=0.5*log2(max(max(dot(dx,dx),dot(dy,dy)),1e-12));
    bool magnifying=lod<=0;
    uint filterMode=magnifying?data.x:data.y;
    bool linearFilter=filterMode==9729u||filterMode==9985u||filterMode==9987u;
    bool linearMip=filterMode==9986u||filterMode==9987u;
    int last=textureQueryLevels(sampler2D(images[nonuniformEXT(id)],filters[0]))-1;
    lod=clamp(lod,0.0,float(last));
    if(magnifying||filterMode==9728u||filterMode==9729u) return levelSample(id,uv,data,0,linearFilter);
    if(!linearMip) return levelSample(id,uv,data,int(floor(lod+0.5)),linearFilter);
    int low=int(floor(lod));
    return mix(levelSample(id,uv,data,low,linearFilter),levelSample(id,uv,data,min(low+1,last),linearFilter),fract(lod));
}
vec3 srgb(vec3 x) {
    return mix(12.92*x,1.055*pow(x,vec3(1.0/2.4))-0.055,greaterThan(x,vec3(0.0031308)));
}
void main() {
    uvDx=dFdx(texcoord); uvDy=dFdy(texcoord);
    Material m=resources.materials[materialId];
    bool frontFacing=gl_FrontFacing==(windingSign>0);
    if(!frontFacing && m.flags.z==0u) discard;
    vec4 base=m.base;
    if(m.textures.x!=NONE) base*=sampleImage(m.textures.x,texcoord);
    if(m.flags.y!=0u && base.a<m.emissive.w) discard;
    float metallic=m.factors.x,roughness=m.factors.y;
    if(m.textures.y!=NONE) { vec4 mr=sampleImage(m.textures.y,texcoord); metallic*=mr.b; roughness*=mr.g; }
    metallic=clamp(metallic,0,1); roughness=clamp(roughness,0.045,1);
    vec3 N=normalize(worldNormal);
    if(m.textures.z!=NONE) {
        vec3 T=normalize(worldTangent.xyz-N*dot(N,worldTangent.xyz));
        vec3 B=cross(N,T)*worldTangent.w;
        vec3 map=sampleImage(m.textures.z,texcoord).xyz*2-1;
        map.xy*=m.factors.z;
        if(dot(map,map)<1e-10) map=vec3(0,0,1);
        N=normalize(mat3(T,B,N)*map);
    }
    if(!frontFacing) N=-N;
    vec3 V=normalize(-worldPosition),L=normalize(-frame.direction.xyz);
    vec3 halfVector=V+L;
    vec3 H=halfVector*inversesqrt(max(dot(halfVector,halfVector),1e-12));
    float NoV=max(dot(N,V),1e-4),NoL=max(dot(N,L),0),NoH=max(dot(N,H),0),VoH=max(dot(V,H),0);
    float a=roughness*roughness,a2=a*a;
    float denom=NoH*NoH*(a2-1)+1;
    float D=a2/max(PI*denom*denom,1e-7);
    float gv=NoL*sqrt(NoV*NoV*(1-a2)+a2),gl=NoV*sqrt(NoL*NoL*(1-a2)+a2);
    float visibility=0.5/max(gv+gl,1e-5);
    vec3 F0=mix(vec3(0.04),base.rgb,metallic);
    vec3 F=F0+(1-F0)*pow(1-VoH,5);
    vec3 diffuse=(1-F)*(1-metallic)*base.rgb/PI;
    vec3 lit=(diffuse+D*visibility*F)*frame.radiance.rgb*NoL;
    float ao=1;
    if(m.textures.w!=NONE) ao=mix(1.0,sampleImage(m.textures.w,texcoord).r,m.factors.w);
    lit+=frame.ambient.rgb*base.rgb*(1-metallic)*ao;
    vec3 emissive=m.emissive.rgb;
    if(m.flags.x!=NONE) emissive*=sampleImage(m.flags.x,texcoord).rgb;
    lit+=emissive;
    lit=max(lit,vec3(0));
    vec3 mapped=lit/(1+lit); // deterministic Reinhard, then sRGB output
    color=vec4(push.encodeSrgb!=0u?srgb(mapped):mapped,1);
}
