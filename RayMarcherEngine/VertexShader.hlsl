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
    float2 Tex : TEXCOORD0;
};

PS_INPUT VS(VS_INPUT input) {
    PS_INPUT output = (PS_INPUT)0;
    
    output.Pos = input.Pos;
    output.Tex = input.Tex;

    return output;
}