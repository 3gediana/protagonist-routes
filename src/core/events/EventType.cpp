#include "core/events/EventType.h"

namespace neuro::events {

const char* toString(EventType type) {
    switch (type) {
        case EventType::None:                   return "None";
        case EventType::Shutdown:               return "Shutdown";

        case EventType::TickStarted:            return "TickStarted";
        case EventType::TickEnded:              return "TickEnded";
        case EventType::GenerationStarted:      return "GenerationStarted";
        case EventType::GenerationEnded:        return "GenerationEnded";

        case EventType::AgentBorn:              return "AgentBorn";
        case EventType::AgentDied:              return "AgentDied";
        case EventType::AgentMoved:             return "AgentMoved";
        case EventType::AgentAteFood:           return "AgentAteFood";
        case EventType::AgentEnergyChanged:     return "AgentEnergyChanged";

        case EventType::ResourceSpawned:        return "ResourceSpawned";
        case EventType::ResourceConsumed:       return "ResourceConsumed";

        case EventType::LanguageSwitched:       return "LanguageSwitched";
        case EventType::UserPaused:             return "UserPaused";
        case EventType::UserResumed:            return "UserResumed";
        case EventType::UserSpeedChanged:       return "UserSpeedChanged";

        case EventType::SpeciesFormed:          return "SpeciesFormed";
        case EventType::SpeciesExtinct:         return "SpeciesExtinct";
        case EventType::MutationOccurred:       return "MutationOccurred";

        case EventType::DecisionTriggered:      return "DecisionTriggered";

        case EventType::TerrainGenerated:       return "TerrainGenerated";
        case EventType::TerrainModified:        return "TerrainModified";
        case EventType::AgentEnteredCave:       return "AgentEnteredCave";
        case EventType::AgentLeftCave:          return "AgentLeftCave";

        case EventType::AgentEmittedSignal:     return "AgentEmittedSignal";
        case EventType::AgentAttacked:          return "AgentAttacked";
        case EventType::AgentFled:              return "AgentFled";
        case EventType::ObjectPickedUp:         return "ObjectPickedUp";
        case EventType::ObjectDropped:          return "ObjectDropped";
        case EventType::MatingOccurred:         return "MatingOccurred";
        case EventType::InterventionTriggered:  return "InterventionTriggered";

        case EventType::UniverseForked:         return "UniverseForked";
        case EventType::UniverseLoaded:         return "UniverseLoaded";

        case EventType::ChopStarted:               return "ChopStarted";
        case EventType::ChopCompleted:             return "ChopCompleted";
        case EventType::ChopAbandoned:             return "ChopAbandoned";
        case EventType::BuildStarted:              return "BuildStarted";
        case EventType::BuildCompleted:            return "BuildCompleted";
        case EventType::WorksiteMaterialDeposited: return "WorksiteMaterialDeposited";
        case EventType::CreditAssigned:            return "CreditAssigned";

        case EventType::Count:                  return "Count";
    }
    return "Unknown";
}

}  // namespace neuro::events
