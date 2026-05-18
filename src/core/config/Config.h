#pragma once
//
// core/config/Config.h
//
// TOML configuration loader. Wraps toml++ with a dotted-path API so
// callers can write `config.get<float>("evolution.mutation.add_node_rate")`
// instead of navigating tables manually.
//
// Configs are loaded once at startup. Hot-reload is not supported in M0;
// adding it later means re-firing a `ConfigReloaded` event.

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <toml++/toml.h>

namespace neuro::config {

class Config {
public:
    Config() = default;

    // Load from a file. Throws std::runtime_error on parse failure or if
    // the file doesn't exist.
    static Config loadFromFile(const std::filesystem::path& path);

    // Load from a string (used by tests).
    static Config loadFromString(std::string_view source);

    // ----- typed getters --------------------------------------------------
    // Path is a dot-separated key, e.g. "world.size". Last segment may be
    // an array index (rare; not supported in M0).
    //
    // get<T>:   throws if missing or wrong type.
    // tryGet<T>: returns nullopt if missing or wrong type (no throw).
    // getOr<T>: returns the default if missing or wrong type.

    template <typename T>
    T get(std::string_view path) const {
        auto opt = tryGet<T>(path);
        if (!opt) throwMissing(path);
        return *opt;
    }

    template <typename T>
    std::optional<T> tryGet(std::string_view path) const;

    template <typename T>
    T getOr(std::string_view path, T fallback) const {
        auto opt = tryGet<T>(path);
        return opt ? *opt : fallback;
    }

    // Vec<T> getter for arrays. Currently supports int / double / string.
    template <typename T>
    std::vector<T> getArray(std::string_view path) const;

    // True if the key exists.
    bool has(std::string_view path) const;

    // Source for diagnostics.
    const std::filesystem::path& path() const { return path_; }

    // Raw access for advanced use (e.g. iterating module config blocks).
    const toml::table& raw() const { return root_; }

private:
    explicit Config(toml::table root, std::filesystem::path path = {});
    [[noreturn]] static void throwMissing(std::string_view path);
    static const toml::node* resolve(const toml::table& root,
                                     std::string_view path);

    toml::table             root_;
    std::filesystem::path   path_;
};

}  // namespace neuro::config
