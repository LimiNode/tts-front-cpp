#include "detail/silero_asset_hash.hpp"
#include "detail/silero_bundle.hpp"
#include "detail/silero_stress_backend.hpp"

#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            std::cerr << "CHECK failed: " << #condition << " at " << __FILE__ << ":" << __LINE__   \
                      << "\n";                                                                     \
            return 1;                                                                              \
        }                                                                                          \
    } while (false)

namespace {

constexpr std::array<const char*, 9> kAssets{"ngrams.tsv",
                                             "embedding.f32",
                                             "bert-vocab.tsv",
                                             "homodict.tsv",
                                             "exceptions.tsv",
                                             "phrase-rules.tsv",
                                             "stress.onnx",
                                             "yo.onnx",
                                             "homosolver.onnx"};

void write_bundle(const std::filesystem::path& root, std::string_view source_revision) {
    std::filesystem::create_directories(root);
    for (const auto* name : kAssets) {
        std::ofstream(root / name, std::ios::binary | std::ios::trunc);
    }
    std::ofstream(root / "homodict.json", std::ios::binary | std::ios::trunc) << "{}\n";
    std::ofstream manifest(root / "manifest.json", std::ios::binary | std::ios::trunc);
    manifest << "{\n"
             << "  \"record_type\": \"silero_native_asset_manifest\",\n"
             << "  \"schema_version\": \"1\",\n"
             << "  \"bundle_version\": \"silero-native-phase1-v1\",\n"
             << "  \"ort_version\": \"1.30.0\",\n"
             << "  \"source_revision\": \"" << source_revision << "\",\n"
             << "  \"model_sha256\": \"" << tts_front::detail::kSileroBundleModelSha256 << "\",\n"
             << "  \"assets\": {\n";
    for (std::size_t index = 0; index < kAssets.size(); ++index) {
        const auto* name = kAssets[index];
        manifest << "    \"" << name << "\": {\"sha256\": \""
                 << tts_front::detail::sha256_file_hex(root / name, 1) << "\"}"
                 << (index + 1 == kAssets.size() ? "\n" : ",\n");
    }
    manifest << "  }\n}\n";
}

std::string read_manifest(const std::filesystem::path& root) {
    std::ifstream input(root / "manifest.json", std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

void write_manifest(const std::filesystem::path& root, const std::string& manifest) {
    std::ofstream output(root / "manifest.json", std::ios::binary | std::ios::trunc);
    output << manifest;
}

void replace_once(std::string& value, std::string_view from, std::string_view to) {
    const auto position = value.find(from);
    if (position == std::string::npos)
        throw std::runtime_error("test manifest replacement did not find source");
    value.replace(position, from.size(), to);
}

template <typename Function> bool throws_runtime_error(Function&& function) {
    try {
        function();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

} // namespace

int main() {
    const auto root = std::filesystem::temp_directory_path() / "tts_front_silero_bundle_test";
    std::filesystem::remove_all(root);
    write_bundle(root, tts_front::detail::kSileroBundleSourceRevision);

    const auto bundle = tts_front::detail::load_silero_bundle(root, "1.30.0", 1);
    CHECK(bundle.asset_sha256.size() == kAssets.size());
    CHECK(bundle.asset("stress.onnx") == root / "stress.onnx");
    CHECK(throws_runtime_error([&] { (void)bundle.asset("homodict.json"); }));
    CHECK(throws_runtime_error([&] { (void)bundle.asset("undeclared.bin"); }));
    std::filesystem::remove(root / "homodict.json");
    CHECK(tts_front::detail::load_silero_bundle(root, "1.30.0", 1).asset_sha256.size() ==
          kAssets.size());
#if !defined(TTS_FRONT_ENABLE_ONNX_STRESS)
    tts_front::detail::SileroStressBackend backend({root, "1.30.0", 1});
    CHECK(backend.bundle_validated());
    CHECK(!backend.sessions_ready());
#endif

    CHECK(throws_runtime_error(
        [&] { (void)tts_front::detail::load_silero_bundle(root, "1.29.0", 1); }));

    {
        std::ofstream(root / "yo.onnx", std::ios::binary | std::ios::trunc) << "corrupt";
        CHECK(throws_runtime_error(
            [&] { (void)tts_front::detail::load_silero_bundle(root, "1.30.0", 1); }));
    }

    write_bundle(root, std::string(40, 'a'));
    CHECK(throws_runtime_error(
        [&] { (void)tts_front::detail::load_silero_bundle(root, "1.30.0", 1); }));

    write_bundle(root, tts_front::detail::kSileroBundleSourceRevision);
    {
        std::ifstream input(root / "manifest.json", std::ios::binary);
        std::string manifest{std::istreambuf_iterator<char>(input),
                             std::istreambuf_iterator<char>()};
        const auto position = manifest.find(tts_front::detail::kSileroBundleModelSha256);
        CHECK(position != std::string::npos);
        manifest.replace(
            position, tts_front::detail::kSileroBundleModelSha256.size(), std::string(64, 'b'));
        std::ofstream output(root / "manifest.json", std::ios::binary | std::ios::trunc);
        output << manifest;
    }
    CHECK(throws_runtime_error(
        [&] { (void)tts_front::detail::load_silero_bundle(root, "1.30.0", 1); }));

    write_bundle(root, tts_front::detail::kSileroBundleSourceRevision);
    {
        auto manifest = read_manifest(root);
        manifest.insert(manifest.rfind('}'), ",\"schema_version\":\"1\"");
        write_manifest(root, manifest);
        CHECK(throws_runtime_error(
            [&] { (void)tts_front::detail::load_silero_bundle(root, "1.30.0", 1); }));
    }

    write_bundle(root, tts_front::detail::kSileroBundleSourceRevision);
    {
        auto manifest = read_manifest(root);
        replace_once(manifest, "\"schema_version\": \"1\"", "\"schema_version\": 1");
        write_manifest(root, manifest);
        CHECK(throws_runtime_error(
            [&] { (void)tts_front::detail::load_silero_bundle(root, "1.30.0", 1); }));
    }

    write_bundle(root, tts_front::detail::kSileroBundleSourceRevision);
    {
        auto manifest = read_manifest(root);
        replace_once(manifest,
                     "\"record_type\": \"silero_native_asset_manifest\"",
                     "\"record_type\": \"bad\\q\"");
        write_manifest(root, manifest);
        CHECK(throws_runtime_error(
            [&] { (void)tts_front::detail::load_silero_bundle(root, "1.30.0", 1); }));
    }

    write_bundle(root, tts_front::detail::kSileroBundleSourceRevision);
    {
        auto manifest = read_manifest(root);
        replace_once(manifest, "\"stress.onnx\":", "\"outside.onnx\":");
        manifest.insert(manifest.rfind('}'),
                        ",\"stress.onnx\":{\"sha256\":\"" +
                            tts_front::detail::sha256_file_hex(root / "stress.onnx", 1) + "\"}");
        write_manifest(root, manifest);
        CHECK(throws_runtime_error(
            [&] { (void)tts_front::detail::load_silero_bundle(root, "1.30.0", 1); }));
    }

    write_bundle(root, tts_front::detail::kSileroBundleSourceRevision);
    {
        auto manifest = read_manifest(root);
        const auto close = manifest.rfind("  }\n");
        manifest.insert(close,
                        ",\"stress.onnx\":{\"sha256\":\"" +
                            tts_front::detail::sha256_file_hex(root / "stress.onnx", 1) + "\"}");
        write_manifest(root, manifest);
        CHECK(throws_runtime_error(
            [&] { (void)tts_front::detail::load_silero_bundle(root, "1.30.0", 1); }));
    }

    write_bundle(root, tts_front::detail::kSileroBundleSourceRevision);
    {
        auto manifest = read_manifest(root);
        manifest += "garbage";
        write_manifest(root, manifest);
        CHECK(throws_runtime_error(
            [&] { (void)tts_front::detail::load_silero_bundle(root, "1.30.0", 1); }));
    }

    write_bundle(root, tts_front::detail::kSileroBundleSourceRevision);
    std::filesystem::remove(root / "phrase-rules.tsv");
    CHECK(throws_runtime_error(
        [&] { (void)tts_front::detail::load_silero_bundle(root, "1.30.0", 1); }));

    write_bundle(root, tts_front::detail::kSileroBundleSourceRevision);
    std::filesystem::remove(root / "manifest.json");
    CHECK(throws_runtime_error(
        [&] { (void)tts_front::detail::load_silero_bundle(root, "1.30.0", 1); }));

    write_bundle(root, tts_front::detail::kSileroBundleSourceRevision);
    {
        auto manifest = read_manifest(root);
        const auto hash = tts_front::detail::sha256_file_hex(root / "stress.onnx", 1);
        replace_once(manifest, hash, std::string(63, 'a'));
        write_manifest(root, manifest);
        CHECK(throws_runtime_error(
            [&] { (void)tts_front::detail::load_silero_bundle(root, "1.30.0", 1); }));
    }

    std::filesystem::remove_all(root);
    return 0;
}
