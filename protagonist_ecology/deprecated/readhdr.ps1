param([string]$Path)
$b = [IO.File]::ReadAllBytes($Path)
Write-Output ("file=" + $Path + " bytes=" + $b.Length)
Write-Output ("magic = 0x" + ([BitConverter]::ToUInt32($b,0)).ToString('X8'))
Write-Output ("version(u32@4) = " + [BitConverter]::ToUInt32($b,4))
# dump u64 values from offset 8 onward
for ($off = 8; $off -le 72; $off += 8) {
    $v = [BitConverter]::ToUInt64($b, $off)
    Write-Output ("u64@" + $off + " = " + $v)
}
