"""Evaluation metrics for closed-loop / open-loop runs (all inputs are DataFrames from io_utils.load_run)."""
import numpy as np


def _tail(df, t0):
    return df[df.time_s >= t0]


def frac_outside_band(df, y_lo, y_hi, t0=0.0):
    y = _tail(df, t0).thr_total_mbps.to_numpy()
    return float(np.mean((y < y_lo) | (y > y_hi)))


def settling_time_into_band(df, y_lo, y_hi, t_event=0.0, hold=1.0):
    """First time >= t_event after which y stays in band for at least `hold` seconds (NaN if never)."""
    d = df[df.time_s >= t_event]; t = d.time_s.to_numpy(); y = d.thr_total_mbps.to_numpy()
    ok = (y >= y_lo) & (y <= y_hi)
    for i in range(len(t)):
        j = np.searchsorted(t, t[i] + hold)
        if ok[i:j].all() and j <= len(t) and t[min(j, len(t)) - 1] - t[i] >= hold - 1e-9:
            return float(t[i] - t_event)
    return float("nan")


def n_switches(df, t0=0.0):
    u = _tail(df, t0).prb_applied.to_numpy(); return int(np.sum(np.diff(u) != 0))


def total_switch_magnitude(df, t0=0.0):
    u = _tail(df, t0).prb_applied.to_numpy(); return float(np.sum(np.abs(np.diff(u))))


def mean_prb(df, t0=0.0): return float(_tail(df, t0).prb_applied.mean())
def mean_throughput(df, t0=0.0): return float(_tail(df, t0).thr_total_mbps.mean())


def prediction_rmse(y_meas, y_pred):
    y_meas = np.asarray(y_meas, float); y_pred = np.asarray(y_pred, float)
    return float(np.sqrt(np.mean((y_meas - y_pred) ** 2)))


def fit_percent(y, yhat):
    y = np.asarray(y, float); yhat = np.asarray(yhat, float)
    return float(100 * (1 - np.linalg.norm(y - yhat) / np.linalg.norm(y - y.mean())))


def summarize(df, y_lo, y_hi, t0=0.0, t_event=0.0):
    return dict(frac_outside=frac_outside_band(df, y_lo, y_hi, t0), settle_s=settling_time_into_band(df, y_lo, y_hi, t_event),
                switches=n_switches(df, t0), switch_mag=total_switch_magnitude(df, t0),
                mean_prb=mean_prb(df, t0), mean_thr=mean_throughput(df, t0))


def mean_std(values):
    v = np.asarray(values, float); return float(np.nanmean(v)), float(np.nanstd(v, ddof=1) if len(v) > 1 else 0.0)
