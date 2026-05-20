set(PROTAGONIST_ECOLOGY_ROUTE_SOURCES
    capability/ChopTreeCapability.cpp
    capability/CraftSpearCapability.cpp
    capability/DrinkWaterCapability.cpp
    capability/NurseryPickUpCapability.cpp
    capability/ProtagonistHuntCapability.cpp
    capability/ThrowCarriedObjectCapability.cpp
    brain/MemoryBank.cpp
    brain/ProtagonistBrain.cpp
    brain/ProtagonistGenome.cpp
    perception/AgentSocialPerception.cpp
    perception/BasePerception.cpp
    perception/CarriedObjectPerception.cpp
    perception/DangerPerception.cpp
    perception/ObjectLogisticsPerception.cpp
    perception/StockpilePerception.cpp
    perception/SurvivalStatePerception.cpp
    perception/TreePerception.cpp
    perception/WaterPerception.cpp
    perception/WeaponPerception.cpp
    perception/WorksitePerception.cpp
    runtime/BootstrapPopulation.cpp
    runtime/ProtagonistEcologyConfig.cpp
    runtime/ProtagonistEvolution.cpp
    runtime/ProtagonistEcologyRuntime.cpp
    runtime/ProtagonistWorldFactory.cpp
    runtime/TraceRecorder.cpp
    world/BaseLayer.cpp
    world/CampfireLayer.cpp
    world/InterventionDirectorLayer.cpp
    world/NurseryLogisticsLayer.cpp
    world/NurseryProgressionLayer.cpp
    world/NurserySupplyLayer.cpp
    world/SandboxStatusLayer.cpp
    world/SurvivalStatusLayer.cpp
    world/TreeLayer.cpp
    world/WaterLayer.cpp
    world/WorksiteLayer.cpp
)

set(PROTAGONIST_ECOLOGY_ROUTE_HEADERS
    capability/ChopTreeCapability.h
    capability/CraftSpearCapability.h
    capability/DrinkWaterCapability.h
    capability/NurseryPickUpCapability.h
    capability/ProtagonistHuntCapability.h
    capability/ThrowCarriedObjectCapability.h
    brain/MemoryBank.h
    brain/ProtagonistBrain.h
    brain/ProtagonistGenome.h
    perception/AgentSocialPerception.h
    perception/BasePerception.h
    perception/CarriedObjectPerception.h
    perception/DangerPerception.h
    perception/ObjectLogisticsPerception.h
    perception/StockpilePerception.h
    perception/SurvivalStatePerception.h
    perception/TreePerception.h
    perception/WaterPerception.h
    perception/WeaponPerception.h
    perception/WorksitePerception.h
    runtime/BootstrapPopulation.h
    runtime/ProtagonistEcologyConfig.h
    runtime/ProtagonistEvolution.h
    runtime/ProtagonistEcologyRuntime.h
    runtime/ProtagonistWorldFactory.h
    runtime/TraceRecorder.h
    world/BaseLayer.h
    world/CampfireLayer.h
    world/InterventionDirectorLayer.h
    world/NurseryLogisticsLayer.h
    world/NurseryProgressionLayer.h
    world/NurserySupplyLayer.h
    world/SandboxStatusLayer.h
    world/SurvivalStatusLayer.h
    world/TreeLayer.h
    world/WaterLayer.h
    world/WorksiteLayer.h
)

add_library(neural_eco_route_protagonist STATIC
    ${PROTAGONIST_ECOLOGY_ROUTE_SOURCES}
    ${PROTAGONIST_ECOLOGY_ROUTE_HEADERS}
)
add_library(neural_eco::route_protagonist ALIAS neural_eco_route_protagonist)

target_include_directories(neural_eco_route_protagonist
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

target_link_libraries(neural_eco_route_protagonist
    PUBLIC
        neural_eco::core
        neural_eco::runtime_headless
)

target_compile_features(neural_eco_route_protagonist PUBLIC cxx_std_20)

add_executable(neural-eco-protagonist main.cpp)
target_link_libraries(neural-eco-protagonist PRIVATE neural_eco::route_protagonist)

set_target_properties(neural-eco-protagonist PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
