param([switch]$Execute)
$runsDir = "D:\claude-code\c++\routes\deep_protagonist\runs"
# KEEP (never delete here): all .pt/.opt model checkpoints EXCEPT the smoke throwaway,
# and the current bloodline metrics r1_cook_s24*.jsonl (the empirical record).
# DELETE: console logs (.log/.err/.stderr/.stdout), stale non-bloodline .jsonl,
#         and the smoke_ew_DELETEME throwaway checkpoint pair.
$top = Get-ChildItem $runsDir -File -Force
$del = @()
foreach ($f in $top) {
    $n = $f.Name
    $ext = $f.Extension.ToLower()
    if ($n -like "smoke_ew_DELETEME.*") { $del += $f; continue }
    if ($ext -in ".log",".err",".stderr",".stdout") { $del += $f; continue }
    if ($ext -eq ".jsonl" -and ($n -notlike "r1_cook_s24*")) { $del += $f; continue }
}
$tot = 0
foreach ($f in $del) {
    $tot += $f.Length
    Write-Host ("  -" + $f.Name + "  " + [math]::Round($f.Length/1KB,1) + "KB")
    if ($Execute) { Remove-Item -LiteralPath $f.FullName -Force }
}
$tag = ""; if (-not $Execute) { $tag = "  [DRY RUN]" }
Write-Host ("files: " + $del.Count + "   reclaimed: " + [math]::Round($tot/1MB,2) + " MB" + $tag)
