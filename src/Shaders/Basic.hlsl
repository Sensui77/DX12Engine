struct PSInput {
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float3 normal : NORMAL;
};

cbuffer CameraData : register(b0) {
    float4x4 ViewProj;
};

PSInput VSMain(float3 position : POSITION, float4 color : COLOR, float3 normal : NORMAL) {
    PSInput result;
    
    // Multiplica pela matriz de visão projetada (Handedness Left-Handed por padrão)
    result.position = mul(float4(position, 1.0f), ViewProj);
    
    result.color = color;
    result.normal = normal;
    return result;
}

struct PSOutput {
    float4 Albedo : SV_Target0;
    float4 Normal : SV_Target1;
};

PSOutput PSMain(PSInput input) {
    PSOutput result;
    result.Albedo = input.color;
    
    // Normal map vindo interpolado do VertexShader (gerado pela FastNoise na CPU)
    result.Normal = float4(input.normal, 1.0f);
    
    return result;
}
