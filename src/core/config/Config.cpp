#include "core/config/Config.h"

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace neuro::config {

namespace {

// Split "a.b.c" into ["a", "b", "c"]. We don't support escaped dots.
std::vector<std::string_view> splitPath(std::string_view path) {
    std::vector<std::string_view> parts;
    std::size_t start = 0;
    for (std::size_t i = 0; i < path.size(); ++i) {
        if (path[i] == '.') {
            parts.emplace_back(path.substr(start, i - start));
            start = i + 1;
        }
    }
    if (start < path.size()) {
        parts.emplace_back(path.substr(start));
    }
    return parts;
}

}  // namespace

// ---------------------------------------------------------------------------
// construction
// ---------------------------------------------------------------------------
Config::Config(toml::table root, std::filesystem::path path)
    : root_(std::move(root)), path_(std::move(path)) {}

Config Config::loadFromFile(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        throw std::runtime_error("Config file not found: " + path.string());
    }
    try {
        auto table = toml::parse_file(path.string());
        return Config{std::move(table), path};
    } catch (const toml::parse_error& e) {
        std::ostringstream os;
        os << "Failed to parse " << path << ": " << e.description()
           << " at " << e.source().begin;
        throw std::runtime_error(os.str());
    }
}

Config Config::loadFromString(std::string_view source) {
    try {
        auto table = toml::parse(source);
        return Config{std::move(table)};
    } catch (const toml::parse_error& e) {
        std::ostringstream os;
        os << "Failed to parse TOML string: " << e.description();
        throw std::runtime_error(os.str());
    }
}

// ---------------------------------------------------------------------------
// path resolution
// ---------------------------------------------------------------------------
const toml::node* Config::resolve(const toml::table& root,
                                  std::string_view path) {
    const auto parts = splitPath(path);
    if (parts.empty()) return nullptr;

    const toml::node* current = &root;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        const auto* table = current->as_table();
        if (!table) return nullptr;
        auto it = table->find(parts[i]);
        if (it == table->end()) return nullptr;
        current = &it->second;
    }
    return current;
}

void Config::throwMissing(std::string_view path) {
    throw std::runtime_error("Config key not found or wrong type: "
                             + std::string(path));
}

bool Config::has(std::string_view path) const {
    return resolve(root_, path) != nullptr;
}

// ---------------------------------------------------------------------------
// typed getters - explicit instantiations only
// ---------------------------------------------------------------------------
template <>
std::optional<bool> Config::tryGet<bool>(std::string_view path) const {
    auto node = resolve(root_, path);
    if (!node) return std::nullopt;
    if (auto v = node->value<bool>()) return *v;
    return std::nullopt;
}

template <>
std::optional<int> Config::tryGet<int>(std::string_view path) const {
    auto node = resolve(root_, path);
    if (!node) return std::nullopt;
    if (auto v = node->value<int64_t>()) return static_cast<int>(*v);
    return std::nullopt;
}

template <>
std::optional<int64_t> Config::tryGet<int64_t>(std::string_view path) const {
    auto node = resolve(root_, path);
    if (!node) return std::nullopt;
    if (auto v = node->value<int64_t>()) return *v;
    return std::nullopt;
}

template <>
std::optional<std::size_t> Config::tryGet<std::size_t>(std::string_view path) const {
    auto node = resolve(root_, path);
    if (!node) return std::nullopt;
    if (auto v = node->value<int64_t>()) {
        if (*v < 0) return std::nullopt;
        return static_cast<std::size_t>(*v);
    }
    return std::nullopt;
}

template <>
std::optional<float> Config::tryGet<float>(std::string_view path) const {
    auto node = resolve(root_, path);
    if (!node) return std::nullopt;
    if (auto v = node->value<double>()) return static_cast<float>(*v);
    if (auto v = node->value<int64_t>()) return static_cast<float>(*v);
    return std::nullopt;
}

template <>
std::optional<double> Config::tryGet<double>(std::string_view path) const {
    auto node = resolve(root_, path);
    if (!node) return std::nullopt;
    if (auto v = node->value<double>()) return *v;
    if (auto v = node->value<int64_t>()) return static_cast<double>(*v);
    return std::nullopt;
}

template <>
std::optional<std::string> Config::tryGet<std::string>(std::string_view path) const {
    auto node = resolve(root_, path);
    if (!node) return std::nullopt;
    if (auto v = node->value<std::string>()) return *v;
    return std::nullopt;
}

// ---------------------------------------------------------------------------
// array getters
// ---------------------------------------------------------------------------
template <>
std::vector<int> Config::getArray<int>(std::string_view path) const {
    auto node = resolve(root_, path);
    if (!node) throwMissing(path);
    const auto* arr = node->as_array();
    if (!arr) throwMissing(path);
    std::vector<int> result;
    result.reserve(arr->size());
    for (const auto& elem : *arr) {
        if (auto v = elem.value<int64_t>()) {
            result.push_back(static_cast<int>(*v));
        } else {
            throwMissing(path);
        }
    }
    return result;
}

template <>
std::vector<double> Config::getArray<double>(std::string_view path) const {
    auto node = resolve(root_, path);
    if (!node) throwMissing(path);
    const auto* arr = node->as_array();
    if (!arr) throwMissing(path);
    std::vector<double> result;
    result.reserve(arr->size());
    for (const auto& elem : *arr) {
        if (auto double_value = elem.value<double>()) {
            result.push_back(*double_value);
        } else if (auto int_value = elem.value<int64_t>()) {
            result.push_back(static_cast<double>(*int_value));
        } else {
            throwMissing(path);
        }
    }
    return result;
}

template <>
std::vector<float> Config::getArray<float>(std::string_view path) const {
    auto d = getArray<double>(path);
    std::vector<float> result;
    result.reserve(d.size());
    for (double value : d) {
        result.push_back(static_cast<float>(value));
    }
    return result;
}

template <>
std::vector<std::string> Config::getArray<std::string>(std::string_view path) const {
    auto node = resolve(root_, path);
    if (!node) throwMissing(path);
    const auto* arr = node->as_array();
    if (!arr) throwMissing(path);
    std::vector<std::string> result;
    result.reserve(arr->size());
    for (const auto& elem : *arr) {
        if (auto v = elem.value<std::string>()) {
            result.push_back(*v);
        } else {
            throwMissing(path);
        }
    }
    return result;
}

}  // namespace neuro::config
