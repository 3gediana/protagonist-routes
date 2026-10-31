$ErrorActionPreference='SilentlyContinue'
$pe='D:\claude-code\c++\routes\protagonist_ecology'
Get-ChildItem $pe -Recurse -File -Include '*.h','*.hpp','*.cpp' | Where-Object { $_.FullName -notmatch '\\data\\runs\\|\\build' } | Select-String -Pattern 'class QualityDiversityArchive|struct BehaviorDescriptor|BehaviorDescriptor|QualityDiversityArchive' | Select-Object Path,LineNumber,Line | Format-List
