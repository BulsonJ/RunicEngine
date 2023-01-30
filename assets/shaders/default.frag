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

layout(std140,set = 0, binding = 2) readonly buffer MaterialDataBuffer{
	MaterialData objects[];
} materialDataArray;

layout (set = 0, binding = 3) uniform sampler2D bindlessTextures[];

layout(std140,set = 1, binding = 0) uniform  CameraBuffer{
	mat4 viewMatrix;
	mat4 projMatrix;
	vec4 cameraPos;
} cameraData;

struct DirectionalLight{
	vec4 ambient;
	vec4 diffuse;
	vec4 specular;
	vec4 direction;
};

struct PointLight{
	vec4 ambient;
	vec4 diffuse;
	vec4 specular;
	vec4 position;

	float constant;
    float linear;
    float quadratic;
	float padding;
};

layout(std140,set = 1, binding = 1) uniform  DirLightBuffer{
	DirectionalLight light;
} lightData;

layout(std140,set = 1, binding = 2) readonly buffer  PointLightBuffer{
	PointLight lights[];
} pointLightData;


vec3 CalcDirLight(DirectionalLight light, MaterialData material, vec3 normal, vec3 viewDir)
{
    vec3 lightDir = normalize(-light.direction.xyz);
    // diffuse shading
    float diff = max(dot(normal, lightDir), 0.0);
    // specular shading
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    // combine results
    vec3 ambient  = light.ambient.rgb  * texture(bindlessTextures[(nonuniformEXT(material.textureIndex.x))], inTexCoords).rgb;
    vec3 diffuse  = light.diffuse.rgb  * diff * texture(bindlessTextures[(nonuniformEXT(material.textureIndex.x))], inTexCoords).rgb;
    vec3 specular = light.specular.rgb * spec * material.specular.rgb;
    return (ambient + diffuse + specular);
}  

vec3 CalcPointLight(PointLight light, MaterialData material, vec3 normal, vec3 fragPos, vec3 viewDir)
{
    vec3 lightDir = normalize(light.position.xyz - fragPos);
    // diffuse shading
    float diff = max(dot(normal, lightDir), 0.0);
    // specular shading
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    // attenuation
    float distance    = length(light.position.xyz - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + 
  			     light.quadratic * (distance * distance));    
    // combine results
    vec3 ambient  = light.ambient.rgb  * texture(bindlessTextures[(nonuniformEXT(material.textureIndex.x))], inTexCoords).rgb;
    vec3 diffuse  = light.diffuse.rgb  * diff * texture(bindlessTextures[(nonuniformEXT(material.textureIndex.x))], inTexCoords).rgb;
    vec3 specular = light.specular.rgb * spec * material.specular.rgb;;
    ambient  *= attenuation;
    diffuse  *= attenuation;
    specular *= attenuation;
    return (ambient + diffuse + specular);
} 

void main(void)	{
	DrawData draw = drawDataArray.objects[inDrawDataIndex];
	MaterialData matData = materialDataArray.objects[draw.materialIndex];

	int normalIndex = matData.textureIndex.y;
	int emissiveIndex = matData.textureIndex.w;

	vec3 norm;
	if (normalIndex > 0){
		norm = texture(bindlessTextures[(nonuniformEXT(normalIndex))], inTexCoords).rgb;
		norm = norm * 2.0 - 1.0;
	} else {
		norm = inNormal;
	}
	norm = normalize(inTBN * norm);
	vec3 viewDir = normalize(cameraData.cameraPos.xyz - inWorldPos);

	// phase 1: Directional lighting
    vec3 result = CalcDirLight(lightData.light, matData, norm, viewDir);
    // phase 2: Point lights
    for(int i = 0; i < 4; i++){
        result += CalcPointLight(pointLightData.lights[i],matData, norm, inWorldPos, viewDir);   
	}

	// phase 3: add emission
	vec3 emission;
	if (emissiveIndex > 0){
		emission = texture(bindlessTextures[(nonuniformEXT(emissiveIndex))], inTexCoords).rgb;
	}
	
	outFragColor = vec4(result + emission,1.0);


}