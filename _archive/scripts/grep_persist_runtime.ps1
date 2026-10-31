cd D:\claude-code\c++\routes\protagonist_ecology
$f = "runtime/ProtagonistEcologyRuntime.cpp"
Write-Host "=== persist_dir / MainWorldPersistence usage in runtime ==="
Select-String -Path $f -Pattern 'persist_dir|MainWorldPersistence|::load|::save|generations_elapsed|snapshot_id' | ForEach-Object { $_.LineNumber.ToString() + ': ' + $_.Line.Trim() }
Write-Host ""
Write-Host "=== TripleWorldRuntime.cpp persistence usage ==="
Select-String -Path "runtime/TripleWorldRuntime.cpp" -Pattern 'MainWorldPersistence|persist_dir|::load|::save' | ForEach-Object { $_.LineNumber.ToString() + ': ' + $_.Line.Trim() }
