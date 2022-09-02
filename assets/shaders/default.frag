#version 460
#extension GL_EXT_nonuniform_qualifier : enable

layout (location = 0) in vec3 inColor;
layout (location = 1) in vec2 inTexCoords;
layout (location = 2) in vec3 inNormal;
layout (location = 3) in vec3 inWorldPos;
layout (location = 4) in flat int inDrawDataIndex;
layout (location = 5) in mat3 inTBN;

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
	vec4 ambient;
	vec4 diffuse;
	vec4 specular;
	vec4 direction;
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
	int roughnessIndex = matData.textureIndex.z;
	int emissiveIndex = matData.textureIndex.w;

	// pass to shader
	vec3 sunlightDirection = lightData.direction.xyz;
	vec3 sunlightDiffuse = lightData.diffuse.rgb;
	vec3 sunlightAmbient = lightData.ambient.rgb;
	vec3 materialDiffuseColour = matData.diffuse.rgb;
	float materialShininess = matData.shininess;
	vec3 materialSpecular = matData.specular;

	//if (roughnessIndex > 0){
	//	materialSpecular = texture(bindlessTextures[(nonuniformEXT(roughnessIndex))], inTexCoords).rgb;
	//}

	// Diffuse
	vec3 materialNormal;
	if (normalIndex > 0){
		materialNormal = texture(bindlessTextures[(nonuniformEXT(normalIndex))], inTexCoords).rgb;
		materialNormal = materialNormal * 2.0 - 1.0;
	} else {
		materialNormal = inNormal;
	}
	vec3 norm = normalize(inTBN * materialNormal);
	vec3 lightDir = normalize(sunlightDirection);
	float diff = max(dot(norm, lightDir), 0.0);
	vec3 diffuse = sunlightDiffuse;

	vec3 materialDiffuse;
	if (diffuseIndex > 0){
		vec4 materialDiffuseTex = texture(bindlessTextures[(nonuniformEXT(diffuseIndex))], inTexCoords);
		if (materialDiffuseTex.a == 0){
			discard;
		}
		materialDiffuse = materialDiffuseTex.rgb;
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
	vec3 specular = lightData.specular.rgb * spec * materialSpecular;  

	vec3 emission;
	if (emissiveIndex > 0){
		emission = texture(bindlessTextures[(nonuniformEXT(emissiveIndex))], inTexCoords).rgb;
	}

	vec3 result = ambient + diffuse + specular + emission;
	outFragColor = vec4(result,1.0);
}