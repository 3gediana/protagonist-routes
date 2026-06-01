#pragma once

#include "runtime/ProtagonistEcologyConfig.h"

#include <memory>

namespace neuro {
class World2D;
}

namespace neuro::routes::protagonist {

std::unique_ptr<World2D> createBootstrapWorld(const ProtagonistEcologyConfig& config);

}  // namespace neuro::routes::protagonist
