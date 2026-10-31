$dp='D:\claude-code\c++\routes\deep_protagonist\runs'
foreach($tag in @('entropy_n1','huntfocus_n1','aggressive_n1')){
  Write-Output "=== ft_hyp_$tag stdout tail 80 ==="
  Get-Content "$dp\ft_hyp_$tag.stdout.log" -Tail 80 -ErrorAction SilentlyContinue
  Write-Output "=== ft_hyp_$tag stderr ==="
  Get-Content "$dp\ft_hyp_$tag.stderr.log" -Tail 40 -ErrorAction SilentlyContinue
}
Write-Output '=== last saves ==='
Get-ChildItem $dp -Filter 'ft_hyp_*n1*' | Select-Object Name,Length,LastWriteTime | Format-Table -AutoSize
