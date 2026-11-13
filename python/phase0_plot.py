import pandas as pd, numpy as np, matplotlib; matplotlib.use("Agg"); import matplotlib.pyplot as plt
c=pd.read_csv("data/raw/phase0_cap.csv"); b=pd.read_csv("data/raw/phase0_bw.csv")
fig,ax=plt.subplots(figsize=(7,3.6))
ax.plot(c.time_s,c.thr_total_mbps,label="scheduler PRB cap (MaxDlPrb)"); ax.plot(b.time_s,b.thr_total_mbps,"--",label="runtime SetDlBandwidth")
for t,n in zip([0,10,20,30,40],[100,25,50,6,100]): ax.axvline(t,c="gray",lw=.5); ax.text(t+.3,ax.get_ylim()[1]*.92,f"N={n}",fontsize=8)
ax.set_xlabel("time (s)"); ax.set_ylabel("aggregate DL throughput (Mbps)"); ax.legend(loc="center right"); fig.tight_layout(); fig.savefig("figures/phase0_prb_step_check.png",dpi=200)
for name,d in (("cap",c),("bw",b)):
    print(name,[ (n, round(d[(d.time_s>t+2)&(d.time_s<=t+10)].thr_total_mbps.mean(),2)) for t,n in zip([0,10,20,30,40,50],[100,25,50,6,100,100])])
