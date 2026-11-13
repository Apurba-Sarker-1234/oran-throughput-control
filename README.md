# O-RAN throughput control (ALS 5754 project)

## Setup (ns-3.42 + ns3-oran)
    wget https://www.nsnam.org/releases/ns-allinone-3.42.tar.bz2 && tar xjf it   # -> ns-3.42
    git clone https://github.com/usnistgov/ns3-oran.git ns-3.42/contrib/oran
    cd ns-3.42 && patch -p1 < ../patches/pf-ff-mac-scheduler-maxdlprb.patch     # runtime PRB cap in PfFfMacScheduler
    (cd contrib/oran && git apply ../../../patches/ns3-oran-prb-control.patch)   # throughput reporter, set-PRB command, open-loop LM
    ./ns3 configure -d optimized --enable-modules "lte;oran;applications;mobility;point-to-point;internet;flow-monitor;config-store"
    cp ../ns3/scenario_single_cell.cc scratch/scn/ ; ./ns3 build
Patches are relative to ns-3.42 (scheduler) and to the oran checkout (oran).

## Input mapping (RBG rounding is part of the plant)
nominal {6,15,25,50,75,100} -> effective {8,16,24,52,76,100} (configs/prb_mapping.json). All identification/design uses effective PRBs.

## Reproduce (Phase 0-1)
    python3 python/phase0_plot.py          # figures/phase0_prb_step_check.png
    python3 python/dynamics_analysis.py    # supplementary 10 ms dynamics check (UDP vs TCP), data from ns3/dynamics_check.cc
    python3 python/run_experiments.py phase1 --jobs 12   # 78 ns-3 runs -> data/raw/<category>/*.csv (+ .meta.json, .latency.csv, .lmrun.csv, .e2.db); skips existing; ~40 min
    python3 python/phase1_report.py        # static maps, level throughputs, data inventory, latency table -> data/processed, report/tables
    python3 python/plot_phase1.py          # step_response, id_example, model_failure_cases figures

## Conventions
* Log columns: time_s,k,prb_applied(effective),prb_requested(effective),thr_total_mbps,thr_ue_i,offered_load_mbps,policy,seed,prb_nominal. Row k is the 100 ms window ending at time_s.
* Timing: report grid and log windows at k*T_s; LM polls 5 ms later; command applied 1 ms after the poll (+ `--e2Latency` * T_s at the eNB terminator).
* ID data: offered load 120 Mbps (> capacity), random level every 2-5 s (multiples of T_s) among the six levels.
* Backlog-drain runs (offered 16/40 Mbps) are model-failure cases and must not be used for fitting.
* UDP CBR with a deterministic channel: seeds only change the random schedules (and RA timing), not channel noise.
* Project is not a git repository; run metadata records patch hashes and binary hash instead of a commit.
