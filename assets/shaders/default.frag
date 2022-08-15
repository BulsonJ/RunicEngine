#version 460
#extension GL_EXT_nonuniform_qualifier : enable

layout (location = 0) in vec3 inColor;
layout (location = 1) in vec2 inTexCoords;
layout (location = 2) in vec3 inNormal;
layout (location = 3) in vec3 inWorldPos;
layout (location = 4) in flat int inDrawDataIndex;

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

layout (set = 0, binding = 3) uniform sampler2D bindlessTextures[];

void main(void)	{
	DrawData draw = drawDataArray.objects[inDrawDataIndex];
	MaterialData matData = materialDataArray.objects[draw.materialIndex];
	int diffuseIndex = matData.textureIndex.x;
	int normalIndex = matData.textureIndex.y;

	// pass to shader
	vec3 sunlightDirection = lightData.direction.xyz;
	vec3 sunlightDiffuse = lightData.color.xyz;
	vec3 sunlightAmbient = lightData.ambientColor.xyz;
	vec3 materialDiffuseColour = matData.diffuse.rgb;
	float materialShininess = matData.shininess;
	vec3 materialSpecular = matData.specular;

	// Diffuse
	vec3 materialNormal;
	if (normalIndex >= 0){
		materialNormal = texture(bindlessTextures[(nonuniformEXT(normalIndex))], inTexCoords).rgb;
	} else {
		materialNormal = inNormal;

	}
	vec3 norm = normalize(materialNormal);
	vec3 lightDir = normalize(sunlightDirection);
	float diff = max(dot(norm, lightDir), 0.0);
	vec3 diffuse = sunlightDiffuse;

	vec3 materialDiffuse;
	if (diffuseIndex >= 0){
		materialDiffuse = texture(bindlessTextures[(nonuniformEXT(diffuseIndex))], inTexCoords).rgb;
	} else {
		materialDiffuse = (diff * materialDiffuseColour);
	}
	diffuse = diffuse * materialDiffuse;

	//Ambient
	vec3 ambient = sunlightAmbient * materialDiffuse;

	// Specular
	vec3 viewDir = normalize(cameraData.cameraPos.xyz - inWorldPos);
	vec3 reflectDir = reflect(-lightDir, norm);  
	float spec = pow(max(dot(viewDir, reflectDir), 0.0), materialShininess);
	vec3 specular = (materialSpecular * spec) * sunlightDiffuse;  

	vec3 result = ambient + diffuse + specular;
	outFragColor = vec4(result,1.0);
}