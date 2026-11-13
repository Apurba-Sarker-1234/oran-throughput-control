import os, sys, numpy as np, matplotlib; matplotlib.use("Agg"); import matplotlib.pyplot as plt, warnings
warnings.filterwarnings("ignore"); sys.path.insert(0, os.path.dirname(__file__)); from io_utils import ROOT, load_run
R = f"{ROOT}/data/raw"; F = f"{ROOT}/figures"
# 1) step responses vs actuation latency (T_s = 100 ms windows)
fig, axs = plt.subplots(1, 2, figsize=(7.2, 2.8), sharey=False)
for L in (0, 1, 2):
    d = load_run(f"{R}/latency_check/latency_check_L{L}.csv")
    for ax, t0, ttl in zip(axs, (10, 20), ("100→24 PRB (nominal 25)", "24→100 PRB")):
        w = d[(d.time_s > t0 - 0.55) & (d.time_s < t0 + 1.05)]; ax.step(w.time_s - t0, w.thr_total_mbps, where="post", label=f"latency {L} T_s"); ax.set_title(ttl, fontsize=8)
for ax in axs: ax.axvline(0, c="k", lw=.5); ax.set_xlabel("time since command schedule step (s)"); ax.grid(alpha=.3)
axs[0].set_ylabel("throughput (Mbps), 100 ms windows"); axs[0].legend(fontsize=7); fig.tight_layout(); fig.savefig(f"{F}/step_response.png", dpi=200); fig.savefig(f"{F}/step_response.pdf")
# 2) example identification run
d = load_run(f"{R}/id_fit/id_L0_fit_s1.csv"); w = d[(d.time_s > 1) & (d.time_s < 61)]
fig, ax = plt.subplots(2, 1, figsize=(7.2, 3.8), sharex=True); ax[0].step(w.time_s, w.prb_applied, where="post"); ax[0].set_ylabel("effective PRB u"); ax[1].plot(w.time_s, w.thr_total_mbps, lw=.8); ax[1].set_ylabel("y (Mbps)"); ax[1].set_xlabel("time (s)")
for a in ax: a.grid(alpha=.3)
fig.tight_layout(); fig.savefig(f"{F}/id_example.png", dpi=200)
# 3) model-failure cases: backlog drain at load 40, and load step
fig, axs = plt.subplots(2, 1, figsize=(7.2, 4.4))
d = load_run(f"{R}/backlog_failure/backlog_load40_s21.csv"); w = d[(d.time_s > 1) & (d.time_s < 61)]
ax = axs[0]; ax.plot(w.time_s, w.thr_total_mbps, lw=.8, label="y measured"); ax.plot(w.time_s, np.minimum(40, 0.7208 * w.prb_applied), "--", lw=.8, label="static prediction min(40, K·u)"); ax.set_title("Backlog drain: offered 40 Mbps, random PRB schedule", fontsize=8); ax.legend(fontsize=7); ax.set_ylabel("Mbps")
for N, c in ((100, "C0"), (52, "C1"), (24, "C2")):
    d = load_run(f"{R}/loadstep_fixed/loadstep_fixed_N{ {100:100,52:50,24:25}[N] }.csv"); axs[1].plot(d.time_s, d.thr_total_mbps, c, lw=.8, label=f"fixed {N} PRB")
axs[1].set_title("Offered load 120→40 (t=20 s)→120 (t=40 s): y at 100 PRB follows only after the ~10 s RLC backlog drain", fontsize=8); axs[1].set_xlabel("time (s)"); axs[1].set_ylabel("Mbps"); axs[1].legend(fontsize=7)
for a in axs: a.grid(alpha=.3)
fig.tight_layout(); fig.savefig(f"{F}/model_failure_cases.png", dpi=200)
