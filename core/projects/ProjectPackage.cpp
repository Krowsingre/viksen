#include "core/projects/ProjectPackage.h"

#include <fstream>
#include <sstream>
#include <system_error>

#include "core/serialization/Ids.h"

namespace atmos::core {

namespace {
const char* kSubdirs[] = {"assets/audio", "layers", "automation", "worlds", "sessions", "thumbnails"};

PackageResult fail(std::string msg) { return {false, std::move(msg)}; }

PackageResult writeFileAtomic(const fs::path& target, const std::string& contents) {
    const fs::path tmp = target.parent_path() / (target.filename().string() + ".tmp");
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f) return fail("cannot write " + tmp.string());
        f << contents;
        f.flush();
        if (!f) return fail("write failed for " + tmp.string());
    }
    std::error_code ec;
    fs::rename(tmp, target, ec); // atomic replace on POSIX; MoveFileEx semantics on Windows
    if (ec) {
        fs::remove(target, ec);
        fs::rename(tmp, target, ec);
        if (ec) return fail("cannot replace " + target.string() + ": " + ec.message());
    }
    return {true, {}};
}
} // namespace

PackageResult ProjectPackage::create(const fs::path& dir) {
    std::error_code ec;
    for (const char* sub : kSubdirs) {
        fs::create_directories(dir / sub, ec);
        if (ec) return fail("cannot create " + (dir / sub).string() + ": " + ec.message());
    }
    return {true, {}};
}

PackageResult ProjectPackage::save(const fs::path& dir, ProjectManifest& m) {
    if (auto r = create(dir); !r.ok) return r;
    for (const auto& issue : validate(m))
        if (issue.severity == ValidationIssue::Severity::Error) return fail("invalid project: " + issue.message);

    m.modifiedAt = utcTimestampNow();
    if (m.createdAt.empty()) m.createdAt = m.modifiedAt;

    const fs::path manifestPath = dir / kManifestName;
    std::error_code ec;
    if (fs::exists(manifestPath, ec)) fs::copy_file(manifestPath, dir / kBackupName, fs::copy_options::overwrite_existing, ec);
    return writeFileAtomic(manifestPath, manifestToJson(m).dump(2) + "\n");
}

PackageResult ProjectPackage::load(const fs::path& dir, ProjectManifest& m, LoadResult* details) {
    auto tryLoad = [&](const fs::path& p, LoadResult& lr) -> PackageResult {
        std::ifstream f(p, std::ios::binary);
        if (!f) return fail("cannot open " + p.string());
        std::stringstream ss;
        ss << f.rdbuf();
        const Json j = Json::parse(ss.str(), nullptr, /*allow_exceptions=*/false);
        if (j.is_discarded()) return fail("manifest is not valid JSON: " + p.string());
        lr = manifestFromJson(j, m);
        if (!lr.ok) return fail(lr.error);
        return {true, {}};
    };
    LoadResult lr;
    PackageResult r = tryLoad(dir / kManifestName, lr);
    if (!r.ok && fs::exists(dir / kBackupName)) {
        // Crash recovery: fall back to the last good manifest.
        LoadResult backup;
        if (tryLoad(dir / kBackupName, backup).ok) {
            lr = backup;
            r = {true, "primary manifest unreadable (" + r.error + "); recovered from backup"};
        }
    }
    if (details) *details = lr;
    return r;
}

PackageResult ProjectPackage::importAsset(const fs::path& dir, const fs::path& src, std::string& rel) {
    std::error_code ec;
    if (!fs::is_regular_file(src, ec)) return fail("not a file: " + src.string());
    if (auto r = create(dir); !r.ok) return r;
    const fs::path audioDir = dir / "assets" / "audio";
    fs::path name = src.filename();
    fs::path dest = audioDir / name;
    for (int n = 1; fs::exists(dest, ec); ++n)
        dest = audioDir / (src.stem().string() + "-" + std::to_string(n) + src.extension().string());
    fs::copy_file(src, dest, fs::copy_options::none, ec);
    if (ec) return fail("copy failed: " + ec.message());
    rel = fs::relative(dest, dir, ec).generic_string();
    return {true, {}};
}

fs::path ProjectPackage::resolve(const fs::path& dir, const SourceRef& s) {
    if (s.storage == SourceStorage::External) return fs::path(s.path);
    return dir / fs::path(s.path);
}

std::vector<MissingAsset> ProjectPackage::findMissingAssets(const fs::path& dir, const ProjectManifest& m) {
    std::vector<MissingAsset> missing;
    std::error_code ec;
    for (const auto& s : m.sources) {
        const fs::path p = resolve(dir, s);
        if (!fs::is_regular_file(p, ec)) missing.push_back({s.id, p});
    }
    return missing;
}

bool ProjectPackage::relink(ProjectManifest& m, const std::string& id, const fs::path& p, SourceStorage storage) {
    for (auto& s : m.sources) {
        if (s.id != id) continue;
        s.storage = storage;
        s.path = p.generic_string();
        return true;
    }
    return false;
}

} // namespace atmos::core
