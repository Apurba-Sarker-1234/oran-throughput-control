import pandas as pd, numpy as np, matplotlib; matplotlib.use("Agg"); import matplotlib.pyplot as plt
NOM=[6,15,25,50,75,100]; EFF={6:8,15:16,25:24,50:52,75:76,100:100}; LOADS=[2,4,6,8,10,15,30]
rows=[]
for tr in ["udp","tcprate"]:
    for L in LOADS:
        for N in NOM:
            d=pd.read_csv(f"data/raw/static/{tr}_L{L}_N{N}.csv"); y=d[d.time_s>=8].thr_total_mbps
            rows.append((tr,L,4*L,N,EFF[N],y.mean(),y.std()))
T=pd.DataFrame(rows,columns=["traffic","load_per_ue","offered_total","N_nom","N_eff","y_mean","y_std"]); T.to_csv("data/processed/static_map.csv",index=False)
pd.DataFrame({"nominal":NOM,"effective":[EFF[n] for n in NOM]}).to_csv("data/processed/prb_mapping.csv",index=False)
fig,axs=plt.subplots(1,2,figsize=(9,3.6),sharey=True)
for ax,tr,t in zip(axs,["udp","tcprate"],["UDP CBR","TCP (rate-limited OnOff)"]):
    for L in LOADS:
        s=T[(T.traffic==tr)&(T.load_per_ue==L)]; ax.plot(s.N_eff,s.y_mean,"o-",ms=3,label=f"{4*L} Mbps")
    ax.set_title(t); ax.set_xlabel("effective PRBs"); ax.grid(alpha=.3)
axs[0].set_ylabel("steady throughput (Mbps)"); axs[1].legend(title="offered total",fontsize=7)
fig.tight_layout(); fig.savefig("figures/static_map.png",dpi=200)
pv=T.pivot_table(index=["traffic","offered_total"],columns="N_eff",values="y_mean").round(1); print(pv.to_string())
print(T.groupby("traffic").y_std.mean())
