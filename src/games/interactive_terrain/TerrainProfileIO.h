#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <algorithm>
#include <fstream>
#include <filesystem>
#include <string>
#include <vector>
#include <windows.h>

#include <nlohmann/json.hpp>

#include "TerrainTypes.h"

using json = nlohmann::json;

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(TerrainParams,
    octaves, gain, lacunarity, weightedStrength, featureScale,
    ridgedWeight, heightScale, warpEnabled, warpAmplitude, warpFeatureScale,
    warpRecursiveAmplitude, ridgedMultiWeight,
    billowWeight, billowFeatureScale
)

inline std::filesystem::path GetApplicationDirectory() {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    return std::filesystem::path(path).parent_path();
}

inline std::filesystem::path GetTerrainProfilesDirectory() {
    return GetApplicationDirectory() / L"profiles";
}

inline std::vector<std::string> ListTerrainProfiles() {
    std::vector<std::string> files;
    const auto profileDir = GetTerrainProfilesDirectory();
    if (!std::filesystem::exists(profileDir)) {
        return files;
    }

    for (const auto& entry : std::filesystem::directory_iterator(profileDir)) {
        if (entry.path().extension() == ".json") {
            files.push_back(entry.path().filename().string());
        }
    }

    std::sort(files.begin(), files.end());
    return files;
}

inline bool SaveTerrainProfile(const std::string& filename, const TerrainParams& params) {
    const auto profileDir = GetTerrainProfilesDirectory();
    std::filesystem::create_directories(profileDir);

    std::filesystem::path path = profileDir / filename;
    if (path.extension() != ".json") {
        path += ".json";
    }

    std::ofstream file(path);
    if (!file.is_open()) return false;

    json j = params;
    file << j.dump(4);
    return true;
}

inline bool LoadTerrainProfile(const std::string& filename, TerrainParams& outParams) {
    std::filesystem::path path = GetTerrainProfilesDirectory() / filename;
    if (path.extension() != ".json") {
        path += ".json";
    }

    std::ifstream file(path);
    if (!file.is_open()) return false;

    try {
        json j;
        file >> j;
        outParams = j.get<TerrainParams>();
        return true;
    } catch (...) {
        return false;
    }
}
