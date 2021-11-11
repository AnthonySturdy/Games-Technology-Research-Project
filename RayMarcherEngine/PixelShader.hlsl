cbuffer ConstantBuffer : register(b0) {
	matrix World;
	matrix View;
	matrix Projection;
	float4 vOutputColor;
}

Texture2D txDiffuse : register(t0);
Texture2D txNormal : register(t1);
Texture2D txParallax : register(t2);
SamplerState samLinear : register(s0);

#define MAX_LIGHTS 1
// Light types.
#define DIRECTIONAL_LIGHT 0
#define POINT_LIGHT 1
#define SPOT_LIGHT 2

struct _Material {
	float4  Emissive;       // 16 bytes
							//----------------------------------- (16 byte boundary)
	float4  Ambient;        // 16 bytes
							//------------------------------------(16 byte boundary)
	float4  Diffuse;        // 16 bytes
							//----------------------------------- (16 byte boundary)
	float4  Specular;       // 16 bytes
							//----------------------------------- (16 byte boundary)
	float   SpecularPower;  // 4 bytes
	bool    UseTexture;     // 4 bytes
    bool	UseNormal;		// 4 bytes
    bool	UseParallax;	// 4 bytes
							//----------------------------------- (16 byte boundary)
    float ParallaxStrength; // 4 bytes
    float3	padding;		// 12 bytes
							//----------------------------------- (16 byte boundary)
};  // Total:               // 96 bytes ( 6 * 16 )

cbuffer MaterialProperties : register(b1) {
	_Material Material;
};

struct Light {
	float4      Position;               // 16 bytes
										//----------------------------------- (16 byte boundary)
	float4      Direction;              // 16 bytes
										//----------------------------------- (16 byte boundary)
	float4      Color;                  // 16 bytes
										//----------------------------------- (16 byte boundary)
	float       SpotAngle;              // 4 bytes
	float       ConstantAttenuation;    // 4 bytes
	float       LinearAttenuation;      // 4 bytes
	float       QuadraticAttenuation;   // 4 bytes
										//----------------------------------- (16 byte boundary)
	int         LightType;              // 4 bytes
	bool        Enabled;                // 4 bytes
	int2        Padding;                // 8 bytes
										//----------------------------------- (16 byte boundary)
};  // Total:                           // 80 bytes (5 * 16)

cbuffer LightProperties : register(b2) {
	float4 EyePosition;                 // 16 bytes
										//----------------------------------- (16 byte boundary)
	float4 GlobalAmbient;               // 16 bytes
										//----------------------------------- (16 byte boundary)
	Light Lights[MAX_LIGHTS];           // 80 * 8 = 640 bytes
};

//--------------------------------------------------------------------------------------
struct PS_INPUT {
	float4 Pos : SV_POSITION;
	float4 worldPos : POSITION;
	float3 Norm : NORMAL;
	float2 Tex : TEXCOORD0;
    float3 Tan : TANGENT;
    float3 Binorm : BINORMAL;
};

float4 DoDiffuse(Light light, float3 L, float3 N) {
	float NdotL = max(0, dot(N, L));
	return light.Color * NdotL;
}

float4 DoSpecular(Light lightObject, float3 vertexToEye, float3 lightDirectionToVertex, float3 Normal) {
	float4 lightDir = float4(normalize(-lightDirectionToVertex), 1);
	vertexToEye = normalize(vertexToEye);

	float lightIntensity = saturate(dot(Normal, lightDir));
	float4 specular = float4(0, 0, 0, 0);
	if (lightIntensity > 0.0f) {
		float3  reflection = normalize(2 * lightIntensity * Normal - lightDir);
		specular = pow(saturate(dot(reflection, vertexToEye)), Material.SpecularPower); // 32 = specular power
	}

	return specular;
}

float DoAttenuation(Light light, float d) {
	return 1.0f / (light.ConstantAttenuation + light.LinearAttenuation * d + light.QuadraticAttenuation * d * d);
}

struct LightingResult {
	float4 Diffuse;
	float4 Specular;
};

LightingResult DoPointLight(Light light, float3 vertexToEye, float4 vertexPos, float3 N) {
	LightingResult result;

	float3 LightDirectionToVertex = (vertexPos - light.Position).xyz;
	float distance = length(LightDirectionToVertex);
	LightDirectionToVertex = LightDirectionToVertex / distance;

	float3 vertexToLight = (light.Position - vertexPos).xyz;
	distance = length(vertexToLight);
	vertexToLight = vertexToLight / distance;

	float attenuation = DoAttenuation(light, distance);

	result.Diffuse = DoDiffuse(light, vertexToLight, N) * attenuation;
	result.Specular = DoSpecular(light, vertexToEye, LightDirectionToVertex, N) * attenuation;

	return result;
}

LightingResult ComputeLighting(float4 vertexPos, float3 N) {
	float3 vertexToEye = normalize(EyePosition - vertexPos).xyz;

	LightingResult totalResult = { { 0, 0, 0, 0 },{ 0, 0, 0, 0 } };

	[unroll]
	for (int i = 0; i < MAX_LIGHTS; ++i) {
		LightingResult result = { { 0, 0, 0, 0 },{ 0, 0, 0, 0 } };

		if (!Lights[i].Enabled)
			continue;

		result = DoPointLight(Lights[i], vertexToEye, vertexPos, N);

		totalResult.Diffuse += result.Diffuse;
		totalResult.Specular += result.Specular;
	}

	totalResult.Diffuse = saturate(totalResult.Diffuse);
	totalResult.Specular = saturate(totalResult.Specular);

	return totalResult;
}

//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------

float4 PS(PS_INPUT IN) : SV_TARGET
{
    float3x3 tbn = float3x3(IN.Tan, IN.Binorm, IN.Norm);
	
	/***********************************************
	MARKING SCHEME: Parallax Mapping
	DESCRIPTION:	Calculate view direction in tangent space, step through depth layers along 
					view direction until heightmap sample is less than current layer depth
	REFERENCE:		https://learnopengl.com/Advanced-Lighting/Parallax-Mapping
	***********************************************/
    float3 viewDir = normalize(mul(tbn, EyePosition.xyz - IN.worldPos.xyz));
    float2 texCoords = IN.Tex;	// Will be uneffected if not using parallax map
    if (Material.UseParallax)
    {
        viewDir.y = -viewDir.y;
		
		// Calculate number of layers based on view direction
        float minLayers = 8;
        float maxLayers = 96;
        int numLayers = (int)lerp(maxLayers, minLayers, max(dot(float3(0.0f, 0.0f, 1.0f), viewDir), 0.0f));
		
		// How 'deep' each layer will be based on number of layers
        float layerDepth = 1.0f / numLayers;
		
		// Amount TexCoords will shift per layer step
        float2 p = viewDir.xy * Material.ParallaxStrength;
        float2 deltaTexCoords = p / numLayers;
		
		// Initial values
        float currentLayerDepth = 0.0f;
		float2 currentTexCoords = texCoords;
        float currentDepthMapValue = 1.0f - txParallax.Sample(samLinear, currentTexCoords).r;
		
		// Used by SampleGrad which allows texture sampling inside loop
        float2 dx = ddx(IN.Tex);
        float2 dy = ddy(IN.Tex);

		// Loop until heightmap value is less than layer depth value
        while (currentLayerDepth < currentDepthMapValue)
        {
			// Shift TexCoords along delta
            currentTexCoords -= deltaTexCoords;
			// Sample heightmap (inversed as the heightmaps I'm using are wrong way round)
            currentDepthMapValue = 1.0f - txParallax.SampleGrad(samLinear, currentTexCoords, dx, dy).r;
			// Step to next layer
            currentLayerDepth += layerDepth;
        }
		
		// Update texCoords with new parallaxed coords
        texCoords = currentTexCoords;
    }
	
	/***********************************************
	MARKING SCHEME: Normal Mapping
	DESCRIPTION: Map sampling, normal value decompression, transformation to tangent space
	***********************************************/
    float3 bumpNormal = IN.Norm; // Will be uneffected if not using normal map
    if (Material.UseNormal) {
        float4 bumpMap = txNormal.Sample(samLinear, texCoords);
        bumpMap = (bumpMap * 2.0f) - 1.0f;
        bumpNormal = normalize(mul(bumpMap.xyz, tbn));
    }
	
    LightingResult lit = ComputeLighting(IN.worldPos, bumpNormal);

	float4 texColor = { 1, 1, 1, 1 };

	float4 emissive = Material.Emissive;
	float4 ambient = Material.Ambient * GlobalAmbient;
	float4 diffuse = Material.Diffuse * lit.Diffuse;
	float4 specular = Material.Specular * lit.Specular;

	if (Material.UseTexture) {
		texColor = txDiffuse.Sample(samLinear, texCoords);
	}

	float4 finalColor = (emissive + ambient + diffuse + specular) * texColor;

	return finalColor;
}

//--------------------------------------------------------------------------------------
// PSSolid - render a solid color
//--------------------------------------------------------------------------------------
float4 PSSolid(PS_INPUT input) : SV_Target
{
	return vOutputColor;
}