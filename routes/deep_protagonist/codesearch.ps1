param([string]$pat)
$root="D:\claude-code\c++\routes\deep_protagonist"
$files=Get-ChildItem -Path "$root\src","$root\include" -Recurse -Include *.cpp,*.hpp -EA SilentlyContinue
$hits=$files | Select-String -Pattern $pat
foreach($h in $hits){
  $rel = $h.Path.Replace("$root\","")
  Write-Output ($rel + ":" + $h.LineNumber + ": " + $h.Line.Trim())
}
Write-Output ("--- total hits: " + $hits.Count)
