# -*- coding: utf-8 -*-
# CRLF-safe patch: add a night-specific (low) trigger_oversample channel.
import io, sys
NL = "\r\n"
def L(*lines): return NL.join(lines)

base = r"D:\claude-code\c++\routes\deep_protagonist"
bc = base + r"\include\policy\BCTrainer.hpp"
mt = base + r"\src\main_train.cpp"

def patch(path, edits):
    data = io.open(path, "r", encoding="utf-8", newline="").read()
    for old, new, n in edits:
        c = data.count(old)
        if c != n:
            print("ABORT %s: expected %d got %d for:\n%r" % (path, n, c, old[:80])); sys.exit(1)
        data = data.replace(old, new)
    io.open(path, "w", encoding="utf-8", newline="").write(data)
    print("patched", path)

# ---- BCTrainer.hpp ----
bc_edits = [
 (L("    int   seed                  = 1234;","};"),
  L("    int   seed                  = 1234;",
    "    std::vector<std::string> low_os_demos;     // subset of demo_paths using low_trigger_oversample",
    "    int   low_trigger_oversample = -1;         // <0 = unused; else trigger_oversample for low_os_demos (night downweight)",
    "};"), 1),
 (L("    BCConfig cfg_;","    std::vector<float>   obs_;"),
  L("    BCConfig cfg_;",
    "    int                  eff_trig_os_ = 0;   // effective trigger_oversample for the file being loaded",
    "    std::vector<float>   obs_;"), 1),
 (L("    // Scan [ep_start, ep_end) for non-NOOP (trigger) events and replicate a"),
  L("    bool is_low_os_(const std::string& p) const {",
    "        for (const auto& q : cfg_.low_os_demos) if (q == p) return true;",
    "        return false;",
    "    }",
    "",
    "    // Scan [ep_start, ep_end) for non-NOOP (trigger) events and replicate a"), 1),
 (L("        for (const auto& path : cfg_.demo_paths) {",
    "            std::FILE* f = std::fopen(path.c_str(), \"rb\");"),
  L("        for (const auto& path : cfg_.demo_paths) {",
    "            eff_trig_os_ = (cfg_.low_trigger_oversample >= 0 && is_low_os_(path))",
    "                           ? cfg_.low_trigger_oversample : cfg_.trigger_oversample;",
    "            std::FILE* f = std::fopen(path.c_str(), \"rb\");"), 1),
 ("        if (cfg_.trigger_oversample <= 0) return;",
  "        if (eff_trig_os_ <= 0) return;", 1),
 ("            for (int r = 0; r < cfg_.trigger_oversample; ++r)",
  "            for (int r = 0; r < eff_trig_os_; ++r)", 1),
]
patch(bc, bc_edits)

# ---- main_train.cpp ----
mt_edits = [
 ("    std::string bc_demos;                  // comma-separated demo files for --policy bc",
  L("    std::string bc_demos;                  // comma-separated demo files for --policy bc",
    "    std::string bc_low_os_demos;           // subset of bc_demos using bc_low_os (night downweight)",
    "    int         bc_low_os    = -1;         // <0 = unused"), 1),
 ('        else if (a == "--bc-trigger-oversample") bc_trig_os = std::stoi(next());',
  L('        else if (a == "--bc-trigger-oversample") bc_trig_os = std::stoi(next());',
    '        else if (a == "--bc-low-os")    bc_low_os    = std::stoi(next());',
    '        else if (a == "--bc-low-os-demos") bc_low_os_demos = next();'), 1),
 ("        bc.trigger_oversample = bc_trig_os;",
  L("        bc.trigger_oversample = bc_trig_os;",
    "        bc.low_os_demos       = split_csv(bc_low_os_demos);",
    "        bc.low_trigger_oversample = bc_low_os;"), 1),
]
patch(mt, mt_edits)
print("ALL OK")
