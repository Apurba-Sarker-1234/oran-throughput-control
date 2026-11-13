"""Phase 1 outputs: static maps (default + hetero), predicted level throughputs, data inventory table, latency table, UE geometry,
supplementary TCP figure. Run after run_experiments.py phase1."""
import os, sys, json, numpy as np, pandas as pd, matplotlib; matplotlib.use("Agg"); import matplotlib.pyplot as plt, warnings
warnings.filterwarnings("ignore"); sys.path.insert(0, os.path.dirname(__file__))
from io_utils import ROOT, load_run, inventory, NOMINAL, EFFECTIVE, load_latency

inv = inventory(); os.makedirs(f"{ROOT}/data/processed", exist_ok=True); os.makedirs(f"{ROOT}/report/tables", exist_ok=True)
# ---- static maps
rows = []
for _, m in inv[inv.category.str.startswith("static_")].iterrows():
    a = m["args"]; d = load_run(m["csv"]); y = d[d.time_s >= 8].thr_total_mbps
    rows.append(dict(dist=m["category"][7:], offered=4 * float(a["loadSched"].split(":")[1]), N_nom=a["fixedPrb"], N_eff=int(d.prb_applied.iloc[-1]),
                     y=y.mean(), y_std=y.std(), **{f"ue{i}": d[f"thr_ue_{i}"][d.time_s >= 8].mean() for i in range(4)}))
S = pd.DataFrame(rows).sort_values(["dist", "offered", "N_nom"]); S.to_csv(f"{ROOT}/data/processed/static_map_e2.csv", index=False)
fig, axs = plt.subplots(1, 2, figsize=(8, 3.3), sharey=True)
for ax, dn, t in zip(axs, ["default", "hetero"], ["UEs at 50/100/150/200 m (homogeneous)", "UEs at 100/1500/3000/4500 m (heterogeneous)"]):
    for off in (16, 40, 120):
        s = S[(S.dist == dn) & (S.offered == off)]; ax.plot(s.N_eff, s.y, "o-", ms=4, label=f"{off} Mbps")
    ax.set_title(t, fontsize=8); ax.set_xlabel("effective PRBs"); ax.set_xticks(EFFECTIVE); ax.grid(alpha=.3)
axs[0].set_ylabel("steady throughput (Mbps)"); axs[0].legend(title="offered (total)", fontsize=7)
fig.tight_layout(); fig.savefig(f"{ROOT}/figures/static_map.png", dpi=200); fig.savefig(f"{ROOT}/figures/static_map.pdf")
piv = S.pivot_table(index=["dist", "offered"], columns="N_eff", values="y").round(2); print("STATIC MAP (Mbps)\n", piv.to_string())
open(f"{ROOT}/report/tables/static_map.tex", "w").write(piv.to_latex())
# slope per effective PRB at saturation
sat = S[(S.offered == 120)]; print("\nK at saturation (Mbps/PRB):", sat.assign(K=sat.y / sat.N_eff).groupby("dist").K.agg(["mean", "std"]).round(4).to_dict())
# ---- per-UE geometry / channel equality
print("\nper-UE throughput at N=100, 120 Mbps offered (Mbps):")
for dn in ("default", "hetero"):
    r = S[(S.dist == dn) & (S.offered == 120) & (S.N_nom == 100)].iloc[0]; print(dn, [round(r[f"ue{i}"], 2) for i in range(4)])
# ---- inventory
inv["n_samples"] = [len(load_run(c)) for c in inv.csv]
inv["sim_s"] = [a["simTime"] for a in inv.args]
g = inv[inv.category != "latency_check"].groupby(["category", "group"]).agg(runs=("name", "count"), sim_s=("sim_s", "first"), samples=("n_samples", "sum"), seeds=("name", lambda x: len(x))).reset_index()
g["latency_Ts"] = [",".join(sorted({str(a["e2Latency"]) for a in inv[(inv.category == c) & (inv.group == gr)].args})) for c, gr in zip(g.category, g.group)]
g["offered_Mbps"] = [",".join(sorted({str(int(4 * float(a["loadSched"].split(":")[1].split(",")[0]))) for a in inv[(inv.category == c) & (inv.group == gr)].args})) for c, gr in zip(g.category, g.group)]
g.to_csv(f"{ROOT}/data/processed/data_inventory.csv", index=False); print("\nINVENTORY\n", g.to_string(index=False))
open(f"{ROOT}/report/tables/data_inventory.tex", "w").write(g.to_latex(index=False))
# ---- latency
lat = []
for _, m in inv[inv.category == "latency_check"].iterrows():
    l = load_latency(m["csv"]); l = l[l.issue_time_s > 1]; r = pd.read_csv(m["csv"] + ".lmrun.csv"); r = r[r.newest_report_time_s > 0]
    lat.append(dict(e2Latency_Ts=m["args"]["e2Latency"], cmd_latency_ms_mean=1000 * l.latency_s.mean(), cmd_latency_ms_min=1000 * l.latency_s.min(), cmd_latency_ms_max=1000 * l.latency_s.max(),
                    report_age_at_lm_ms=1000 * (r.run_time_s - r.newest_report_time_s)[r.run_time_s > 1].median()))
L = pd.DataFrame(lat).sort_values("e2Latency_Ts").round(2); print("\nLATENCY\n", L.to_string(index=False)); L.to_csv(f"{ROOT}/data/processed/latency.csv", index=False)
open(f"{ROOT}/report/tables/latency.tex", "w").write(L.to_latex(index=False))
# ---- predicted level throughputs at each load: y = min(offered, K*N_eff) with measured saturated levels
s120 = S[(S.dist == "default") & (S.offered == 120)].set_index("N_eff").y
P = pd.DataFrame({off: [min(off, v) for v in s120.values] for off in (16, 40, 120)}, index=s120.index).round(2); P.index.name = "N_eff"
print("\nPREDICTED level throughput min(offered, y_sat(N)):\n", P.to_string()); P.to_csv(f"{ROOT}/data/processed/level_throughput.csv")
# ---- supplementary TCP figure from earlier fixed-N sweeps
tcp = pd.read_csv(f"{ROOT}/data/processed/static_map.csv"); tcp = tcp[tcp.traffic == "tcprate"]
fig, ax = plt.subplots(figsize=(4.2, 3.2))
for L_ in (8, 24, 40, 120):
    s = tcp[tcp.offered_total == L_]; ax.plot(s.N_eff.replace({48: 52}), s.y_mean, "o-", ms=3, label=f"{L_} Mbps")
ax.set_xlabel("effective PRBs"); ax.set_ylabel("steady throughput (Mbps)"); ax.set_title("Supplementary: TCP (RLC AM)", fontsize=8); ax.legend(fontsize=7); ax.grid(alpha=.3)
fig.tight_layout(); fig.savefig(f"{ROOT}/figures/supp_static_map_tcp.png", dpi=200)
