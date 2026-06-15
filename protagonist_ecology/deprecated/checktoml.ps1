Get-ChildItem 'D:\claude-code\c++\routes\protagonist_ecology\onepot_sD_warm_5000*.toml' | ForEach-Object {
  $name = $_.Name
  Select-String -Path $_.FullName -Pattern '^action_dim','^generations','^random_seed','resume_checkpoint_path' | ForEach-Object {
    Write-Output ($name + ' | ' + $_.Line)
  }
  Write-Output '---'
}
