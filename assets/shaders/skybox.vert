#version 460

layout (location = 0) in vec3 vPosition;
layout (location = 1) in vec3 vNormal;
layout (location = 2) in vec3 vColor;
layout (location = 3) in vec2 vTexCoord;
layout (location = 4) in vec3 vTangent;

layout (location = 0) out vec3 outColor;
layout (location = 1) out vec2 outTexCoords;
layout (location = 2) out vec3 outNormal;
layout (location = 3) out vec3 outWorldPos;
layout (location = 4) out flat int outDrawDataIndex;
layout (location = 5) out vec3 outViewDir;

layout( push_constant ) uniform constants
{
	int drawDataIndex;
} pushConstants;

struct DrawData{
	int transformIndex;
	int materialIndex;
};

struct ObjectData{
	mat4 modelMatrix;
	mat4 normalMatrix;
};

struct MaterialData{
	ivec4 diffuseIndex;
};

layout(std140,set = 0, binding = 0) readonly buffer DrawDataBuffer{
	DrawData objects[];
} drawDataArray;

layout(std140,set = 0, binding = 1) readonly buffer TransformBuffer{
	ObjectData objects[];
} transformData;

layout(std140,set = 0, binding = 2) readonly buffer MaterialDataBuffer{
	MaterialData objects[];
} materialDataArray;

layout(std140,set = 1, binding = 0) uniform  CameraBuffer{
	mat4 viewMatrix;
	mat4 projMatrix;
	vec4 cameraPos;
} cameraData;

void main(void)		{
	DrawData draw = drawDataArray.objects[pushConstants.drawDataIndex];
	mat4 proj = cameraData.projMatrix;
	mat4 view = cameraData.viewMatrix;

	outColor = vColor;
	outTexCoords = vTexCoord;
	outDrawDataIndex = pushConstants.drawDataIndex;
	outNormal = mat3(transformData.objects[draw.transformIndex].normalMatrix) * vNormal;
	//outWorldPos = vec3(model * vec4(vPosition, 1.0f));

	vec3 pos = vPosition;
	mat4 invproj = inverse(cameraData.projMatrix);
	pos.xy	  *= vec2(invproj[0][0],invproj[1][1]);
	pos.z 	= -1.0f;

	outViewDir	= transpose(mat3(cameraData.viewMatrix)) * normalize(pos);
	gl_Position	= vec4(vPosition, 1.0).xyww;


}
