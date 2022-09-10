#version 450

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragUV;

layout (set = 0, binding = 1) uniform WorldData{
	vec4 fogColor;
	vec4 fogDistances;
	vec4 ambientColor;
	vec4 sunlightDirection;
	vec4 sunlightColor;
} sceneData; //same here this is all

layout (set = 2, binding = 0) uniform sampler2D tex1;

void main() {
	//outColor = vec4(fragColor + sceneData.ambientColor.xyz,1.0); //usually ambient isnt here
	vec4 col = texture(tex1, fragUV);
	outColor = vec4(col.z,col.y,col.x,col.w);
}