import csv, glob, collections
import matplotlib; matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch, FancyArrowPatch
plt.rcParams.update({"font.family":"DejaVu Sans","font.size":8.5,"axes.grid":True,"grid.alpha":.25,"axes.spines.top":False,"axes.spines.right":False})
NAVY="#1f3a5f"; TEAL="#1b8a8f"; ORG="#d9822b"; RED="#c0392b"; GRY="#7f8c8d"; GRN="#2e8b57"
def rd(p): return list(csv.DictReader(open(p)))
M=collections.defaultdict(dict)
for f in glob.glob("out/metrics_*.csv"):
    t=f.split("metrics_")[1][:-4]
    for r in rd(f): M[t][r["group"]+"."+r["key"]]=float(r["value"])

# ---------- F1 architecture ----------
fig,ax=plt.subplots(figsize=(7.4,4.6)); ax.set_xlim(0,100); ax.set_ylim(0,62); ax.axis("off")
def box(x,y,w,h,txt,fc,ec=NAVY,fs=8,col="black",bold=False):
    ax.add_patch(FancyBboxPatch((x,y),w,h,boxstyle="round,pad=0.3,rounding_size=1.2",fc=fc,ec=ec,lw=1))
    ax.text(x+w/2,y+h/2,txt,ha="center",va="center",fontsize=fs,color=col,fontweight="bold" if bold else "normal")
def arr(x1,y1,x2,y2,c=NAVY,st="-|>"): ax.add_patch(FancyArrowPatch((x1,y1),(x2,y2),arrowstyle=st,mutation_scale=9,color=c,lw=1))
ax.text(1,60,"Application layer (cooperative scheduler, 10 tasks)",fontsize=8.5,color=NAVY,fontweight="bold")
box(1,46,18,9,"ADAS\nFCW + brake +\nimpact","#dce8f7"); box(21,46,16,9,"Alert\nbuzzer / LEDs","#dce8f7"); box(39,46,18,9,"Dashboard\nMAX7219 view","#dce8f7")
box(59,46,18,9,"Telemetry + CLI\nCRC-8 frames","#dce8f7"); box(79,46,20,9,"Supervisor\nfaults, watchdog","#dce8f7")
box(1,33,98,8,"Shared vehicle state g_veh (values, timestamps, sequence numbers, fault mask) + event log","#fff3d6",ec=ORG,fs=8.2,bold=True)
ax.text(1,29.3,"Driver / service layer",fontsize=8.5,color=NAVY,fontweight="bold")
box(1,17,16,9,"mpu6050\nretry, recover,\nfrozen-data","#e3f3e8",ec=GRN); box(19,17,16,9,"range\nα-β tracker\n+ gating","#e3f3e8",ec=GRN); box(37,17,14,9,"sensors\nspeed, battery\nSoC LUT","#e3f3e8",ec=GRN)
box(53,17,14,9,"max7219\ndirty update,\nself-heal","#e3f3e8",ec=GRN); box(69,17,15,9,"uart_log\nTX ring, framing","#e3f3e8",ec=GRN); box(86,17,13,9,"sched\nWCET, jitter","#e3f3e8",ec=GRN)
ax.text(1,13.3,"Hardware abstraction layer (hal.h)  -  port/hal_sim.c (host SIL)  |  STM32F103 port (HAL/LL)",fontsize=8.5,color=NAVY,fontweight="bold")
box(1,1,98,9,"I2C1 400 kHz | SPI1 1 MHz | USART1 115200 | ADC1 2ch | TIM1 input-capture | GPIO | IWDG | SysTick 1 kHz","#eeeeee",ec=GRY,fs=8)
for x in (10,28,44,60,76,92): arr(x,26,x,33.0,c=GRN)
for x in (10,29,48,68,89): arr(x,41,x,46,c=ORG)
fig.tight_layout(); fig.savefig("fig/f1_arch.png",dpi=200); plt.close()

# ---------- F3 safety path latency budget ----------
fig,ax=plt.subplots(figsize=(7.4,2.1)); ax.set_xlim(0,100); ax.set_ylim(0,20); ax.axis("off")
st=[("Echo capture\n(≤23 ms, poll 5 ms)",TEAL),("α-β tracker\n(no lag, gated)",TEAL),("Vote: 2 of last 3\nsamples (≤50–100 ms)",NAVY),("adas_task\n(10 ms period)",NAVY),("alert_task\n(10 ms period)",NAVY),("Buzzer / LED\nGPIO edge",ORG)]
w=15
for i,(t,c) in enumerate(st):
    x=1+i*16.5; ax.add_patch(FancyBboxPatch((x,6),w,10,boxstyle="round,pad=0.3,rounding_size=1",fc="white",ec=c,lw=1.4)); ax.text(x+w/2,11,t,ha="center",va="center",fontsize=7.4)
    if i<5: arr(x+w+.3,11,x+16.2,11,c=c)
ax.text(1,2,"Measured end-to-end (TC-S12, 40 randomised trials): min 46 ms, mean 79 ms, p95 112 ms, max 120 ms after the true TTC threshold crossing  (requirement ≤ 250 ms)",fontsize=7.6,color=RED)
fig.tight_layout(); fig.savefig("fig/f3_path.png",dpi=200); plt.close()

# ---------- F4 S01 trace ----------
T=rd("out/trace_s01_final.csv"); t=[int(r["t"])/1000 for r in T]
fig,axs=plt.subplots(4,1,figsize=(7.4,7.0),sharex=True)
axs[0].plot(t,[int(r["dist_raw"]) if int(r["dist_raw"])<6000 else None for r in T],".",ms=2,color=GRY,label="raw echo (mm)")
axs[0].plot(t,[int(r["dist_true"]) if int(r["dist_true"])>=0 else None for r in T],color=NAVY,lw=1.2,label="true distance")
axs[0].plot(t,[int(r["dist_filt"]) if int(r["dist_filt"])>=0 else None for r in T],color=TEAL,lw=1.2,label="tracker output")
axs[0].set_ylabel("distance [mm]"); axs[0].set_ylim(0,4300); axs[0].legend(ncol=3,fontsize=7,loc="upper right")
axs[1].plot(t,[int(r["closing"]) for r in T],color=ORG,label="closing speed [mm/s]"); axs[1].set_ylabel("closing [mm/s]")
a2=axs[1].twinx(); a2.plot(t,[min(int(r["ttc"]),5000) if int(r["ttc"])>=0 else None for r in T],color=RED,lw=1,label="TTC [ms]"); a2.set_ylabel("TTC [ms]"); a2.grid(False)
for y,l in ((3000,"ADV 3 s"),(2000,"WARN 2 s"),(1000,"CRIT 1 s")): a2.axhline(y,color=RED,ls=":",lw=.7); a2.text(0.1,y+60,l,fontsize=6.5,color=RED)
axs[1].legend(loc="center right",fontsize=7); 
axs[2].step(t,[int(r["level"]) for r in T],color=NAVY,where="post"); axs[2].set_yticks([0,1,2,3]); axs[2].set_yticklabels(["CLEAR","ADV","WARN","CRIT"]); axs[2].set_ylabel("ADAS level")
axs[2].fill_between(t,0,[int(r["buzzer"]) *0.6 for r in T],step="post",color=ORG,alpha=.5,label="buzzer GPIO"); axs[2].legend(loc="upper right",fontsize=7)
axs[3].plot(t,[int(r["ax"]) for r in T],color=GRN); axs[3].set_ylabel("IMU ax [mg]"); axs[3].set_xlabel("time [s]")
for a in axs: a.set_xlim(0,9)
fig.suptitle("TC-S01 emergency braking scenario (signals recorded from the integrated firmware)",fontsize=9.5)
fig.tight_layout(); fig.savefig("fig/f4_s01.png",dpi=200); plt.close()

# ---------- F5 filter comparison ----------
fig,axs=plt.subplots(1,2,figsize=(7.4,2.9))
for tag,c,l in (("raw",GRY,"raw difference"),("v0_median_consec",ORG,"median-of-3 + EMA"),("final",TEAL,"α-β tracker + gating")):
    T=rd(f"out/trace_s01_{tag}.csv"); xs=[int(r["t"])/1000 for r in T if 800<=int(r["t"])<=2300]; ys=[int(r["closing"]) for r in T if 800<=int(r["t"])<=2300]
    axs[0].plot(xs,ys,color=c,lw=.9,label=l)
axs[0].axhline(1500,color="k",ls=":",lw=.8); axs[0].set_ylim(-3000,6000); axs[0].set_xlabel("time [s]"); axs[0].set_ylabel("closing-speed estimate [mm/s]"); axs[0].legend(fontsize=6.5); axs[0].set_title("Estimate during 1.5 m/s approach (truth dotted)",fontsize=8)
bins=range(-200,340,20)
for tag,c,l in (("v0_median_consec",ORG,"median-of-3 (before)"),("final",TEAL,"α-β tracker (after)")):
    v=[float(r["latency_ms"]) for r in rd(f"out/latency_{tag}.csv")]; axs[1].hist(v,bins=bins,alpha=.65,color=c,label=l)
axs[1].axvline(250,color=RED,ls="--",lw=1); axs[1].text(252,axs[1].get_ylim()[1]*.85,"limit\n250 ms",fontsize=7,color=RED)
axs[1].set_xlabel("CRIT alert latency vs true threshold crossing [ms]"); axs[1].set_ylabel("trials"); axs[1].legend(fontsize=6.5); axs[1].set_title("TC-S12: 40 randomised trials",fontsize=8)
fig.tight_layout(); fig.savefig("fig/f5_filter.png",dpi=200); plt.close()

# ---------- F6 scheduling ----------
fig,axs=plt.subplots(1,2,figsize=(7.4,2.7))
names=["final\n(non-blocking)","UART polled\n(blocking)","HC-SR04 read\nblocking"]
vals=[M["final"]["exp_sched.adas_max_jitter_us"]/1000,M["uart_blocking"]["exp_sched.adas_max_jitter_us"]/1000,M["range_blocking"]["exp_sched.adas_max_jitter_us"]/1000]
b=axs[0].bar(names,vals,color=[TEAL,ORG,RED]); axs[0].set_yscale("log"); axs[0].axhline(2,color=RED,ls="--",lw=1); axs[0].text(1.9,2.3,"2 ms limit",fontsize=7,color=RED,ha="right")
for r,v in zip(b,vals): axs[0].text(r.get_x()+r.get_width()/2,v*1.15,f"{v:.2f} ms",ha="center",fontsize=7)
axs[0].set_ylabel("max start jitter of 10 ms ADAS task [ms]"); axs[0].set_title("Blocking vs interrupt-driven I/O (20 s)",fontsize=8)
n2=["bus timeout\n5 ms","bus timeout\n2 ms"]; v2=[M["final"]["exp_i2c.timeout5000_misses"],M["final"]["exp_i2c.timeout2000_misses"]]
b=axs[1].bar(n2,v2,color=[RED,TEAL]); 
for r,v in zip(b,v2): axs[1].text(r.get_x()+r.get_width()/2,v+.4,f"{int(v)}",ha="center",fontsize=8)
axs[1].set_ylabel("deadline misses in 20 s"); axs[1].set_title("I2C bus stuck once per second",fontsize=8)
fig.tight_layout(); fig.savefig("fig/f6_sched.png",dpi=200); plt.close()

# ---------- F7 battery ----------
T=rd("out/trace_batt_final.csv"); t=[int(r["t"])/1000 for r in T]
fig,ax1=plt.subplots(figsize=(7.4,2.9)); ax1.plot(t,[int(r["pack_true_mv"])/1000 for r in T],color=GRY,lw=1,label="true pack voltage (with sag pulses)"); ax1.plot(t,[int(r["pack_meas_mv"])/1000 for r in T],color=NAVY,lw=1.2,label="firmware filtered voltage")
ax1.set_ylabel("pack voltage [V]"); ax1.set_xlabel("time [s]  (discharge compressed to 220 s)"); a2=ax1.twinx(); a2.plot(t,[int(r["soc"]) for r in T],color=TEAL,lw=1.4,label="SoC [%]"); a2.set_ylabel("SoC [%]"); a2.grid(False)
lw=[x for x,r in zip(t,T) if int(r["low"])]; cr=[x for x,r in zip(t,T) if int(r["crit"])]
if lw: ax1.axvline(lw[0],color=ORG,ls="--",lw=1); ax1.text(lw[0]+1,15.0,"LOW (<20 %)",fontsize=7,color=ORG)
if cr: ax1.axvline(cr[0],color=RED,ls="--",lw=1); ax1.text(cr[0]+1,13.8,"CRIT (<10 %)",fontsize=7,color=RED)
h1,l1=ax1.get_legend_handles_labels(); h2,l2=a2.get_legend_handles_labels(); ax1.legend(h1+h2,l1+l2,fontsize=7,loc="upper right")
fig.tight_layout(); fig.savefig("fig/f7_batt.png",dpi=200); plt.close()
print("figs ok")
