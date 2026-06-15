$files = @(
  'D:\claude-code\c++\routes\protagonist_ecology\onepot_sD_warm_50001.toml',
  'D:\claude-code\c++\routes\protagonist_ecology\onepot_sD_warm_50002.toml',
  'D:\claude-code\c++\routes\protagonist_ecology\onepot_sD_warm_50003.toml'
)
foreach ($f in $files) {
  $c = Get-Content -Raw $f
  $c2 = $c -replace '(?m)^action_dim = 16\s*$', 'action_dim = 17'
  Set-Content -NoNewline -Path $f -Value $c2
  $line = (Select-String -Path $f -Pattern '^action_dim').Line
  Write-Output ($f + "  ->  " + $line)
}
