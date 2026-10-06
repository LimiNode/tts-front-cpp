#include "silero_bundle.hpp"

#include "silero_asset_hash.hpp"
#include "tts_front/core/json.hpp"
#include "tts_front/core/utf8.hpp"

#include <array>
#include <fstream>
#include <stdexcept>

namespace tts_front::detail {
namespace {

constexpr std::array<std::string_view, 9> kRequiredAssets{"ngrams.tsv",
                                                          "embedding.f32",
                                                          "bert-vocab.tsv",
                                                          "homodict.tsv",
                                                          "exceptions.tsv",
                                                          "phrase-rules.tsv",
                                                          "stress.onnx",
                                                          "yo.onnx",
                                                          "homosolver.onnx"};

std::string read_required(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input)
        throw std::runtime_error("missing native bundle file: " + path.string());
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

const JsonValue& object_field(const JsonValue& object, std::string_view name) {
    if (object.kind != JsonValue::Kind::Object)
        throw std::runtime_error("native bundle manifest field is not an object");
    const auto found = object.object.find(std::string(name));
    if (found == object.object.end())
        throw std::runtime_error("native bundle manifest is missing field: " + std::string(name));
    return found->second;
}

std::string string_field(const JsonValue& object, std::string_view name) {
    const auto& value = object_field(object, name);
    if (value.kind != JsonValue::Kind::String)
        throw std::runtime_error("native bundle manifest field is not a string: " +
                                 std::string(name));
    return value.string;
}

} // namespace

std::filesystem::path SileroBundle::asset(std::string_view name) const {
    const auto found = asset_sha256.find(std::string(name));
    if (found == asset_sha256.end())
        throw std::runtime_error("asset is not declared by the native bundle: " +
                                 std::string(name));
    return root / found->first;
}

SileroBundle load_silero_bundle(const std::filesystem::path& root,
                                std::string_view ort_version,
                                std::size_t hash_chunk_size) {
    const auto content = read_required(root / "manifest.json");
    if (!is_valid_utf8(content))
        throw std::runtime_error("native bundle manifest is not valid UTF-8");
    const auto manifest = parse_json(content);
    if (manifest.kind != JsonValue::Kind::Object)
        throw std::runtime_error("native bundle manifest root is not an object");
    SileroBundle bundle;
    bundle.root = root;
    bundle.schema_version = string_field(manifest, "schema_version");
    bundle.bundle_version = string_field(manifest, "bundle_version");
    bundle.ort_version = string_field(manifest, "ort_version");
    bundle.source_revision = string_field(manifest, "source_revision");
    bundle.model_sha256 = string_field(manifest, "model_sha256");
    if (string_field(manifest, "record_type") != "silero_native_asset_manifest" ||
        bundle.schema_version != kSileroBundleSchemaVersion ||
        bundle.bundle_version != kSileroBundleVersion || bundle.ort_version != ort_version ||
        bundle.source_revision != kSileroBundleSourceRevision ||
        bundle.model_sha256 != kSileroBundleModelSha256) {
        throw std::runtime_error("unsupported native bundle compatibility profile");
    }
    const auto& assets = object_field(manifest, "assets");
    for (const auto name : kRequiredAssets) {
        const auto& asset = object_field(assets, name);
        const auto hash = string_field(asset, "sha256");
        if (hash.size() != 64 ||
            hash.find_first_not_of("0123456789abcdefABCDEF") != std::string::npos)
            throw std::runtime_error("invalid native bundle asset hash: " + std::string(name));
        const auto path = root / name;
        if (sha256_file_hex(path, hash_chunk_size) != hash)
            throw std::runtime_error("native bundle SHA-256 mismatch: " + std::string(name));
        bundle.asset_sha256.emplace(std::string(name), hash);
    }
    return bundle;
}

} // namespace tts_front::detail
