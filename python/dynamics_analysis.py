import pandas as pd, numpy as np, matplotlib; matplotlib.use("Agg"); import matplotlib.pyplot as plt, warnings; warnings.filterwarnings("ignore")
steps=[(10,100,25),(20,25,100),(30,100,6),(40,6,50),(50,50,100)]
runs={"UDP sat (30 Mbps/UE)":"udp_sat","UDP 10 Mbps/UE":"udp_10","TCP bulk":"tcp_bulk","TCP-OnOff 10 Mbps/UE":"tcp_10"}
def metrics(d,t0):
    d=d.set_index("time_s").thr_total_mbps
    pre=d[t0-3:t0-0.2].mean(); post=d[t0+4:t0+9.5].mean(); sm=d.rolling(10,center=True).mean()  # 100 ms smoothing
    seg=sm[t0:t0+9.5]; Δ=post-pre
    frac=(seg-pre)/Δ if abs(Δ)>0.3 else None
    if frac is None: return pre,post,np.nan,np.nan,np.nan,np.nan
    t63=frac[frac>=.63].index.min()-t0 if (frac>=.63).any() else np.nan
    out=np.abs(frac-1)>.05; ts=(out[out].index.max()-t0+0.01) if out.any() else 0.0
    ov=(frac.max()-1)*100 if Δ>0 else (frac.max()-1)*100
    ext=(seg.max()-max(pre,post))/abs(Δ)*100 if Δ>0 else (min(pre,post)-seg.min())/abs(Δ)*100
    return pre,post,t63,ts,ext,Δ
rows=[]; fig,axs=plt.subplots(len(runs),2,figsize=(9,8),sharex="col")
for r,(lab,f) in enumerate(runs.items()):
    d=pd.read_csv(f"data/raw/dyn_{f}.csv")
    for c,(t0,a,b) in enumerate([(20,25,100),(10,100,25)]):
        w=d[(d.time_s>t0-1)&(d.time_s<t0+3)]; axs[r,c].plot(w.time_s-t0,w.thr_total_mbps,lw=.6); axs[r,c].axvline(0,c="r",lw=.5)
        axs[r,c].set_title(f"{lab}: {a}→{b}",fontsize=8)
    for t0,a,b in steps:
        p,q,t63,ts,ex,D=metrics(d,t0); rows.append((lab,f"{a}→{b}",round(p,1),round(q,1),t63,ts,round(ex,1) if ex==ex else np.nan))
for a in axs[-1]: a.set_xlabel("time since step (s)")
fig.tight_layout(); fig.savefig("figures/dynamics_check_steps.png",dpi=160)
T=pd.DataFrame(rows,columns=["traffic","step(nominal)","pre","post","t63_s","settle5%_s","overshoot_%"]); print(T.to_string())
