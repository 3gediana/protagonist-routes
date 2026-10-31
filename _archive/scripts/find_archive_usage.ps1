$root='D:\claude-code\c++\routes\protagonist_ecology'
Get-ChildItem $root -Recurse -File -Include *.cpp,*.h | Select-String -Pattern 'QualityDiversityArchive|reproduceFromArchive|me_neat|archive\.submit|archive\.clear|eliteIds\(' | ForEach-Object { "{0}:{1}: {2}" -f $_.Path,$_.LineNumber,$_.Line.Trim() }
