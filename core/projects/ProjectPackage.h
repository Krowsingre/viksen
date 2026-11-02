#pragma once
// Directory-based project package:
//   <Name>.<ext>/manifest.json, assets/audio/, layers/, automation/, worlds/,
//   sessions/, thumbnails/
// The extension is chosen by the application (product name is not hard-coded here).
// Saves are atomic (write temp + rename) and keep the previous manifest as a backup.

#include <filesystem>
#include <string>
#include <vector>

#include "core/projects/ProjectManifest.h"

namespace atmos::core {

namespace fs = std::filesystem;

struct PackageResult {
    bool ok = false;
    std::string error;
};

struct MissingAsset {
    std::string sourceId;
    fs::path expectedPath;
};

class ProjectPackage {
public:
    static constexpr const char* kManifestName = "manifest.json";
    static constexpr const char* kBackupName = "manifest.backup.json";

    static PackageResult create(const fs::path& packageDir);
    static PackageResult save(const fs::path& packageDir, ProjectManifest& manifest);
    static PackageResult load(const fs::path& packageDir, ProjectManifest& manifest, LoadResult* details = nullptr);

    // Copies an audio file into assets/audio (never moves or modifies the original)
    // and returns the package-relative path. Name collisions get a numeric suffix.
    static PackageResult importAsset(const fs::path& packageDir, const fs::path& sourceFile, std::string& relativePathOut);

    static fs::path resolve(const fs::path& packageDir, const SourceRef& source);
    static std::vector<MissingAsset> findMissingAssets(const fs::path& packageDir, const ProjectManifest& manifest);
    // Points a source at a new location (relinking). Does not touch media files.
    static bool relink(ProjectManifest& manifest, const std::string& sourceId, const fs::path& newPath,
                       SourceStorage storage);
};

} // namespace atmos::core
