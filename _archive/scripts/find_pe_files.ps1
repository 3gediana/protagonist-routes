$root='D:\claude-code\c++\routes\protagonist_ecology'
Get-ChildItem $root -Recurse -File -Include QualityDiversityArchive.h,QualityDiversityArchive.cpp,ProtagonistEvolution.cpp,ProtagonistEvolution.h | ForEach-Object { $_.FullName }
