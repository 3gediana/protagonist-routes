$peRuns='D:\claude-code\c++\routes\protagonist_ecology\data\runs'
function SizeGB($items){ $s=($items | Measure-Object Length -Sum).Sum; if($null -eq $s){$s=0}; [math]::Round($s/1GB,2) }
$all=Get-ChildItem $peRuns -Recurse -File -Force -ErrorAction SilentlyContinue
[pscustomobject]@{Category='checkpoint files'; GB=SizeGB($all | ?{$_.FullName -match 'checkpoint_gen'})}
[pscustomobject]@{Category='trace files'; GB=SizeGB($all | ?{$_.FullName -match '\\trace\\'})}
[pscustomobject]@{Category='csv/json/log/bin noncheckpoint nontrace'; GB=SizeGB($all | ?{$_.FullName -notmatch 'checkpoint_gen' -and $_.FullName -notmatch '\\trace\\'})}
Write-Output '=== Trace dirs top ==='
Get-ChildItem $peRuns -Recurse -Directory -Filter 'trace' -ErrorAction SilentlyContinue | ForEach-Object { $s=SizeGB(Get-ChildItem $_.FullName -Recurse -File -Force -ErrorAction SilentlyContinue); [pscustomobject]@{Path=$_.FullName; GB=$s} } | Sort-Object GB -Descending | Select-Object -First 20 | Format-Table -AutoSize
Write-Output '=== Onepot warm seed sizes ==='
Get-ChildItem $peRuns -Directory | ForEach-Object { $files=Get-ChildItem $_.FullName -Recurse -File -Force -ErrorAction SilentlyContinue; [pscustomobject]@{Seed=$_.Name; GB=SizeGB($files); CheckpointGB=SizeGB($files|?{$_.FullName -match 'checkpoint_gen'}); TraceGB=SizeGB($files|?{$_.FullName -match '\\trace\\'}); OtherGB=SizeGB($files|?{$_.FullName -notmatch 'checkpoint_gen' -and $_.FullName -notmatch '\\trace\\'})} } | Sort-Object GB -Descending | Format-Table -AutoSize
