#pragma once

struct TerrainParams {
    int   octaves = 6;
    float gain = 0.5f;
    float lacunarity = 2.0f;
    float weightedStrength = 0.0f;
    float featureScale = 100.0f;   // 1/frequency — larger = bigger features
    float ridgedWeight = 0.5f;     // blend weight for ridged fractal
    float heightScale = 30.0f;     // world-space height multiplier

    // Domain Warping (Advanced Organic Forms)
    bool  warpEnabled = false;
    float warpAmplitude = 50.0f;   // strength of the warping push
    float warpFeatureScale = 200.0f; // 1/frequency for warp

    float warpRecursiveAmplitude = 0.0f; // NEW: level 2 recursion strength
    float ridgedMultiWeight = 0.0f;      // NEW: multiplicative scale for ridges

    // Billow Noise (Bubbly / Rounded Hills)
    float billowWeight = 0.0f;           // blend weight for billow fractal
    float billowFeatureScale = 150.0f;   // independent scale for billow

    // Mathematical Shaping
    float redistributionPower = 1.0f;    // Exponent for redistribution
    float terrainContrast = 1.0f;        // Contrast multiplier
};

struct GenerationContext {
    unsigned long long worldSeed = 1337;
    int chunkX = 0;
    int chunkZ = 0;
};
