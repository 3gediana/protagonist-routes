#include "brain/ProtagonistGenome.h"

#include <cstdint>
#include <fstream>
#include <random>

namespace neuro::routes::protagonist {

namespace {

constexpr std::uint32_t kProtagonistGenomeMagic = 0x50474E4D;  // 'PGNM'
constexpr std::uint32_t kProtagonistGenomeVersion = 2;

void writeBuffer(std::ofstream& stream, const std::vector<float>& buffer) {
    const std::uint64_t count = buffer.size();
    stream.write(reinterpret_cast<const char*>(&count), sizeof(count));
    if (count > 0) {
        stream.write(reinterpret_cast<const char*>(buffer.data()), static_cast<std::streamsize>(count * sizeof(float)));
    }
}

bool readBuffer(std::ifstream& stream, std::vector<float>& buffer) {
    std::uint64_t count = 0;
    stream.read(reinterpret_cast<char*>(&count), sizeof(count));
    if (!stream) {
        return false;
    }
    if (count != buffer.size()) {
        return false;
    }
    if (count > 0) {
        stream.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(count * sizeof(float)));
    }
    return static_cast<bool>(stream);
}

}  // namespace

namespace {

void fillRandom(std::vector<float>& data, std::mt19937& rng, float scale = 0.35f) {
    std::normal_distribution<float> dist(0.0f, scale);
    for (float& value : data) {
        value = dist(rng);
    }
}

}  // namespace

ProtagonistGenome::ProtagonistGenome(GenomeId id,
                                     std::size_t input_dim,
                                     std::size_t hidden_dim,
                                     std::size_t memory_cells,
                                     std::size_t memory_slots,
                                     std::size_t read_heads,
                                     std::size_t action_dim)
    : id_(id),
      input_dim_(input_dim),
      hidden_dim_(hidden_dim),
      memory_cells_(memory_cells),
      memory_slots_(memory_slots),
      read_heads_(read_heads),
      action_dim_(action_dim),
      input_hidden_weights_((input_dim_ + memory_cells_ * read_heads_) * hidden_dim_, 0.0f),
      hidden_bias_(hidden_dim_, 0.0f),
      hidden_action_weights_(hidden_dim_ * action_dim_, 0.0f),
      action_bias_(action_dim_, 0.0f),
      hidden_interface_weights_(hidden_dim_ * interfaceDim(), 0.0f),
      interface_bias_(interfaceDim(), 0.0f) {}

GenomeId ProtagonistGenome::id() const {
    return id_;
}

std::unique_ptr<IGenome> ProtagonistGenome::clone() const {
    return std::make_unique<ProtagonistGenome>(*this);
}

std::size_t ProtagonistGenome::complexity() const {
    return parameterCount();
}

void ProtagonistGenome::setId(GenomeId id) {
    id_ = id;
}

std::size_t ProtagonistGenome::parameterCount() const {
    return input_hidden_weights_.size()
         + hidden_bias_.size()
         + hidden_action_weights_.size()
         + action_bias_.size()
         + hidden_interface_weights_.size()
         + interface_bias_.size();
}

std::unique_ptr<ProtagonistGenome> ProtagonistGenome::random(GenomeId id,
                                                             std::size_t input_dim,
                                                             std::size_t hidden_dim,
                                                             std::size_t memory_cells,
                                                             std::size_t memory_slots,
                                                             std::size_t read_heads,
                                                             std::size_t action_dim,
                                                             std::mt19937& rng) {
    auto genome = std::make_unique<ProtagonistGenome>(id, input_dim, hidden_dim, memory_cells, memory_slots, read_heads, action_dim);
    fillRandom(genome->input_hidden_weights_, rng);
    fillRandom(genome->hidden_bias_, rng);
    fillRandom(genome->hidden_action_weights_, rng);
    fillRandom(genome->action_bias_, rng);
    fillRandom(genome->hidden_interface_weights_, rng);
    fillRandom(genome->interface_bias_, rng);
    return genome;
}

bool ProtagonistGenome::saveToFile(const std::filesystem::path& path) const {
    std::error_code ec;
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path(), ec);
    }
    std::ofstream stream(path, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!stream) {
        return false;
    }

    stream.write(reinterpret_cast<const char*>(&kProtagonistGenomeMagic), sizeof(kProtagonistGenomeMagic));
    stream.write(reinterpret_cast<const char*>(&kProtagonistGenomeVersion), sizeof(kProtagonistGenomeVersion));

    const std::uint64_t input_dim = input_dim_;
    const std::uint64_t hidden_dim = hidden_dim_;
    const std::uint64_t memory_cells = memory_cells_;
    const std::uint64_t memory_slots = memory_slots_;
    const std::uint64_t read_heads = read_heads_;
    const std::uint64_t action_dim = action_dim_;
    const std::uint64_t genome_id = id_.value;
    stream.write(reinterpret_cast<const char*>(&genome_id), sizeof(genome_id));
    stream.write(reinterpret_cast<const char*>(&input_dim), sizeof(input_dim));
    stream.write(reinterpret_cast<const char*>(&hidden_dim), sizeof(hidden_dim));
    stream.write(reinterpret_cast<const char*>(&memory_cells), sizeof(memory_cells));
    stream.write(reinterpret_cast<const char*>(&memory_slots), sizeof(memory_slots));
    stream.write(reinterpret_cast<const char*>(&read_heads), sizeof(read_heads));
    stream.write(reinterpret_cast<const char*>(&action_dim), sizeof(action_dim));

    writeBuffer(stream, input_hidden_weights_);
    writeBuffer(stream, hidden_bias_);
    writeBuffer(stream, hidden_action_weights_);
    writeBuffer(stream, action_bias_);
    writeBuffer(stream, hidden_interface_weights_);
    writeBuffer(stream, interface_bias_);

    return static_cast<bool>(stream);
}

std::unique_ptr<ProtagonistGenome> ProtagonistGenome::loadFromFile(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::in | std::ios::binary);
    if (!stream) {
        return nullptr;
    }

    std::uint32_t magic = 0;
    std::uint32_t version = 0;
    stream.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    stream.read(reinterpret_cast<char*>(&version), sizeof(version));
    if (!stream || magic != kProtagonistGenomeMagic || (version != 1 && version != kProtagonistGenomeVersion)) {
        return nullptr;
    }

    std::uint64_t genome_id = 0;
    std::uint64_t input_dim = 0;
    std::uint64_t hidden_dim = 0;
    std::uint64_t memory_cells = 0;
    std::uint64_t memory_slots = 0;
    std::uint64_t read_heads = 0;
    std::uint64_t action_dim = 0;
    stream.read(reinterpret_cast<char*>(&genome_id), sizeof(genome_id));
    stream.read(reinterpret_cast<char*>(&input_dim), sizeof(input_dim));
    stream.read(reinterpret_cast<char*>(&hidden_dim), sizeof(hidden_dim));
    stream.read(reinterpret_cast<char*>(&memory_cells), sizeof(memory_cells));
    if (version >= 2) {
        stream.read(reinterpret_cast<char*>(&memory_slots), sizeof(memory_slots));
        stream.read(reinterpret_cast<char*>(&read_heads), sizeof(read_heads));
    }
    stream.read(reinterpret_cast<char*>(&action_dim), sizeof(action_dim));
    if (!stream) {
        return nullptr;
    }

    if (version < 2) {
        memory_slots = 64;
        read_heads = 4;
    }

    auto genome = std::make_unique<ProtagonistGenome>(
        GenomeId{genome_id},
        static_cast<std::size_t>(input_dim),
        static_cast<std::size_t>(hidden_dim),
        static_cast<std::size_t>(memory_cells),
        static_cast<std::size_t>(memory_slots),
        static_cast<std::size_t>(read_heads),
        static_cast<std::size_t>(action_dim));

    if (!readBuffer(stream, genome->input_hidden_weights_)) return nullptr;
    if (!readBuffer(stream, genome->hidden_bias_)) return nullptr;
    if (!readBuffer(stream, genome->hidden_action_weights_)) return nullptr;
    if (!readBuffer(stream, genome->action_bias_)) return nullptr;
    if (!readBuffer(stream, genome->hidden_interface_weights_)) return nullptr;
    if (!readBuffer(stream, genome->interface_bias_)) return nullptr;

    return genome;
}

}  // namespace neuro::routes::protagonist
