#include "NoiseGenerator.h"
#include <iostream>

bool NoiseGenerator::Initialize(const TerrainParams& params) {
    m_params = params;

    // Node Graph: OpenSimplex2F -> FractalFBm + FractalRidged -> Add -> Remap
    //
    //   Simplex (= OpenSimplex2F)
    //       |
    //       +---> FractalFBm  (base terrain: smooth plains + hills)
    //       |
    //       +---> FractalRidged (mountain ridges)
    //                |
    //                v
    //       FractalFBm --+--> Add (blend) --> Remap (0..1) --> DomainScale
    //       FractalRidged+

    // 1. Base noise: OpenSimplex2F
    auto simplex = FastNoise::New<FastNoise::Simplex>();
    if (!simplex) {
        std::cerr << "[NoiseGenerator] Falha ao criar no Simplex\n";
        return false;
    }
    simplex->SetScale(m_params.featureScale);

    // 2. FractalFBm — smooth terrain base
    auto fbm = FastNoise::New<FastNoise::FractalFBm>();
    if (!fbm) {
        std::cerr << "[NoiseGenerator] Falha ao criar no FractalFBm\n";
        return false;
    }
    fbm->SetSource(simplex);
    fbm->SetOctaveCount(m_params.octaves);
    fbm->SetGain(m_params.gain);
    fbm->SetLacunarity(m_params.lacunarity);
    fbm->SetWeightedStrength(m_params.weightedStrength);

    // 3. FractalRidged — sharp mountain ridges
    auto ridged = FastNoise::New<FastNoise::FractalRidged>();
    ridged->SetSource(simplex);
    ridged->SetOctaveCount(m_params.octaves);
    ridged->SetGain(m_params.gain);
    ridged->SetLacunarity(m_params.lacunarity);
    ridged->SetWeightedStrength(m_params.weightedStrength);

    // 3.5 Billow effect (using Abs modifier + FractalFBm)
    auto billowSimplex = FastNoise::New<FastNoise::Simplex>();
    billowSimplex->SetScale(m_params.billowFeatureScale);
    
    auto absModifier = FastNoise::New<FastNoise::Abs>();
    absModifier->SetSource(billowSimplex);

    auto billow = FastNoise::New<FastNoise::FractalFBm>();
    billow->SetSource(absModifier);
    billow->SetOctaveCount(m_params.octaves);
    billow->SetGain(m_params.gain);
    billow->SetLacunarity(m_params.lacunarity);
    billow->SetWeightedStrength(m_params.weightedStrength);

    // 4. Blend: Add FBm + Ridged*weight + Billow*weight
    // AND Multi-Fractal Blend: FBm * (1 + Ridged*multiWeight)
    auto ridgedScaled = FastNoise::New<FastNoise::Multiply>();
    ridgedScaled->SetLHS(ridged);
    ridgedScaled->SetRHS(m_params.ridgedWeight);

    auto billowScaled = FastNoise::New<FastNoise::Multiply>();
    billowScaled->SetLHS(billow);
    billowScaled->SetRHS(m_params.billowWeight);

    auto baseCombine = FastNoise::New<FastNoise::Add>();
    baseCombine->SetLHS(fbm);
    baseCombine->SetRHS(ridgedScaled);

    auto combined = FastNoise::New<FastNoise::Add>();
    combined->SetLHS(baseCombine);
    combined->SetRHS(billowScaled);

    // Apply Multi-Fractal effect if weight > 0
    FastNoise::SmartNode<> blendedNode = combined;
    if (m_params.ridgedMultiWeight > 0.001f) {
        auto multi = FastNoise::New<FastNoise::Multiply>();
        
        // Scale the ridges by the multi-multiplier
        auto scaledRidges = FastNoise::New<FastNoise::Multiply>();
        scaledRidges->SetLHS(ridgedScaled); // This is ridged * ridgedWeight
        scaledRidges->SetRHS(m_params.ridgedMultiWeight);

        // We use (1 + Ridged * multiWeight) to scale the FBm
        auto one = FastNoise::New<FastNoise::Add>();
        one->SetLHS(scaledRidges);
        one->SetRHS(1.0f);
        
        multi->SetLHS(fbm);
        multi->SetRHS(one);
        blendedNode = multi;
    }

    // 5. Domain Warping (Advanced Distortions)
    FastNoise::SmartNode<> rootNode = blendedNode;
    if (m_params.warpEnabled) {
        // Level 1: Normal Warp
        auto warp1 = FastNoise::New<FastNoise::DomainWarpGradient>();
        warp1->SetSource(blendedNode);
        warp1->SetWarpAmplitude(m_params.warpAmplitude);
        warp1->SetScale(m_params.warpFeatureScale);
        
        rootNode = warp1;

        // Level 2: Recursive Warp (Warping the already warped noise)
        if (m_params.warpRecursiveAmplitude > 0.01f) {
            auto warp2 = FastNoise::New<FastNoise::DomainWarpGradient>();
            warp2->SetSource(warp1);
            warp2->SetWarpAmplitude(m_params.warpRecursiveAmplitude);
            warp2->SetScale(m_params.warpFeatureScale * 0.5f); // Different frequency for variety
            rootNode = warp2;
        }
    }

    // 6. Remap from [-1,1] to [0,1] so heights are positive
    auto remap = FastNoise::New<FastNoise::Remap>();
    if (!remap) {
        std::cerr << "[NoiseGenerator] Falha ao criar no Remap\n";
        return false;
    }
    remap->SetSource(rootNode);
    remap->SetFromMin(-1.0f);
    remap->SetFromMax(1.0f);
    remap->SetToMin(0.0f);
    remap->SetToMax(1.0f);

    m_generator = remap;

    std::cout << "[NoiseGenerator] Node Graph criado: Simplex -> FBm + Ridged*" << m_params.ridgedWeight
              << " -> Add -> Remap[0,1] (AVX2 auto-detect)\n";
    return true;
}

void NoiseGenerator::GenerateHeightmap(
    std::vector<float>& outHeights,
    int width, int height,
    float startX, float startZ,
    float stepSize,
    const GenerationContext& ctx) const
{
    outHeights.resize(static_cast<size_t>(width) * height);

    // GenUniformGrid2D fills outHeights with values in [0,1] (after Remap)
    // Output layout: outHeights[y * width + x]
    m_generator->GenUniformGrid2D(
        outHeights.data(),
        startX, startZ,       // world-space start position
        width, height,        // sample count
        stepSize, stepSize,   // distance between samples
        static_cast<int>(ctx.worldSeed)
    );

    // Scale to world-space height
    for (auto& h : outHeights) {
        h *= m_params.heightScale;
    }
}
