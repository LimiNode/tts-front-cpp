#include "detail/silero_asset_hash.hpp"
#include "detail/silero_bundle.hpp"
#include "detail/silero_sentence.hpp"
#include "detail/silero_stress_backend.hpp"

#include <array>
#include <cstdlib>
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
    {
        const auto tokens = tts_front::detail::silero_tokenize("Это село🙂.");
        CHECK(tokens.size() == 5);
        CHECK(tokens[0].clean == "это" && tokens[0].process);
        CHECK(tokens[2].raw == "село" && tokens[2].clean == "село" && tokens[2].process);
        CHECK(tokens[3].raw == "🙂" && !tokens[3].process && !tokens[3].classifier_input);
        CHECK(tts_front::detail::silero_lower_ru("ЁЛКА") == "ёлка");
        CHECK(tts_front::detail::silero_clean_word("село🙂") == "село");
        CHECK(throws_runtime_error(
            [&] { (void)tts_front::detail::silero_tokenize(std::string("село\xFF", 7)); }));

        tts_front::detail::SileroRuntimeData data;
        data.phrase_rules["село"].push_back({"село", "Это [HOMO] село [/HOMO].", "сел+о"});
        CHECK(tts_front::detail::silero_phrase_variant(data, "село", "Это [HOMO] село [/HOMO].") ==
              "сел+о");
        CHECK(
            !tts_front::detail::silero_phrase_variant(data, "село", "Это [HOMO] селоход [/HOMO]."));
        data.ngram_ids.emplace("UNK", 0);
        data.dimension = 16;
        data.embedding_weights.assign(16, 1.0f);
        const auto embedding = tts_front::detail::silero_embed(data, "село");
        CHECK(embedding.size() == 16 && embedding.front() == 1.0f);
    }

#if defined(TTS_FRONT_ENABLE_ONNX_STRESS)
    if (const auto* asset_root = std::getenv("TTS_FRONT_SILERO_TEST_BUNDLE")) {
        tts_front::detail::SileroStressBackend backend({asset_root, "1.30.0", 1U << 20});
        CHECK(backend.runtime_data_ready() && backend.sessions_ready());
        struct Expected {
            const char* sentence;
            const char* pronunciation;
            std::vector<std::size_t> stressed_vowels;
        };
        for (const auto& expected :
             std::array{Expected{"Мама мыла раму.", "Мама мыла раму.", {0, 0, 0}},
                        Expected{"Квантолик.", "Квантолик.", {1}},
                        Expected{"Солнце село.", "Солнце село.", {0, 0}},
                        Expected{"Это большое село.", "Это большое село.", {0, 1, 1}},
                        Expected{"Елка ёлка.", "Ёлка ёлка.", {0, 0}},
                        Expected{"«Мама», — мел!", "«Мама», — мёл!", {0, 0}}}) {
            const auto result = backend.process(expected.sentence);
            CHECK(result.pronunciation_text == expected.pronunciation);
            CHECK(result.words.size() == expected.stressed_vowels.size());
            for (std::size_t index = 0; index < result.words.size(); ++index) {
                CHECK(result.words[index].stressed_vowel == expected.stressed_vowels[index]);
            }
            const auto repeated = backend.process(expected.sentence);
            CHECK(repeated.pronunciation_text == result.pronunciation_text);
            CHECK(repeated.words.size() == result.words.size());
        }
        const auto opaque = backend.process("Это большое село🙂.");
        CHECK(opaque.pronunciation_text == "Это большое село🙂.");
        CHECK(throws_runtime_error([&] { (void)backend.process(std::string("\xFF", 1)); }));
    }
#endif

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
    CHECK(throws_runtime_error([&] { (void)backend.process("Мама"); }));
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
