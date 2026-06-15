@echo off
rem D-136 one-click long run: fresh evolution, 160 generations,
rem episodes_per_generation=3 (selection noise reduction, see knowledge k75).
set PATH=D:\NVIDIA\CUDA\v12.8\bin;%PATH%
cd /d D:\claude-code\c++\routes\protagonist_ecology
D:\claude-code\c++\build_ninja_cuda\bin\neural-eco-protagonist-batched.exe ^
  --config d136_longrun.toml > d136_longrun.log 2>&1
