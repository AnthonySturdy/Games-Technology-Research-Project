cbuffer ConstantBuffer : register(b0) {
    matrix World;
    matrix View;
    matrix Projection;
    float4 vOutputColor;
}

struct VS_INPUT {
    float4 Pos : POSITION;
    float3 Norm : NORMAL;
    float2 Tex : TEXCOORD0;
    float3 Tan : TANGENT;
    float3 Binorm : BINORMAL;
};

struct PS_INPUT {
    float4 Pos : SV_POSITION;
    float4 worldPos : POSITION;
    float3 Norm : NORMAL;
    float2 Tex : TEXCOORD0;
    float3 Tan : TANGENT;
    float3 Binorm : BINORMAL;
};

PS_INPUT VS(VS_INPUT input) {
    PS_INPUT output = (PS_INPUT)0;
    output.Pos = mul(input.Pos, World);
    output.worldPos = output.Pos;
    output.Pos = mul(output.Pos, View);
    output.Pos = mul(output.Pos, Projection);
    
    output.Tex = input.Tex;
    
    output.Norm = mul(input.Norm, (float3x3)World);
    output.Norm = normalize(output.Norm);
    
    output.Tan = mul(input.Tan, (float3x3)World);
    output.Tan = normalize(output.Tan);
    
    output.Binorm = mul(input.Binorm, (float3x3)World);
    output.Binorm = normalize(output.Binorm);

    return output;
}