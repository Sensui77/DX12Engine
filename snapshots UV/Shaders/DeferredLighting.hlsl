Texture2D tAlbedo : register(t0);
Texture2D tNormal : register(t1);
SamplerState sPoint : register(s0);

struct PSInput {
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

PSInput VSMain(uint vertexID : SV_VertexID) {
    PSInput result;
    // Produz Triângulo Único (Big Triangle) 
    // ID 0: (0, 0)
    // ID 1: (2, 0)
    // ID 2: (0, 2)
    result.uv = float2((vertexID << 1) & 2, vertexID & 2);
    result.position = float4(result.uv * float2(2, -2) + float2(-1, 1), 0, 1);
    return result;
}

float4 PSMain(PSInput input) : SV_TARGET {
    float4 albedo = tAlbedo.Sample(sPoint, input.uv);
    float4 normalMap = tNormal.Sample(sPoint, input.uv);
    
    // Ignoramos a cor de fundo (Zero Normal flag do clear de G-Buffer)
    if (length(normalMap.xyz) < 0.1f) {
        return float4(0.53f, 0.81f, 0.92f, 1.0f); // Cor simples de Ceu de fundo SkyBlue
    }

    float3 N = normalize(normalMap.xyz);
    
    // 1. Luz Direcional Principal (Sol)
    float3 lightDir = normalize(float3(0.5f, 1.0f, -0.5f));
    float3 sunColor = float3(1.0f, 0.95f, 0.8f); // Amarelo sol suave
    float ndotl = max(dot(N, lightDir), 0.0f);
    float3 directDiffuse = albedo.rgb * sunColor * ndotl;

    // 2. Luz Ambiente Hemitropica (Hemispherical Ambient)
    float3 skyColor = float3(0.53f, 0.81f, 0.92f); // Azul claro do ceu
    float3 groundColor = float3(0.15f, 0.1f, 0.05f); // Marrom terra pro rebote
    
    // Mapeia Normal.y (-1 para 1) em um range (0 para 1) de lerp linear
    float skyFactor = (N.y + 1.0f) * 0.5f;
    float3 ambientDiffuse = lerp(groundColor, skyColor, skyFactor) * albedo.rgb * 0.4f; // 0.4f Intensity
    
    // Soma a iluminacao local com o rebatimento ambiente
    float3 finalColor = directDiffuse + ambientDiffuse;
    
    return float4(finalColor, 1.0f);
}
