#version 450

layout(location = 0) out vec3 fragColor;

layout(location = 0) in vec3 vPos;
layout(location = 1) in vec3 vNormal;
layout(location = 2) in vec3 vColour;

layout(push_constant) uniform constants
{
	vec4 data;
	mat4 render_matrix;
} PData;

void main() {
	//float tick = (PData.data.x/100)+(vPos.x*5)+(vPos.y*5);
	//float a = sin(tick)*0.1;
	//float b = sin(tick+1.0)*0.1;
	//float c = sin(tick+2.0)*0.1;
	// + vec3(a,b,c)
	gl_Position = PData.render_matrix * vec4(vPos, 1.0);
	fragColor = vColour;
}