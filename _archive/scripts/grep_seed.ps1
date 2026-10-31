cd D:\claude-code\c++\routes\protagonist_ecology
Write-Host "=== seedFromCandidates call sites (TripleWorldRuntime.cpp) ==="
Select-String -Path "runtime/TripleWorldRuntime.cpp" -Pattern 'seedFromCandidates|mainWorld\(\)|borderland\(\)|sandbox|initialPopulation|seedSandbox|reseed' | ForEach-Object { $_.LineNumber.ToString() + ': ' + $_.Line.Trim() }
Write-Host ""
Write-Host "=== runtime: how initial NEAT population / sandbox seeded from pools ==="
Select-String -Path "runtime/ProtagonistEcologyRuntime.cpp" -Pattern 'seedFromCandidates|seed_main_world|main_world.*sandbox|sandbox.*main_world|reseed|seedSandbox|population_seed|initial.*pool|pool.*initial' | ForEach-Object { $_.LineNumber.ToString() + ': ' + $_.Line.Trim() }
Write-Host ""
Write-Host "=== config fields linking main_world pool -> sandbox seeding ==="
Select-String -Path "runtime/ProtagonistEcologyConfig.h" -Pattern 'seed.*main|main.*seed|seed_protagonists_from|sandbox_seed|reseed|inject' | ForEach-Object { $_.LineNumber.ToString() + ': ' + $_.Line.Trim() }
