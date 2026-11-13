"""Loading ns-3 run logs and run metadata."""
import json, os, glob
import numpy as np, pandas as pd

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
with open(os.path.join(ROOT, "configs", "prb_mapping.json")) as _f:
    _M = json.load(_f)
NOMINAL = _M["nominal"]; EFFECTIVE = _M["effective"]
NOM2EFF = dict(zip(NOMINAL, EFFECTIVE)); EFF2NOM = dict(zip(EFFECTIVE, NOMINAL))


def load_run(csv_path):
    """Return DataFrame of one run (columns as logged) with attrs['meta'] from <csv>.meta.json if present."""
    df = pd.read_csv(csv_path)
    mp = csv_path + ".meta.json"
    df.attrs["meta"] = json.load(open(mp)) if os.path.exists(mp) else {}
    return df


def ue_cols(df):
    return [c for c in df.columns if c.startswith("thr_ue_")]


def load_latency(csv_path):
    return pd.read_csv(csv_path + ".latency.csv")


def inventory(raw_dir=None, pattern="**/*.csv.meta.json"):
    """Table of all runs with their metadata (one row per run)."""
    raw_dir = raw_dir or os.path.join(ROOT, "data", "raw")
    rows = []
    for p in sorted(glob.glob(os.path.join(raw_dir, pattern), recursive=True)):
        m = json.load(open(p)); m["csv"] = p[:-len(".meta.json")]; rows.append(m)
    return pd.DataFrame(rows)


def fit_arrays(df, t_start=2.0):
    """(u_eff, y) arrays after traffic has started; u = effective PRB applied during window k, y = Mbps in window k."""
    d = df[df.time_s >= t_start]
    return d.prb_applied.to_numpy(float), d.thr_total_mbps.to_numpy(float), d.time_s.to_numpy(float)
