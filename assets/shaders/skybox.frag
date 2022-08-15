#version 460
#extension GL_EXT_nonuniform_qualifier : enable

layout (location = 0) in vec3 inColor;
layout (location = 1) in vec2 inTexCoords;
layout (location = 2) in vec3 inNormal;
layout (location = 3) in vec3 inWorldPos;
layout (location = 4) in flat int inDrawDataIndex;
layout (location = 5) in vec3 inViewDir;

layout (location = 0) out vec4 outFragColor;

struct DrawData{
	int transformIndex;
	int materialIndex;
};

struct MaterialData{
	vec4 diffuse;
	vec3 specular;
	float shininess;
	ivec4 textureIndex;
};

layout(std140,set = 0, binding = 0) readonly buffer DrawDataBuffer{
	DrawData objects[];
} drawDataArray;

layout(std140,set = 1, binding = 0) uniform  CameraBuffer{
	mat4 viewMatrix;
	mat4 projMatrix;
	vec4 cameraPos;
} cameraData;

layout(std140,set = 1, binding = 1) uniform  DirLightBuffer{
	vec4 direction;
	vec4 color;
	vec4 ambientColor;
} lightData;


layout(std140,set = 0, binding = 2) readonly buffer MaterialDataBuffer{
	MaterialData objects[];
} materialDataArray;

//layout (set = 0, binding = 3) uniform sampler2D bindlessTextures[];
layout (set = 0, binding = 3) uniform samplerCube bindlessTextures[];

void main(void)	{
	DrawData draw = drawDataArray.objects[inDrawDataIndex];
	MaterialData matData = materialDataArray.objects[draw.materialIndex];
	int diffuseIndex = matData.textureIndex.x;

	vec4 samp = texture(bindlessTextures[nonuniformEXT(diffuseIndex)],normalize(inViewDir));
	
	outFragColor = vec4(samp.rgb,1.0);
}