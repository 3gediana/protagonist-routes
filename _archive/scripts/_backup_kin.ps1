$r = "D:\claude-code\c++\routes\protagonist_ecology\"
$b = ".bak_kin_20260603"
$files = @(
  "brain\ProtagonistBrain.h",
  "brain\ProtagonistBrain.cpp",
  "perception\KinPerception.h",
  "perception\KinPerception.cpp",
  "runtime\BootstrapPopulation.cpp",
  "main.cpp"
)
foreach ($f in $files) {
  $src = $r + $f
  $dst = $r + $f + $b
  if (Test-Path $src) {
    Copy-Item $src $dst -Force
    Write-Output ("backed up " + $f)
  } else {
    Write-Output ("MISSING " + $f)
  }
}
