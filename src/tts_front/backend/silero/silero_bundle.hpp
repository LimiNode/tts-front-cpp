#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>

namespace tts_front::detail {

/// The pinned compatibility profile used by the private Silero backend.
inline constexpr std::string_view kSileroBundleSchemaVersion = "1";
inline constexpr std::string_view kSileroBundleVersion = "silero-native-phase1-v1";
inline constexpr std::string_view kSileroBundleOrtVersion = "1.30.0";
inline constexpr std::string_view kSileroBundleSourceRevision =
    "d38096cae9bf3ac846bbb88705428f3ed3801b96";
inline constexpr std::string_view kSileroBundleModelSha256 =
    "aecb207df9db34a079de2ba91edba2a9333839ad24de0cf17e7bc82424876786";

/// A manifest-verified bundle.  Asset paths are intentionally resolved only
/// through asset(), so callers cannot accidentally load an undeclared file.
struct SileroBundle {
    std::filesystem::path root;
    std::string schema_version;
    std::string bundle_version;
    std::string ort_version;
    std::string source_revision;
    std::string model_sha256;
    std::unordered_map<std::string, std::string> asset_sha256;

    std::filesystem::path asset(std::string_view name) const;
};

/// Load and verify a complete native bundle using bounded-memory SHA-256.
/// Throws std::runtime_error for malformed, incompatible, missing, or corrupt
/// bundles.  The supplied ORT version is compared with the pinned manifest
/// before any graph is opened by the optional session owner.
SileroBundle load_silero_bundle(const std::filesystem::path& root,
                                std::string_view ort_version = kSileroBundleOrtVersion,
                                std::size_t hash_chunk_size = 1U << 20);

} // namespace tts_front::detail
