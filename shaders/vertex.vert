#version 460

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragUV;

layout(location = 0) in vec3 vPos;
layout(location = 1) in vec3 vNormal;
layout(location = 2) in vec3 vColour;
layout(location = 3) in vec2 vUV;

layout(set = 0, binding = 0) uniform CameraBuffer{
	mat4 view;
	mat4 proj;
	mat4 viewproj;
} cameraData;

struct ObjectData{
	mat4 model;
};

layout(std140,set = 1, binding = 0) readonly buffer ObjectBuffer{
	ObjectData objects[];
} objectBuffer;


layout(push_constant) uniform constants{ //this is unused now and we are now using the storage buffer
	vec4 data;
	mat4 render_matrix;
} PData;

void main() {
	//float tick = (PData.data.x/100)+(vPos.x*5)+(vPos.y*5);
	//float a = sin(tick)*0.1;
	//float b = sin(tick+1.0)*0.1;
	//float c = sin(tick+2.0)*0.1;
	// + vec3(a,b,c)

	mat4 modelMatrix = objectBuffer.objects[gl_BaseInstance].model;
	mat4 transformMat = (cameraData.viewproj * modelMatrix/*PData.render_matrix*/); 
	gl_Position = transformMat * vec4(vPos, 1.0);
	fragColor = vColour;
	fragUV = vUV;
}