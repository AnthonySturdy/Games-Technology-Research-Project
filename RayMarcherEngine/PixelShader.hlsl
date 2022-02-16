#define OBJECT_COUNT 30

struct PS_INPUT {
    float4 Pos : SV_POSITION;
    float2 Tex : TEXCOORD0;
};

cbuffer RenderSettings : register(b0)
{
    struct RS
    {
        unsigned int maxSteps;
        float maxDist;
        float intersectionThreshold;
        float PADDING; // 16 bytes
        uint2 resolution;
        float2 PADDING1; // 32 bytes
    } renderSettings;
}

cbuffer WorldCamera : register(b1)
{
    struct WC
    {
        float fov;
        float3 position; // 16 bytes
        int cameraType;
        float3 PADDING; // 32 bytes
        matrix view; // 48 Bytes
    } camera;
}

cbuffer WorldCBuf : register(b2) {
    struct WorldObject
    {
        bool isActive;
        float3 position;    // 16 bytes
        uint objectType;
        float3 params;      // 32 bytes
    } object[OBJECT_COUNT];
}

#include "GeneratedSceneDistance.hlsli"

struct Ray {
    bool hit;
    uint hitObjectIndex;
    float3 hitPosition;
    float3 hitNormal;
    float depth;
    uint stepCount;
};


float3 CalculateNormal(float3 p) {
    const float2 offset = float2(0.001f, 0.0f);

    int index;
    float3 normal = float3(GetDistanceToScene(p + offset.xyy, index) - GetDistanceToScene(p - offset.xyy, index),
                           GetDistanceToScene(p + offset.yxy, index) - GetDistanceToScene(p - offset.yxy, index),
                           GetDistanceToScene(p + offset.yyx, index) - GetDistanceToScene(p - offset.yyx, index));

    return normalize(normal);
}

Ray RayMarch(float3 ro, float3 rd) {
    // Initialise ray
    Ray ray;
    ray.hit = false;
    ray.hitObjectIndex = 0;
    ray.hitPosition = float3(0.0f, 0.0f, 0.0f);
    ray.hitNormal = float3(0.0f, 0.0f, 0.0f);
    ray.depth = 0.0f;
    ray.stepCount = 0;
    
    // Step along ray direction
    for (; ray.stepCount < renderSettings.maxSteps; ++ray.stepCount)
    {
        int index;
        float curDist = GetDistanceToScene(ro + rd * ray.depth, index);

        // If distance less than threshold, ray has intersected
        if (curDist < renderSettings.intersectionThreshold)
        {
            ray.hit = true;
            ray.hitObjectIndex = ceil(index);
            ray.hitPosition = ro + rd * ray.depth;
            ray.hitNormal = CalculateNormal(ray.hitPosition);
                    
            return ray;
        }
        
        // Increment total depth by  distance to scene
        ray.depth += curDist;
        if (ray.depth > renderSettings.maxDist)
            break;
    }
    
    return ray;
}


float4 PS(PS_INPUT IN) : SV_TARGET {
    const float aspectRatio = renderSettings.resolution.x / (float)renderSettings.resolution.y;
    float2 uv = IN.Tex;
    uv.y = 1.0f - uv.y;     // Flip UV on Y axis
    uv = uv * 2.0f - 1.0f;  // Move UV to (-1, 1) range
    uv.x *= aspectRatio;    // Apply viewport aspect ratio
    
    float3 ro = camera.position;    // Ray origin
    float3 rd = normalize(mul(transpose(camera.view), float4(uv, tan(-camera.fov), 0.0f)).xyz); // Ray direction

    float4 finalColour = float4(rd * .5 + .5, 1.0f); // Sky color
    Ray ray = RayMarch(ro, rd);
    if (ray.hit) {
        finalColour = float4(ray.hitNormal * .5 + .5, 1.0f);
    }
    
    return finalColour;
}