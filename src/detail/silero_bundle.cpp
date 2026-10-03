#include "silero_bundle.hpp"

#include "silero_asset_hash.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <stdexcept>

namespace tts_front::detail {
namespace {

constexpr std::array<std::string_view, 10> kRequiredAssets{"ngrams.tsv",
                                                           "embedding.f32",
                                                           "bert-vocab.tsv",
                                                           "homodict.json",
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

std::string
json_string_field(const std::string& content, std::string_view name, std::size_t search_begin = 0) {
    const auto key = content.find('"' + std::string(name) + '"', search_begin);
    if (key == std::string::npos)
        throw std::runtime_error("native bundle manifest is missing field: " + std::string(name));
    const auto colon = content.find(':', key + name.size() + 2);
    const auto begin =
        colon == std::string::npos ? std::string::npos : content.find('"', colon + 1);
    if (begin == std::string::npos)
        throw std::runtime_error("malformed native bundle manifest field: " + std::string(name));
    std::string value;
    for (std::size_t cursor = begin + 1; cursor < content.size(); ++cursor) {
        const auto character = content[cursor];
        if (character == '"')
            return value;
        if (character == '\\' && cursor + 1 < content.size()) {
            const auto escaped = content[++cursor];
            if (escaped != '"' && escaped != '\\' && escaped != '/')
                throw std::runtime_error("unsupported escape in native bundle manifest");
            value.push_back(escaped);
        } else {
            value.push_back(character);
        }
    }
    throw std::runtime_error("unterminated native bundle manifest field: " + std::string(name));
}

std::string asset_hash_field(const std::string& content, std::string_view name) {
    const auto assets = content.find("\"assets\"");
    if (assets == std::string::npos)
        throw std::runtime_error("native bundle manifest is missing assets");
    const auto key = content.find('"' + std::string(name) + '"', assets + 1);
    if (key == std::string::npos)
        throw std::runtime_error("native bundle manifest is missing asset: " + std::string(name));
    const auto sha = content.find("\"sha256\"", key);
    const auto colon = sha == std::string::npos ? std::string::npos : content.find(':', sha + 8);
    const auto begin =
        colon == std::string::npos ? std::string::npos : content.find('"', colon + 1);
    const auto end = begin == std::string::npos ? std::string::npos : content.find('"', begin + 1);
    if (end == std::string::npos || end == begin + 1)
        throw std::runtime_error("malformed native bundle asset hash: " + std::string(name));
    const auto value = content.substr(begin + 1, end - begin - 1);
    if (value.size() != 64 || std::any_of(value.begin(), value.end(), [](unsigned char character) {
            return std::isxdigit(character) == 0;
        }))
        throw std::runtime_error("invalid native bundle asset hash: " + std::string(name));
    return value;
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
    SileroBundle bundle;
    bundle.root = root;
    bundle.schema_version = json_string_field(content, "schema_version");
    bundle.bundle_version = json_string_field(content, "bundle_version");
    bundle.ort_version = json_string_field(content, "ort_version");
    bundle.source_revision = json_string_field(content, "source_revision");
    bundle.model_sha256 = json_string_field(content, "model_sha256");
    if (json_string_field(content, "record_type") != "silero_native_asset_manifest" ||
        bundle.schema_version != kSileroBundleSchemaVersion ||
        bundle.bundle_version != kSileroBundleVersion || bundle.ort_version != ort_version ||
        bundle.source_revision != kSileroBundleSourceRevision ||
        bundle.model_sha256 != kSileroBundleModelSha256) {
        throw std::runtime_error("unsupported native bundle compatibility profile");
    }
    for (const auto name : kRequiredAssets) {
        const auto hash = asset_hash_field(content, name);
        const auto path = root / name;
        if (sha256_file_hex(path, hash_chunk_size) != hash)
            throw std::runtime_error("native bundle SHA-256 mismatch: " + std::string(name));
        bundle.asset_sha256.emplace(std::string(name), hash);
    }
    return bundle;
}

} // namespace tts_front::detail
