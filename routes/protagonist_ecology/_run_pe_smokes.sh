#!/bin/bash
# Sequential PE smoke queue (priority order), skip runs already at 30 gens.
cd "D:/claude-code/c++/routes/protagonist_ecology"
EXE="/d/claude-code/c++/build_ninja_cuda/bin/neural-eco-protagonist-batched.exe"
count_rows() {
  local rd
  rd=$(grep -oP 'runs_dir\s*=\s*"\K[^"]+' "$1.toml" 2>/dev/null)
  [ -z "$rd" ] && { echo 0; return; }
  local latest
  latest=$(ls -d ${rd}/2*/ 2>/dev/null | sort | tail -1)
  [ -z "$latest" ] && { echo 0; return; }
  python -c "import csv,sys; print(sum(1 for _ in csv.DictReader(open(sys.argv[1]))))" "$latest/fitness.csv" 2>/dev/null || echo 0
}
for cfg in pe_smoke_b_huntbd_50031 pe_smoke_c_mech_50031 pe_smoke_a_base_50031 \
           pe_smoke_b_huntbd_50032 pe_smoke_c_mech_50032 pe_smoke_a_base_50032; do
  n=$(count_rows "$cfg")
  if [ "$n" -ge 30 ]; then echo "[skip] $cfg already has $n gens"; continue; fi
  echo "[run ] $cfg (existing rows: $n)  start=$(date +%H:%M:%S)"
  "$EXE" --config "$cfg.toml" > "$cfg.log" 2>&1
  rc=$?
  echo "[done] $cfg exit=$rc rows=$(count_rows "$cfg")  end=$(date +%H:%M:%S)"
done
echo "ALL_PE_SMOKES_DONE"
