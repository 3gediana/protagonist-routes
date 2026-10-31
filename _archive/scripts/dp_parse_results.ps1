$dp='D:\claude-code\c++\routes\deep_protagonist\runs'
foreach($f in Get-ChildItem $dp -File -Filter 'ft_hyp_*_bg.jsonl'){
  $rows=@()
  Get-Content $f.FullName | ForEach-Object { try { $o=$_ | ConvertFrom-Json; $unlockCount=($o.unlocks | Where-Object {$_ -eq 1}).Count; $rows += [pscustomobject]@{Episode=$o.episode; Steps=$o.steps; Reward=[double]$o.reward; Score=[double]$o.score; Unlocks=$unlockCount; Deaths=$o.deaths; Cold=$o.deaths_cold; FoodDeath=$o.deaths_food; Collect=$o.collect_total; Explored=$o.explored_m; Raw=$_} } catch {} }
  if($rows.Count -eq 0){ continue }
  $bestUnlock=$rows | Sort-Object Unlocks,Reward -Descending | Select-Object -First 1
  $bestReward=$rows | Sort-Object Reward -Descending | Select-Object -First 1
  $last=$rows[-1]
  [pscustomobject]@{File=$f.Name; Episodes=$rows.Count; BestUnlocks=$bestUnlock.Unlocks; BestUnlockEp=$bestUnlock.Episode; BestReward=[math]::Round($bestReward.Reward,2); BestRewardEp=$bestReward.Episode; LastUnlocks=$last.Unlocks; LastScore=$last.Score; LastDeaths=$last.Deaths; LastFoodDeaths=$last.FoodDeath; LastColdDeaths=$last.Cold } | Format-List
}
