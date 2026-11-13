"""Batch runner: builds a job list, runs ns-3 scenario in parallel, skips existing outputs, writes <csv>.meta.json.
Usage: python3 python/run_experiments.py phase1 [--jobs N] [--dry]"""
import argparse, hashlib, json, os, random, subprocess, sys, time
from concurrent.futures import ThreadPoolExecutor

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
NS3 = os.path.join(ROOT, "ns-3-env", "ns-3.42")
BIN = os.path.join(NS3, "build", "scratch", "scn", "ns3.42-scenario_single_cell-optimized")
RAW = os.path.join(ROOT, "data", "raw")
LEVELS = [6, 15, 25, 50, 75, 100]
HETERO = "100,1500,3000,4500"; DEFAULT_D = "50,100,150,200"
LOAD_UE = {16: 4, 40: 10, 120: 30}   # total offered Mbps -> per UE (4 UEs)


def sha(p):
    return hashlib.sha256(open(p, "rb").read()).hexdigest()[:12]


def random_schedule(seed, sim_t, hold=(2.0, 5.0), Ts=0.1, levels=LEVELS):
    rng = random.Random(seed); t = 0.0; cur = rng.choice(levels); items = []
    while t < sim_t:
        items.append(f"{t:.1f}:{cur}"); t += round(rng.uniform(*hold) / Ts) * Ts
        cur = rng.choice([l for l in levels if l != cur])
    return ",".join(items)


def job(name, cat, group, **kw):
    a = dict(nUe=4, traffic="udp", policy="openloop", schedule="0:100", loadSched="0:30", simTime=60, Ts=0.1, seed=1,
             e2Latency=0, distances=DEFAULT_D, fixedPrb=100); a.update(kw)
    return dict(name=name, category=cat, group=group, args=a)


def phase1_jobs():
    J = []
    for L in (0, 1, 2):                      # identification + validation, saturated load only (120 Mbps), hold 2-5 s
        for role, seeds in (("fit", range(1, 6)), ("val", range(11, 14))):
            for s in seeds:
                J.append(job(f"id_L{L}_{role}_s{s}", "id_" + role, f"L{L}", seed=s, e2Latency=L, simTime=200,
                             schedule=random_schedule(s, 200), loadSched="0:30"))
    for L in (0, 1, 2):                      # command latency / report-age measurement (also step response, 10 s holds)
        J.append(job(f"latency_check_L{L}", "latency_check", "latency", e2Latency=L, simTime=60,
                     schedule="0:100,10:25,20:100,30:6,40:52,50:100".replace("52", "50"), loadSched="0:30"))
    for tot in (16, 40):                     # backlog / unsaturated: dedicated model-failure runs (not used for ID)
        for s in (21, 22, 23):
            J.append(job(f"backlog_load{tot}_s{s}", "backlog_failure", f"load{tot}", seed=s, simTime=200,
                         schedule=random_schedule(s, 200), loadSched=f"0:{LOAD_UE[tot]}"))
    for N in LEVELS:                          # load step 120 -> 40 -> 120 with fixed PRB
        J.append(job(f"loadstep_fixed_N{N}", "loadstep_fixed", "loadstep", simTime=60, policy="fixed", fixedPrb=N,
                     loadSched="0:30,20:10,40:30"))
    for s in (31, 32, 33):                    # load step with random open-loop PRBs
        J.append(job(f"loadstep_openloop_s{s}", "loadstep_openloop", "loadstep", seed=s, simTime=120,
                     schedule=random_schedule(s, 120), loadSched="0:30,40:10,80:30"))
    for dn, dist in (("default", DEFAULT_D), ("hetero", HETERO)):   # static maps through the E2 path
        for tot, per in LOAD_UE.items():
            for N in LEVELS:
                J.append(job(f"static_{dn}_load{tot}_N{N}", "static_" + dn, f"static_{dn}", simTime=20, policy="fixed",
                             fixedPrb=N, loadSched=f"0:{per}", distances=dist))
    return J


def run(j, dry=False):
    d = os.path.join(RAW, j["category"]); os.makedirs(d, exist_ok=True)
    csv = os.path.join(d, j["name"] + ".csv")
    if os.path.exists(csv + ".meta.json"):
        return "skip"
    cmd = [BIN] + [f"--{k}={v}" for k, v in j["args"].items()] + [f"--out={csv}"]
    if dry:
        return " ".join(cmd)
    t0 = time.time(); r = subprocess.run(cmd, capture_output=True, text=True, cwd=NS3)
    if r.returncode != 0:
        return f"FAIL {j['name']}: {r.stderr[-300:]}"
    meta = dict(j, ns3_version="3.42", cmd=" ".join(cmd), binary_sha256_12=sha(BIN), runtime_s=round(time.time() - t0, 1),
                patches={p: sha(os.path.join(ROOT, "patches", p)) for p in sorted(os.listdir(os.path.join(ROOT, "patches")))},
                git_commit="n/a (project directory is not a git repository)")
    json.dump(meta, open(csv + ".meta.json", "w"), indent=1)
    return "ok"


if __name__ == "__main__":
    ap = argparse.ArgumentParser(); ap.add_argument("matrix", choices=["phase1"]); ap.add_argument("--jobs", type=int, default=10)
    ap.add_argument("--dry", action="store_true"); a = ap.parse_args()
    J = phase1_jobs(); print(len(J), "jobs")
    with ThreadPoolExecutor(a.jobs) as ex:
        res = list(ex.map(lambda j: run(j, a.dry), J))
    print({k: res.count(k) for k in set(res) if not k.startswith("FAIL")}); [print(r) for r in res if r.startswith("FAIL")]
