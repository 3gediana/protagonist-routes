#include "runtime/MainWorldPersistence.h"

#include "brain/ProtagonistGenome.h"
#include "core/brain/neat/NeatGenome.h"
#include "core/types/Forward.h"

#include <spdlog/spdlog.h>

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string_view>
#include <system_error>

namespace neuro::routes::protagonist {

namespace {

constexpr const char* kManifestName = "main_world_manifest.json";

// Minimal JSON escape (same alphabet as ProtagonistEcologyRuntime::jsonEscape).
// Local copy to keep MainWorldPersistence self-contained.
std::string escapeJson(std::string_view input) {
    std::string out;
    out.reserve(input.size() + 8);
    for (const char ch : input) {
        switch (ch) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out += ch; break;
        }
    }
    return out;
}

// Genome on-disk binary format: archetype 0 = ProtagonistGenome,
// archetype >= 1 = NeatGenome. This mirrors the runtime's actual
// population layout (see ProtagonistEcologyRuntime population +
// species_contexts[].population in createBootstrapWorld).
bool saveGenome(const IGenome& g, std::size_t archetype, const std::filesystem::path& path) {
    if (archetype == 0) {
        const auto* protag = dynamic_cast<const ProtagonistGenome*>(&g);
        if (protag == nullptr) {
            spdlog::warn("MainWorldPersistence: archetype 0 genome is not ProtagonistGenome; skipping");
            return false;
        }
        return protag->saveToFile(path);
    }
    const auto* neat = dynamic_cast<const NeatGenome*>(&g);
    if (neat == nullptr) {
        spdlog::warn("MainWorldPersistence: archetype {} genome is not NeatGenome; skipping", archetype);
        return false;
    }
    return neat->saveToFile(path);
}

std::unique_ptr<IGenome> loadGenome(std::size_t archetype, const std::filesystem::path& path) {
    if (archetype == 0) {
        return ProtagonistGenome::loadFromFile(path);
    }
    return NeatGenome::loadFromFile(path);
}

bool savePool(const WorldPopulationPool& pool,
              std::size_t archetype,
              const std::filesystem::path& dir,
              std::ostringstream& json_entries) {
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    if (ec) {
        spdlog::error("MainWorldPersistence: failed to create dir {}: {}", dir.string(), ec.message());
        return false;
    }
    for (std::size_t i = 0; i < pool.genomes.size(); ++i) {
        if (pool.genomes[i] == nullptr) continue;
        char fname[64];
        std::snprintf(fname, sizeof(fname), "genome_%zu.bin", i);
        const auto blob_path = dir / fname;
        if (!saveGenome(*pool.genomes[i], archetype, blob_path)) {
            return false;
        }
        if (json_entries.tellp() > std::streampos(0)) {
            json_entries << ",\n";
        }
        const double fit = (i < pool.fitnesses.size()) ? pool.fitnesses[i] : 0.0;
        const std::uint64_t lineage = (i < pool.lineage_ids.size()) ? pool.lineage_ids[i] : 0u;
        json_entries << "        {\"file\":\"" << escapeJson(fname)
                     << "\",\"genome_id\":" << pool.genomes[i]->id().value
                     << ",\"lineage_id\":" << lineage
                     << ",\"fitness\":" << fit << "}";
    }
    return true;
}

// Write `pools_by_archetype` as a JSON array. Each entry is an object
// {archetype:N, entries:[...]}.
bool saveSlot(const WorldSlot& slot,
              const std::string& slot_subdir,
              const std::filesystem::path& root,
              std::ostringstream& slot_json) {
    slot_json << "  \"" << slot_subdir << "\": [\n";
    bool first_archetype = true;
    for (std::size_t a = 0; a < slot.pools_by_archetype.size(); ++a) {
        if (!first_archetype) slot_json << ",\n";
        first_archetype = false;
        const auto archetype_dir = root / slot_subdir / ("archetype_" + std::to_string(a));
        std::ostringstream entries_json;
        if (!savePool(slot.pools_by_archetype[a], a, archetype_dir, entries_json)) {
            return false;
        }
        slot_json << "    {\"archetype\":" << a
                  << ",\"borderland_eval_count\":" << slot.pools_by_archetype[a].borderland_eval_count
                  << ",\"entries\":[\n"
                  << entries_json.str() << "\n    ]}";
    }
    slot_json << "\n  ]";
    return true;
}

// Tiny JSON state machine. We control the manifest's shape, so we only
// need primitive whitespace + string + number + bracket recognition,
// not a full parser.
struct JsonScanner {
    const std::string& src;
    std::size_t pos{0};
    explicit JsonScanner(const std::string& s) : src(s) {}
    void skipWs() {
        while (pos < src.size() && (src[pos] == ' ' || src[pos] == '\t'
                                    || src[pos] == '\n' || src[pos] == '\r')) ++pos;
    }
    bool consume(char c) {
        skipWs();
        if (pos < src.size() && src[pos] == c) { ++pos; return true; }
        return false;
    }
    bool peek(char c) {
        skipWs();
        return pos < src.size() && src[pos] == c;
    }
    std::string readString() {
        skipWs();
        if (pos >= src.size() || src[pos] != '"') return {};
        ++pos;
        std::string out;
        while (pos < src.size() && src[pos] != '"') {
            if (src[pos] == '\\' && pos + 1 < src.size()) {
                const char esc = src[pos + 1];
                switch (esc) {
                    case '"': out += '"'; break;
                    case '\\': out += '\\'; break;
                    case 'n': out += '\n'; break;
                    case 'r': out += '\r'; break;
                    case 't': out += '\t'; break;
                    default: out += esc; break;
                }
                pos += 2;
            } else {
                out += src[pos++];
            }
        }
        if (pos < src.size()) ++pos;  // closing quote
        return out;
    }
    double readNumber() {
        skipWs();
        std::size_t start = pos;
        if (pos < src.size() && (src[pos] == '-' || src[pos] == '+')) ++pos;
        while (pos < src.size() && (std::isdigit(static_cast<unsigned char>(src[pos]))
                                    || src[pos] == '.' || src[pos] == 'e'
                                    || src[pos] == 'E' || src[pos] == '-'
                                    || src[pos] == '+')) ++pos;
        if (start == pos) return 0.0;
        try {
            return std::stod(src.substr(start, pos - start));
        } catch (...) {
            return 0.0;
        }
    }
    std::uint64_t readUint() {
        return static_cast<std::uint64_t>(readNumber());
    }
    bool readBool() {
        skipWs();
        if (pos + 4 <= src.size() && src.compare(pos, 4, "true") == 0) {
            pos += 4;
            return true;
        }
        if (pos + 5 <= src.size() && src.compare(pos, 5, "false") == 0) {
            pos += 5;
            return false;
        }
        return false;  // defensive: malformed treated as false
    }
    // Skip a balanced value (object / array / string / number / literal).
    void skipValue() {
        skipWs();
        if (pos >= src.size()) return;
        const char c = src[pos];
        if (c == '{' || c == '[') {
            const char open = c;
            const char close = (c == '{') ? '}' : ']';
            int depth = 0;
            while (pos < src.size()) {
                if (src[pos] == open) ++depth;
                else if (src[pos] == close) {
                    --depth;
                    if (depth == 0) { ++pos; return; }
                }
                else if (src[pos] == '"') { readString(); continue; }
                ++pos;
            }
            return;
        }
        if (c == '"') { (void)readString(); return; }
        // number / true / false / null
        while (pos < src.size() && src[pos] != ',' && src[pos] != '}' && src[pos] != ']') ++pos;
    }
};

bool loadPoolEntries(JsonScanner& sc,
                     std::size_t archetype,
                     const std::filesystem::path& archetype_dir,
                     WorldPopulationPool& out_pool) {
    if (!sc.consume('[')) return false;
    while (!sc.peek(']')) {
        if (!sc.consume('{')) return false;
        std::string blob_file;
        std::uint64_t genome_id_value = 0;
        std::uint64_t lineage_id = 0;
        double fitness = 0.0;
        while (!sc.peek('}')) {
            const std::string key = sc.readString();
            if (!sc.consume(':')) return false;
            if (key == "file") {
                blob_file = sc.readString();
            } else if (key == "genome_id") {
                genome_id_value = sc.readUint();
            } else if (key == "lineage_id") {
                lineage_id = sc.readUint();
            } else if (key == "fitness") {
                fitness = sc.readNumber();
            } else {
                sc.skipValue();
            }
            sc.consume(',');
        }
        sc.consume('}');
        sc.consume(',');

        auto genome = loadGenome(archetype, archetype_dir / blob_file);
        if (genome == nullptr) {
            spdlog::error("MainWorldPersistence: failed to load genome blob {}", (archetype_dir / blob_file).string());
            return false;
        }
        // Genome IDs in the binary blob already hold the original id;
        // preserve it. (Manifest's genome_id is informational/cross-check.)
        (void)genome_id_value;
        out_pool.genomes.push_back(std::move(genome));
        out_pool.lineage_ids.push_back(lineage_id);
        out_pool.fitnesses.push_back(fitness);
    }
    sc.consume(']');
    return true;
}

bool loadSlot(JsonScanner& sc,
              const std::filesystem::path& root,
              const std::string& slot_subdir,
              WorldSlot& out_slot) {
    if (!sc.consume('[')) return false;
    while (!sc.peek(']')) {
        if (!sc.consume('{')) return false;
        std::size_t archetype = 0;
        std::size_t eval_count_val = 0;
        // Parse the "archetype" key first so we know where to scope the
        // pool load. Then handle "entries". Manifest keys are emitted in
        // a fixed order by save, but we don't depend on that here.
        // Buffer keys/values: we need archetype before we can load entries.
        std::size_t entries_anchor = std::string::npos;
        bool seen_archetype = false;
        while (!sc.peek('}')) {
            const std::string key = sc.readString();
            if (!sc.consume(':')) return false;
            if (key == "archetype") {
                archetype = static_cast<std::size_t>(sc.readUint());
                seen_archetype = true;
            } else if (key == "borderland_eval_count") {
                eval_count_val = static_cast<std::size_t>(sc.readUint());
            } else if (key == "entries") {
                if (!seen_archetype) {
                    // Save format always emits archetype before entries;
                    // we can rely on this for the common case. If not,
                    // bail.
                    spdlog::error("MainWorldPersistence: manifest entries before archetype");
                    return false;
                }
                if (out_slot.pools_by_archetype.size() <= archetype) {
                    out_slot.pools_by_archetype.resize(archetype + 1);
                }
                const auto archetype_dir = root / slot_subdir / ("archetype_" + std::to_string(archetype));
                if (!loadPoolEntries(sc, archetype, archetype_dir,
                                     out_slot.pools_by_archetype[archetype])) {
                    return false;
                }
                entries_anchor = sc.pos;
            } else {
                sc.skipValue();
            }
            sc.consume(',');
        }
        // Restore the per-pool borderland_eval_count (the 50-gen
        // continuity gate ranks promotion on it). Applied after the
        // entries pass so the pool slot is guaranteed to exist.
        if (seen_archetype && archetype < out_slot.pools_by_archetype.size()) {
            out_slot.pools_by_archetype[archetype].borderland_eval_count = eval_count_val;
        }
        sc.consume('}');
        sc.consume(',');
        (void)entries_anchor;
    }
    sc.consume(']');
    return true;
}

}  // namespace

bool MainWorldPersistence::save(const TripleWorldRuntime& triple_world,
                                const MainWorldSnapshotMetadata& meta,
                                const std::filesystem::path& persist_dir) {
    std::error_code ec;
    std::filesystem::create_directories(persist_dir, ec);
    if (ec) {
        spdlog::error("MainWorldPersistence::save: cannot create {}: {}", persist_dir.string(), ec.message());
        return false;
    }

    std::ostringstream borderland_json;
    std::ostringstream main_json;
    if (!saveSlot(triple_world.borderland(), "borderland", persist_dir, borderland_json)) {
        return false;
    }
    if (!saveSlot(triple_world.mainWorld(), "main_world", persist_dir, main_json)) {
        return false;
    }

    std::ofstream manifest(persist_dir / kManifestName, std::ios::out | std::ios::trunc);
    if (!manifest.is_open()) {
        spdlog::error("MainWorldPersistence::save: cannot open manifest for write");
        return false;
    }
    manifest << "{\n"
             << "  \"snapshot_id\": " << meta.snapshot_id << ",\n"
             << "  \"sim_time_seconds\": " << meta.sim_time_seconds << ",\n"
             << "  \"generations_elapsed\": " << meta.generations_elapsed << ",\n"
             << "  \"scenario_name\": \"" << escapeJson(meta.scenario_name) << "\",\n";
    // Stone Bootstrap commit 3: per-archetype curriculum progression
    // state. Written as a flat array of {fire,worksite,stone,heavy,fire2}
    // booleans, one entry per archetype slot. Skipped entirely when
    // archetype_progression is empty so old snapshot consumers stay
    // happy.
    if (!meta.archetype_progression.empty()) {
        manifest << "  \"archetype_progression\": [\n";
        for (std::size_t a = 0; a < meta.archetype_progression.size(); ++a) {
            const auto& s = meta.archetype_progression[a];
            manifest << "    {\"first_fire_ignited\":"  << (s.first_fire_ignited  ? "true" : "false")
                     << ",\"worksite_unlocked\":"      << (s.worksite_unlocked    ? "true" : "false")
                     << ",\"stone_unlocked\":"         << (s.stone_unlocked       ? "true" : "false")
                     << ",\"heavy_unlocked\":"         << (s.heavy_unlocked       ? "true" : "false")
                     << ",\"second_fire_ignited\":"    << (s.second_fire_ignited  ? "true" : "false")
                     << "}";
            if (a + 1 < meta.archetype_progression.size()) manifest << ",";
            manifest << "\n";
        }
        manifest << "  ],\n";
    }
    manifest << borderland_json.str() << ",\n"
             << main_json.str() << "\n"
             << "}\n";
    return static_cast<bool>(manifest);
}

bool MainWorldPersistence::load(const std::filesystem::path& persist_dir,
                                TripleWorldRuntime& triple_world,
                                MainWorldSnapshotMetadata* out_meta) {
    const auto manifest_path = persist_dir / kManifestName;
    if (!std::filesystem::exists(manifest_path)) {
        return false;
    }

    std::ifstream manifest(manifest_path);
    if (!manifest.is_open()) return false;
    std::stringstream buffer;
    buffer << manifest.rdbuf();
    const std::string src = buffer.str();
    JsonScanner sc(src);

    if (!sc.consume('{')) return false;

    // Stage results into a fresh runtime; commit only after full success.
    TripleWorldRuntime staging;
    MainWorldSnapshotMetadata meta;

    while (!sc.peek('}')) {
        const std::string key = sc.readString();
        if (!sc.consume(':')) return false;
        if (key == "snapshot_id") {
            meta.snapshot_id = sc.readUint();
        } else if (key == "sim_time_seconds") {
            meta.sim_time_seconds = sc.readNumber();
        } else if (key == "generations_elapsed") {
            meta.generations_elapsed = static_cast<std::size_t>(sc.readUint());
        } else if (key == "scenario_name") {
            meta.scenario_name = sc.readString();
        } else if (key == "archetype_progression") {
            // Stone Bootstrap commit 3: array of {fire,worksite,stone,
            // heavy,fire2} bool objects, one per archetype index.
            if (!sc.consume('[')) return false;
            while (!sc.peek(']')) {
                if (!sc.consume('{')) return false;
                ArchetypeProgressionState s{};
                while (!sc.peek('}')) {
                    const std::string sub = sc.readString();
                    if (!sc.consume(':')) return false;
                    if      (sub == "first_fire_ignited")   s.first_fire_ignited  = sc.readBool();
                    else if (sub == "worksite_unlocked")    s.worksite_unlocked   = sc.readBool();
                    else if (sub == "stone_unlocked")       s.stone_unlocked      = sc.readBool();
                    else if (sub == "heavy_unlocked")       s.heavy_unlocked      = sc.readBool();
                    else if (sub == "second_fire_ignited")  s.second_fire_ignited = sc.readBool();
                    else                                    sc.skipValue();
                    sc.consume(',');
                }
                sc.consume('}');
                meta.archetype_progression.push_back(s);
                sc.consume(',');
            }
            sc.consume(']');
        } else if (key == "borderland") {
            if (!loadSlot(sc, persist_dir, "borderland", staging.borderland())) {
                return false;
            }
        } else if (key == "main_world") {
            if (!loadSlot(sc, persist_dir, "main_world", staging.mainWorld())) {
                return false;
            }
        } else {
            sc.skipValue();
        }
        sc.consume(',');
    }

    // Commit: move pools from staging into the caller's runtime.
    triple_world.borderland().pools_by_archetype =
        std::move(staging.borderland().pools_by_archetype);
    triple_world.mainWorld().pools_by_archetype =
        std::move(staging.mainWorld().pools_by_archetype);
    if (out_meta != nullptr) {
        *out_meta = meta;
    }
    return true;
}

}  // namespace neuro::routes::protagonist
