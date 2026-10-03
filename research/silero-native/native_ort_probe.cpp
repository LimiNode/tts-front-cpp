#include <onnxruntime_cxx_api.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

struct InputData {
    std::size_t rows = 0;
    std::size_t columns = 0;
    std::vector<float> values;
};

struct HomographData {
    std::size_t batch = 0;
    std::size_t sequence = 0;
    std::vector<std::int64_t> input_ids;
    std::vector<std::int64_t> starts;
    std::vector<std::int64_t> ends;
};

std::size_t checked_product(std::size_t left, std::size_t right) {
    if (right != 0 && left > std::numeric_limits<std::size_t>::max() / right) {
        throw std::runtime_error("input shape overflows size_t");
    }
    return left * right;
}

InputData read_accentor_input(const std::string& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("cannot open accentor input");
    }
    InputData data;
    if (!(input >> data.rows >> data.columns) || data.rows == 0 || data.columns == 0) {
        throw std::runtime_error("invalid accentor input shape");
    }
    data.values.resize(checked_product(data.rows, data.columns));
    for (auto& value : data.values) {
        if (!(input >> value)) {
            throw std::runtime_error("truncated accentor input");
        }
    }
    return data;
}

std::filesystem::path model_path(const char* path) {
#ifdef _WIN32
    const int required = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, nullptr, 0);
    if (required <= 0) {
        throw std::runtime_error("model path is not valid UTF-8");
    }
    std::wstring wide(static_cast<std::size_t>(required), L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, wide.data(), required) <= 0) {
        throw std::runtime_error("model path UTF-8 conversion failed");
    }
    wide.resize(static_cast<std::size_t>(required - 1));
    return std::filesystem::path(wide);
#else
    return std::filesystem::path(path);
#endif
}

HomographData read_homograph_input(const std::string& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("cannot open homograph input");
    }
    HomographData data;
    if (!(input >> data.batch >> data.sequence) || data.batch == 0 || data.sequence == 0) {
        throw std::runtime_error("invalid homograph input shape");
    }
    data.input_ids.resize(checked_product(data.batch, data.sequence));
    data.starts.resize(data.batch);
    data.ends.resize(data.batch);
    for (auto& value : data.input_ids) {
        if (!(input >> value)) {
            throw std::runtime_error("truncated homograph input ids");
        }
    }
    for (auto& value : data.starts) {
        if (!(input >> value) || value < 0 || static_cast<std::size_t>(value) >= data.sequence) {
            throw std::runtime_error("invalid homograph start index");
        }
    }
    for (auto& value : data.ends) {
        if (!(input >> value) || value < 0 || static_cast<std::size_t>(value) >= data.sequence) {
            throw std::runtime_error("invalid homograph end index");
        }
    }
    return data;
}

void print_output(const Ort::Value& output) {
    const auto info = output.GetTensorTypeAndShapeInfo();
    if (info.GetElementType() != ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT) {
        throw std::runtime_error("probe expects float graph output");
    }
    const auto shape = info.GetShape();
    std::size_t count = 1;
    std::cout << "shape=";
    for (std::size_t index = 0; index < shape.size(); ++index) {
        if (shape[index] < 0) {
            throw std::runtime_error("dynamic output shape was not resolved");
        }
        if (index != 0) {
            std::cout << ',';
        }
        std::cout << shape[index];
        count = checked_product(count, static_cast<std::size_t>(shape[index]));
    }
    std::cout << "\nvalues=" << std::setprecision(9);
    const auto* values = output.GetTensorData<float>();
    for (std::size_t index = 0; index < count; ++index) {
        if (index != 0) {
            std::cout << ',';
        }
        std::cout << values[index];
    }
    std::cout << '\n';
}

std::vector<std::string> input_names(Ort::Session& session, Ort::AllocatorWithDefaultOptions& allocator) {
    std::vector<std::string> names;
    names.reserve(session.GetInputCount());
    for (std::size_t index = 0; index < session.GetInputCount(); ++index) {
        auto name = session.GetInputNameAllocated(index, allocator);
        names.emplace_back(name.get());
    }
    return names;
}

std::vector<std::string> output_names(Ort::Session& session,
                                     Ort::AllocatorWithDefaultOptions& allocator) {
    std::vector<std::string> names;
    names.reserve(session.GetOutputCount());
    for (std::size_t index = 0; index < session.GetOutputCount(); ++index) {
        auto name = session.GetOutputNameAllocated(index, allocator);
        names.emplace_back(name.get());
    }
    return names;
}

std::vector<Ort::Value> run_all_outputs(Ort::Session& session,
                                        const std::vector<std::string>& names,
                                        std::vector<Ort::Value>& tensors) {
    std::vector<const char*> input_ptrs;
    input_ptrs.reserve(names.size());
    for (const auto& name : names) {
        input_ptrs.push_back(name.c_str());
    }
    Ort::AllocatorWithDefaultOptions allocator;
    const auto outputs = output_names(session, allocator);
    std::vector<const char*> output_ptrs;
    output_ptrs.reserve(outputs.size());
    for (const auto& name : outputs) {
        output_ptrs.push_back(name.c_str());
    }
    return session.Run(Ort::RunOptions{nullptr}, input_ptrs.data(), tensors.data(), tensors.size(),
                       output_ptrs.data(), output_ptrs.size());
}

void run_accentor(Ort::Session& session, const InputData& data, Ort::MemoryInfo& memory) {
    Ort::Value tensor = Ort::Value::CreateTensor<float>(
        memory, const_cast<float*>(data.values.data()), data.values.size(),
        std::array<std::int64_t, 2>{static_cast<std::int64_t>(data.rows),
                                    static_cast<std::int64_t>(data.columns)}
            .data(),
        2);
    Ort::AllocatorWithDefaultOptions allocator;
    const auto names = input_names(session, allocator);
    if (names.size() != 1) {
        throw std::runtime_error("accentor graph must have exactly one input");
    }
    std::vector<Ort::Value> tensors;
    tensors.emplace_back(std::move(tensor));
    auto output = run_all_outputs(session, names, tensors);
    print_output(output.at(0));
}

void run_homograph(Ort::Session& session, const HomographData& data, Ort::MemoryInfo& memory) {
    const std::array<std::int64_t, 2> input_shape{
        static_cast<std::int64_t>(data.batch), static_cast<std::int64_t>(data.sequence)};
    const std::array<std::int64_t, 1> span_shape{static_cast<std::int64_t>(data.batch)};
    std::vector<Ort::Value> tensors;
    tensors.emplace_back(Ort::Value::CreateTensor<std::int64_t>(
        memory, const_cast<std::int64_t*>(data.input_ids.data()), data.input_ids.size(),
        input_shape.data(), input_shape.size()));
    tensors.emplace_back(Ort::Value::CreateTensor<std::int64_t>(
        memory, const_cast<std::int64_t*>(data.starts.data()), data.starts.size(), span_shape.data(), 1));
    tensors.emplace_back(Ort::Value::CreateTensor<std::int64_t>(
        memory, const_cast<std::int64_t*>(data.ends.data()), data.ends.size(), span_shape.data(), 1));
    Ort::AllocatorWithDefaultOptions allocator;
    const auto names = input_names(session, allocator);
    if (names.size() != tensors.size()) {
        throw std::runtime_error("homograph graph input count mismatch");
    }
    auto output = run_all_outputs(session, names, tensors);
    print_output(output.at(0));
}

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 7 || std::string(argv[1]) != "--graph" ||
            std::string(argv[3]) != "--kind" || std::string(argv[5]) != "--input") {
            std::cerr << "usage: silero_native_ort_probe --graph FILE --kind accentor|homograph --input FILE\n";
            return 2;
        }
        Ort::Env environment(ORT_LOGGING_LEVEL_WARNING, "silero-native");
        Ort::SessionOptions options;
        options.SetIntraOpNumThreads(1);
        options.SetInterOpNumThreads(1);
        options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
        const auto path = model_path(argv[2]);
        Ort::Session session(environment, path.c_str(), options);
        Ort::MemoryInfo memory = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        const std::string kind = argv[4];
        if (kind == "accentor") {
            run_accentor(session, read_accentor_input(argv[6]), memory);
        } else if (kind == "homograph") {
            run_homograph(session, read_homograph_input(argv[6]), memory);
        } else {
            throw std::runtime_error("unknown graph kind");
        }
        return 0;
    } catch (const Ort::Exception& error) {
        std::cerr << "ONNX Runtime failure: " << error.what() << '\n';
        return 1;
    } catch (const std::exception& error) {
        std::cerr << "native ORT probe failed: " << error.what() << '\n';
        return 1;
    }
}
