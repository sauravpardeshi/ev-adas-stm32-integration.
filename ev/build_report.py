import csv, glob, collections, re, datetime
from reportlab.lib.pagesizes import A4
from reportlab.lib.units import mm
from reportlab.lib import colors
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.enums import TA_JUSTIFY, TA_CENTER
from reportlab.platypus import (BaseDocTemplate, PageTemplate, Frame, Paragraph, Spacer, Table, TableStyle, Image,
                                PageBreak, KeepTogether, Preformatted, NextPageTemplate)
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
D="/usr/share/fonts/truetype/dejavu/"
pdfmetrics.registerFont(TTFont("DV",D+"DejaVuSans.ttf")); pdfmetrics.registerFont(TTFont("DV-B",D+"DejaVuSans-Bold.ttf"))
pdfmetrics.registerFont(TTFont("DV-I",D+"DejaVuSans-Oblique.ttf")); pdfmetrics.registerFont(TTFont("DVM",D+"DejaVuSansMono.ttf"))
pdfmetrics.registerFontFamily("DV",normal="DV",bold="DV-B",italic="DV-I",boldItalic="DV-B")
NAVY=colors.HexColor("#1f3a5f"); TEAL=colors.HexColor("#1b8a8f"); LIGHT=colors.HexColor("#eef3f9"); RED=colors.HexColor("#c0392b"); GRN=colors.HexColor("#2e8b57")
def S(n,**k): return ParagraphStyle(n,**k)
body=S("b",fontName="DV",fontSize=9,leading=13,alignment=TA_JUSTIFY,spaceAfter=5)
h1=S("h1",fontName="DV-B",fontSize=15,leading=19,textColor=NAVY,spaceBefore=6,spaceAfter=7)
h2=S("h2",fontName="DV-B",fontSize=11,leading=14,textColor=NAVY,spaceBefore=8,spaceAfter=4)
h3=S("h3",fontName="DV-B",fontSize=9.4,leading=12,textColor=TEAL,spaceBefore=5,spaceAfter=2)
cap=S("cap",fontName="DV-I",fontSize=7.8,leading=10,alignment=TA_CENTER,textColor=colors.HexColor("#444444"),spaceAfter=8)
cell=S("c",fontName="DV",fontSize=7.4,leading=9.2); cellb=S("cb",parent=cell,fontName="DV-B")
cellw=S("cw",parent=cell,textColor=colors.white,fontName="DV-B")
small=S("s",fontName="DV",fontSize=7.8,leading=10.5,alignment=TA_JUSTIFY,spaceAfter=3)
code=S("code",fontName="DVM",fontSize=6.5,leading=8.0)
bul=S("bul",parent=body,leftIndent=12,bulletIndent=2,spaceAfter=2)
def P(t,s=body): return Paragraph(t,s)
def B(items): return [Paragraph(i,bul,bulletText="•") for i in items]
def esc(t): return t.replace("&","&amp;").replace("<","&lt;").replace(">","&gt;")
def tbl(rows,widths,head=True,fs=None,zebra=True,extra=None):
    data=[]
    for i,r in enumerate(rows):
        data.append([Paragraph(str(c),cellw if (head and i==0) else cell) if not hasattr(c,"wrap") else c for c in r])
    t=Table(data,colWidths=[w*mm for w in widths],repeatRows=1 if head else 0)
    st=[("GRID",(0,0),(-1,-1),0.3,colors.HexColor("#b8c4d4")),("VALIGN",(0,0),(-1,-1),"TOP"),("LEFTPADDING",(0,0),(-1,-1),3),("RIGHTPADDING",(0,0),(-1,-1),3),("TOPPADDING",(0,0),(-1,-1),2),("BOTTOMPADDING",(0,0),(-1,-1),2)]
    if head: st+= [("BACKGROUND",(0,0),(-1,0),NAVY)]
    if zebra:
        for i in range(1,len(rows)):
            if i%2==0: st.append(("BACKGROUND",(0,i),(-1,i),LIGHT))
    if extra: st+=extra
    t.setStyle(TableStyle(st)); return t
def fig(path,w,caption): 
    from PIL import Image as PI
    iw,ih=PI.open(path).size; return KeepTogether([Image(path,width=w*mm,height=w*mm*ih/iw),P(caption,cap)])
def codeblock(path,start=None,end=None,title=None,maxlines=70):
    L=open(path).read().split("\n")
    if start is not None: L=L[start-1:end]
    L=[l.replace("\t","    ") for l in L][:maxlines]
    out=[]
    if title: out.append(P(title,h3))
    t=Table([[Preformatted("\n".join(L),code)]],colWidths=[178*mm]); t.setStyle(TableStyle([("BACKGROUND",(0,0),(-1,-1),colors.HexColor("#f6f8fa")),("BOX",(0,0),(-1,-1),0.4,colors.HexColor("#c5cdd8")),("LEFTPADDING",(0,0),(-1,-1),4),("TOPPADDING",(0,0),(-1,-1),3),("BOTTOMPADDING",(0,0),(-1,-1),3)]))
    out.append(t); out.append(Spacer(1,5)); return out
def find(path,needle,n):
    L=open(path).read().split("\n")
    for i,l in enumerate(L):
        if needle in l: return i+1,i+n
    raise Exception(needle)

# ---------------- data ----------------
res={}
for f in glob.glob("out/results_*.csv"):
    tag=f.split("results_")[1][:-4]; res[tag]=list(csv.DictReader(open(f)))
M=collections.defaultdict(dict)
for f in glob.glob("out/metrics_*.csv"):
    t=f.split("metrics_")[1][:-4]
    for r in csv.DictReader(open(f)): M[t][r["group"]+"."+r["key"]]=float(r["value"])
F=res["final"]; npass=sum(r["result"]=="PASS" for r in F); N=len(F)
byid={r["id"]:r for r in F}
mf=M["final"]
tasks=list(csv.DictReader(open("out/task_stats_final.csv")))
def count(l): return sum(1 for r in F if r["level"]==l)

# ---------------- page templates ----------------
def decor(c,d):
    c.saveState(); c.setFont("DV",7.5); c.setFillColor(colors.HexColor("#555555"))
    c.drawString(16*mm,10*mm,"Final Task – System Integration & Testing | EV Dashboard & ADAS Warning System | Saurav Pardeshi")
    c.drawRightString(194*mm,10*mm,f"Page {d.page}"); c.setStrokeColor(colors.HexColor("#c5cdd8")); c.line(16*mm,13*mm,194*mm,13*mm); c.restoreState()
def cover(c,d):
    c.saveState(); c.setFillColor(NAVY); c.rect(0,A4[1]-95*mm,A4[0],95*mm,fill=1,stroke=0); c.setFillColor(TEAL); c.rect(0,A4[1]-98*mm,A4[0],3*mm,fill=1,stroke=0); c.restoreState()
doc=BaseDocTemplate("EV_ADAS_Integration_Test_Report.pdf",pagesize=A4,leftMargin=16*mm,rightMargin=16*mm,topMargin=16*mm,bottomMargin=18*mm,title="Final Task – Embedded System Integration & System-Level Testing Report",author="Saurav Pardeshi")
fr=Frame(16*mm,18*mm,178*mm,A4[1]-34*mm,id="f")
doc.addPageTemplates([PageTemplate(id="cover",frames=[Frame(16*mm,16*mm,178*mm,A4[1]-32*mm)],onPage=cover),PageTemplate(id="main",frames=[fr],onPage=decor)])
st=[]
# ---------------- cover ----------------
tt=S("tt",fontName="DV-B",fontSize=24,leading=29,textColor=colors.white); ts=S("ts",fontName="DV",fontSize=12,leading=16,textColor=colors.HexColor("#dbe7f5"))
st+= [Spacer(1,18*mm),Paragraph("Embedded System Integration<br/>&amp; System-Level Testing Report",tt),Spacer(1,5*mm),Paragraph("Real-Time Electric Vehicle Dashboard &amp; ADAS Warning System on STM32",ts),Spacer(1,52*mm)]
meta=[["Final task","Integration of hardware-interface code, firmware routines and debugging techniques into one validated embedded system"],
["Intern","Saurav Pardeshi – B.Tech Electronics &amp; Telecommunication"],["Programme","Embedded Systems Internship – final deliverable"],
["Target MCU","STM32F103C8 (Cortex-M3, 72 MHz) – UART, I2C, SPI, ADC, input-capture, IWDG"],
["Verification","Software-in-the-loop (SIL) simulation of every peripheral with fault injection; "+f"{N} automated test cases, {npass} passed"],
["Date","25 November 2026"],["Firmware version","1.0.0 (source archive submitted alongside this report)"]]
st.append(tbl([[Paragraph(f"<b>{a}</b>",cell),Paragraph(b,cell)] for a,b in meta],[35,143],head=False,zebra=False))
st+= [Spacer(1,10*mm),P("<b>Contents</b>",h2)]
toc=["1  Executive summary","2  Scope, assumptions and module mapping","3  System requirements","4  System architecture and interfaces","5  Integration strategy","6  Test environment and method","7  Test cases and results","8  Integration challenges and how they were overcome","9  Performance and resource results","10  Limitations and next steps","11  Conclusion","Appendix A  Supporting code excerpts","Appendix B  Build, run and reproduce","Appendix C  Hardware / Wokwi validation checklist"]
st+= [P("<br/>".join(toc),S("toc",fontName="DV",fontSize=9.2,leading=14))]
st+= [NextPageTemplate("main"),PageBreak()]

# ---------------- 1 exec summary ----------------
st+=[P("1  Executive summary",h1)]
st+=[P(f"This report documents the final stage of my embedded-systems internship project, the <b>Real-Time EV Dashboard &amp; ADAS Warning System</b>. Over the earlier weeks I built and debugged the individual building blocks – UART communication, I2C sensor interfacing, SPI display driving, ADC sensing and the warning logic. In this task I merged them into <b>one integrated firmware</b>, planned the integration, built a verification environment, ran a structured test campaign, and fixed the problems that only appeared once the modules ran together.")]
st+=[P("<b>The integrated system</b> reads an MPU-6050 accelerometer (I2C), an HC-SR04 ultrasonic range sensor (GPIO/timer capture), a speed input and a battery-voltage input (ADC); computes time-to-collision (TTC), harsh-braking and impact events; drives a buzzer, status LEDs and an 8-digit MAX7219 dashboard (SPI); and streams CRC-protected telemetry and accepts commands over UART. A cooperative scheduler runs ten periodic tasks under an independent watchdog, and a fault manager degrades gracefully when a sensor fails.")]
rows=[["Headline result","Value"],
["Automated test cases (unit / integration / system)",f"{count('Unit')} / {count('Integ')} / {count('System')}  →  <b>{npass} of {N} passed</b>"],
["Forward-collision alert latency after true TTC threshold (40 randomised trials)","min 46 ms, mean 79 ms, p95 112 ms, <b>max 120 ms</b> (limit 250 ms)"],
["Deadline misses in 10-minute soak / TX frames dropped / watchdog resets","0 / 0 / 0"],
["Sensor-fault detection (IMU NACK) / range-sensor stuck / battery-sense open",f"{mf['latency.imu_fault_detect_ms']:.0f} ms / {mf['latency.range_fault_detect_ms']:.0f} ms / ≤ 500 ms – each with automatic recovery"],
["Worst start jitter of the 10 ms safety tasks over 10 minutes",f"{max(int(t['max_jitter_us']) for t in tasks if int(t['period_ms'])<=10)} µs (limit 2000 µs)"],
["Peripheral-blocking CPU time",f"{mf['soak.busy_pct']:.2f} % (limit 5 %), dominated by I2C reads"]]
st.append(tbl(rows,[105,73]))
st+=[Spacer(1,4),P("<b>Main integration challenges</b> (Section 8): (1) a blocking ultrasonic read stalled the whole scheduler – 1300 deadline misses in 20 s until it was made interrupt/poll driven; (2) an I2C bus-timeout that was too long turned a recoverable bus fault into 19 deadline misses; (3) the first outlier filter missed the latency requirement (315 ms worst case) and was replaced by a gated α-β tracker that cut worst-case latency to 120 ms and closing-speed error from 165 mm/s to 22 mm/s RMS; (4) the display was rewriting 82 SPI frames/s and could not recover from corrupted registers – now 8.5 frames/s with a 2 s self-heal; (5) stale or missing sensor data could silently read as ‘no obstacle’ – now the ADAS reports UNAVAILABLE (fail-safe).")]
st+=[P("<b>Honesty note on the evidence.</b> All results in this report come from running the real firmware sources against a software model of the hardware (Section 6). The peripheral models and timing costs are explicit and reproducible, but this is not a substitute for bench measurements; Section 10 and Appendix C list exactly what must still be confirmed on the physical board or in the Wokwi simulator.",small)]

# ---------------- 2 scope ----------------
st+=[P("2  Scope, assumptions and module mapping",h1)]
st+=[P("The task asks for a complete embedded-system model that combines hardware-interface code, firmware routines and debugging techniques, validated against the original requirements. I used my EV Dashboard &amp; ADAS project as the integration vehicle because it naturally needs every technique practised during the internship: the three serial protocols (UART/I2C/SPI), analogue sensing, timing-critical logic and defensive debugging.")]
st+=[P("Each module below corresponds to a building block from the earlier tasks (serial protocols, sensor interfacing, display driving, debugging). During integration they were refactored behind a common hardware-abstraction layer so that exactly the same source runs in the simulator and on the STM32.")]
rows=[["Module (earlier work)","Interface","Role in the integrated system","Source file"],
["I2C sensor driver","I2C1 @ 400 kHz, addr 0x68","MPU-6050 accelerometer: ±4 g, brake / impact detection","mpu6050.c, sensors.c"],
["Ultrasonic ranging","TRIG GPIO + ECHO input-capture","Distance and closing speed for forward-collision warning","range.c"],
["ADC acquisition","ADC1 ch0 / ch1","Speed input (potentiometer) and battery pack voltage / SoC","sensors.c"],
["SPI display driver","SPI1 @ 1 MHz, MAX7219","8-digit dashboard: speed, SoC, ADAS state, E-codes","display.c"],
["UART communication","USART1 115200 8N1","CRC-8 telemetry at 5 Hz, command line (STATUS?, LOG?, RST, HELP)","uart_log.c, ring.c, crc8.c"],
["Warning logic","(software)","TTC classification, debounce, driver-reaction metric","adas.c, alert.c"],
["Debug / robustness work","(software)","Fault manager, event log, watchdog, scheduler statistics","fault.c, evlog.c, sched.c, app.c"]]
st.append(tbl(rows,[36,36,70,36]))
st+=[Spacer(1,4),P("<b>Assumptions.</b> (a) Distance and speed are lab-scale quantities: the HC-SR04 range (2 cm–4 m) is used 1:1, so closing speeds in the tests are 1–1.5 m/s while the speed input is an independent 0–120 km/h value used only to gate the warning; (b) IMU and range stimuli are scripted independently – physical consistency between them is not enforced; (c) battery is a 4-cell Li-ion pack measured through a 47 kΩ / 10 kΩ divider; (d) the SoC table is a generic voltage-to-capacity approximation, not a calibrated curve.",small)]

# ---------------- 3 requirements ----------------
st+=[P("3  System requirements",h1)]
st+=[P("The requirements were fixed <u>before</u> the test campaign and every test case is traced to at least one of them (Section 7). Each is measurable.")]
rows=[["ID","Requirement","Acceptance criterion","Verified by"],
["R01","Speed acquisition","0–120 km/h, error ≤ 1 km/h, sampled every 20 ms; below 5 km/h only proximity alarm","U09, I04"],
["R02","Battery monitoring","Pack-voltage error ≤ 1.5 %; SoC monotonic table; LOW &lt; 20 %, CRIT &lt; 10 % with hysteresis (no chatter); open sense line → E03","U07, U08, I05, I13, S05"],
["R03","Impact and harsh braking","Impact ≥ 2.5 g latched ≤ 100 ms and held until RST; braking ≥ 0.6 g for 40 ms logged ≤ 100 ms","U03, U04, I01, I02, S04"],
["R04","Forward-collision warning","ADV ≤ 3 s, WARN ≤ 2 s, CRIT ≤ 1 s TTC or ≤ 150 mm; alert ≤ 250 ms after threshold; zero false alarms in 60 s noisy following scenario","U10–U13, U21, U22, I03, I04, S02, S03, S12"],
["R05","Dashboard","Speed, SoC, ADAS state and E-code shown on MAX7219; update ≤ 150 ms; corrupted registers restored ≤ 2.1 s; boot ≤ 500 ms","U14–U16, I11, S13"],
["R06","Telemetry and commands","5 Hz ± 5 %, CRC-8, no partial frames, no sequence gaps; commands STATUS?/LOG?/RST/HELP; robust to garbage input","U01, U02, U17, U18, U20, I10, I14, S08, S11"],
["R07","Fault management","IMU/range/battery fault flagged ≤ 200/400/500 ms, shown as E01–E04, ADAS fail-safe (UNAVAILABLE), automatic recovery ≤ 1 s","U05, U06, U19, I06–I09, I12–I14, S06"],
["R08","Real-time behaviour","0 deadline misses; start jitter of ≤ 10 ms tasks ≤ 2 ms; blocking CPU time ≤ 5 %","I08, I10, S02, S06, S08–S10"],
["R09","Watchdog","Hang → reset ≤ 100 ms; reboot cause logged; service restored ≤ 400 ms; no spurious resets","S07"],
["R10","Resources","Flash ≤ 32 KB (50 % of 64 KB), static RAM ≤ 10 KB (50 % of 20 KB)","Section 9 (indicative)"]]
st.append(tbl(rows,[10,32,98,38]))

# ---------------- 4 architecture ----------------
st+=[P("4  System architecture and interfaces",h1)]
st.append(fig("fig/f1_arch.png",150,"Figure 1 – Layered software architecture. Sensor tasks write the shared vehicle state; consumer tasks read it. Only the HAL touches registers."))
st+=[P("4.1  Hardware interface and pin allocation",h2),P("Proposed allocation for an STM32F103C8 (“Blue Pill”) board. Pins are chosen so that each peripheral sits on its default alternate-function pin and no two functions collide.")]
rows=[["Function","Peripheral","Pin(s)","Notes"],
["MPU-6050 SCL / SDA","I2C1","PB6 / PB7","400 kHz, 4.7 kΩ pull-ups, INT unused"],
["MAX7219 SCK / DIN / CS","SPI1 (MOSI only)","PA5 / PA7 / PA4","1 MHz, CS driven as GPIO so frames can be checked"],
["Telemetry / CLI","USART1","PA9 TX / PA10 RX","115200 8N1, TX interrupt-driven from ring buffer"],
["Speed input","ADC1 ch0","PA0","potentiometer, 12-bit"],
["Battery sense","ADC1 ch1","PA1","47 kΩ / 10 kΩ divider → 16.8 V = 2.95 V"],
["HC-SR04 ECHO / TRIG","TIM1 CH1 capture / GPIO","PA8 / PB0","echo width captured in hardware, polled every 5 ms"],
["Buzzer, LEDs ADV/WARN/CRIT/BATT","GPIO","PB12, PB13, PB14, PB15, PB1","active high, 330 Ω"],
["Watchdog / tick","IWDG / SysTick","–","IWDG 100 ms, SysTick 1 kHz wakes the idle WFI"]]
st.append(tbl(rows,[44,38,34,62]))
st+=[P("4.2  Shared data bus and ownership rules",h2),P("All inter-module communication goes through one record, <font name='DVM'>vehicle_t g_veh</font>, in main-loop context only (interrupts touch only HAL buffers). Three rules keep the integration safe: (1) <b>single writer</b> – each field has exactly one producing task; (2) <b>every value carries a timestamp and sequence number</b>, so consumers can detect stale data and process each new sample exactly once; (3) <b>units are fixed and documented</b> (mm, mm/s, mg, deci-km/h, mV).")]
st+=[P("4.3  Task schedule",h2),P("A table-driven cooperative scheduler releases tasks in priority order. Per-task execution time, start jitter and deadline misses are recorded by the scheduler itself; the figures below come from the 10-minute soak test (TC-S08).")]
purpose={"imu":"Read MPU-6050, retry/recover, frozen-data check","speed":"ADC speed, 4-sample moving average","range":"HC-SR04 trigger / capture poll, tracker","adas":"TTC, brake and impact events, FCW state machine","alert":"Buzzer and LED patterns","batt":"ADC, EMA filter, SoC, hysteresis","cli":"Parse commands, stream LOG? entries","display":"MAX7219 dirty-digit update, 2 s refresh","telem":"Emit 5 Hz CRC-8 frame","superv":"Clear scheduler fault after 2 s quiet"}
rows=[["Priority","Task","Period","Purpose","Runs (10 min)","Max exec","Mean exec","Max start jitter","Misses"]]
for i,t in enumerate(tasks,1):
    rows.append([i,t["task"],f"{t['period_ms']} ms",purpose.get(t["task"],""),t["runs"],f"{t['max_exec_us']} µs",f"{t['mean_exec_us']} µs",f"{t['max_jitter_us']} µs",t["misses"]])
st.append(tbl(rows,[12,15,14,55,18,17,17,18,12]))
st+=[P("‘Exec’ is peripheral-blocking time only (I2C/SPI/ADC transfers as modelled); CPU compute time is not included and must be measured on target (Section 10).",small)]
st+=[P("4.4  Safety path and latency budget",h2),P("The path that matters most to the driver is range echo → alert. Each stage is bounded, and the sum was verified end-to-end rather than assumed.")]
st.append(fig("fig/f3_path.png",165,"Figure 2 – Forward-collision warning path with the measured end-to-end latency."))
st+=[P("4.5  UART protocol",h2),P("Every message is a framed ASCII line <font name='DVM'>$&lt;body&gt;*&lt;CRC8-hex&gt;\\r\\n</font>; the CRC-8 (polynomial 0x07) covers the body. A frame is queued <b>all-or-nothing</b>: if the 512-byte TX ring cannot take the whole frame it is dropped and counted, so a receiver never sees a truncated frame that could pass for valid.")]
rows=[["Message","Direction","Body format","Meaning"],
["Telemetry","MCU → PC","EVD,seq,speed_dkmh,soc,dist_mm,ttc_ms,level,flags","5 Hz; dist/ttc = -1 if none; flags: b0 crash, b1 braking, b2 low batt, b3 crit batt, b4+ fault mask"],
["STATUS?","PC → MCU","ACK,STATUS,version,uptime_s,level,faults,tx_dropped,rx_errors","Snapshot of health counters"],
["LOG?","PC → MCU","LOG,index,t_ms,code,arg … ACK,LOG,count","Event log streamed one entry per 10 ms"],
["RST","PC → MCU","ACK,RST","Clears the latched crash flag"],
["HELP / other","PC → MCU","ACK,HELP,… / NAK,UNKNOWN / NAK,OVERFLOW","Lines longer than 31 characters are rejected"]]
st.append(tbl(rows,[20,18,70,70]))
st+=[P("4.6  Fault management matrix",h2)]
rows=[["Fault","Detection","Reaction","Recovery"],
["E01 IMU","3 consecutive failed reads (after 3 retries and an I2C bus-clear), or 25 identical 14-byte frames","Brake/impact detection suspended; FCW continues; E01 on display","Re-init (device reset, wake, ±4 g) every 500 ms until success"],
["E02 Range sensor","3 consecutive echo timeouts (45 ms each) or data older than 300 ms","ADAS = UNAVAILABLE (‘-’ on display, 1 Hz warning LED) – never ‘clear’","Cleared by the first valid echo"],
["E03 Battery sense","3 consecutive ADC readings < 20 or > 4075 counts","SoC replaced by E03; flags in telemetry","Cleared by the first valid reading"],
["E04 Scheduler","Task finishes after its next release","Shown for ≥ 2 s; counted per task","Auto-clears after 2 s without a miss"],
["Hang","IWDG expires (100 ms); kick withheld if any task blocks &gt; 50 ms","Hardware reset; boot cause logged","Normal start-up; state is rebuilt"]]
st.append(tbl(rows,[24,56,52,46]))
st+=[P("4.7  Porting to the STM32",h2),P("The firmware calls only the 20 functions in <font name='DVM'>hal.h</font>. The simulator implements them in <font name='DVM'>port/hal_sim.c</font>; a target port implements the same functions with the STM32 HAL/LL drivers. The mapping below is the porting plan; the target port itself has not been compiled or run in this submission.")]
rows=[["HAL function","STM32 implementation"],
["hal_micros / hal_millis / hal_idle","DWT cycle counter (or TIM2 at 1 MHz) / SysTick counter / __WFI()"],
["hal_i2c_read / write / bus_recover","HAL_I2C_Mem_Read/Write with 2 ms timeout; recovery = bit-banged 9 × SCL on PB6 then re-init peripheral"],
["hal_spi_cs / hal_spi_xfer16","GPIO PA4 + HAL_SPI_Transmit (16-bit frame, blocking)"],
["hal_adc_read","HAL_ADC single conversion on selected channel, 239.5-cycle sample time"],
["hal_range_trigger / poll_us","10 µs GPIO pulse on PB0; TIM1 CH1 input-capture on both edges; poll reads the captured pulse width"],
["hal_uart_register_tx_source / tx_irq_enable","USART1 TXE interrupt pulls bytes from uart_log’s ring buffer; RXNE ISR fills a small RX ring"],
["hal_wdt_start / kick, hal_reset_cause","IWDG with LSI prescaler for 100 ms; RCC reset-flag register"]]
st.append(tbl(rows,[58,120]))

# ---------------- 5 integration strategy ----------------
st+=[P("5  Integration strategy",h1)]
st+=[P("I used a <b>bottom-up, incremental</b> approach with a simulated hardware layer, so every step could be exercised and repeated deterministically. Each stage has an entry condition (what must already pass) and an exit criterion; a failing stage is fixed before moving on.")]
rows=[["Stage","What is integrated","Exit criterion","Test cases"],
["0  Platform","HAL interface, simulated peripherals with cost model and fault injection, scheduler, test harness","Virtual clock, I2C/SPI/UART/ADC/echo models behave as the datasheets (Section 6)","–"],
["1  Single modules","CRC, ring buffer, MPU driver, tracker, ADAS classifier, display formatter, parser, event log, fault priority","All unit tests pass in isolation","TC-U01–U22"],
["2  Pairwise","IMU→ADAS, range→ADAS→alert, speed→ADAS gate, battery→display/telemetry, UART↔every producer","Data, units and timing agree across each interface","TC-I01–I05, I10, I14"],
["3  Degraded modes","Each sensor and bus fault injected while the rest of the system runs","Fault flagged, safe reaction, automatic recovery, no collateral damage","TC-I06–I09, I11–I13"],
["4  Full system","All ten tasks, all peripherals, realistic noise, concurrent UART traffic, watchdog","Scenario tests and 10-minute soak meet every requirement","TC-S01–S13"],
["5  Regression &amp; mutation","Re-run the whole suite on deliberately defective builds","Suite detects the defects that matter (Section 8.7)","6 variant builds"]]
st.append(tbl(rows,[26,66,56,30]))
st+=[P("5.1  Integration risks identified up front",h2)]
rows=[["Risk","Why it matters when modules are combined","Mitigation designed in"],
["Blocking I/O","One slow driver delays every other task","Non-blocking echo capture, interrupt-driven UART TX, bounded work per task"],
["Shared bus","I2C fault in one device can hang the bus for all","Short bus timeout, retry then 9-clock recovery, bus-clear at init"],
["Stale data","Consumer uses an old value as if current","Timestamps + sequence numbers; ADAS checks age; fail-safe UNAVAILABLE"],
["Noisy sensor into threshold logic","Spikes become false alarms or late alarms","Gated tracker, vote-based escalation, de-escalation hysteresis"],
["Protocol framing","Partial frames or missing CS corrupt SPI/UART data","All-or-nothing UART frames; SPI protocol checker in the simulator"],
["Silent lock-up","System appears alive but does nothing","IWDG with kick withheld on long tasks, boot-cause logging"],
["Unit/scale mismatch","mg vs g, mm vs cm, deci-km/h vs km/h across modules","Fixed units in vehicle.h, conversion unit tests (U04, U08, U09, U11)"]]
st.append(tbl(rows,[34,70,74]))

# ---------------- 6 test environment ----------------
st+=[P("6  Test environment and method",h1)]
st+=[P("6.1  Software-in-the-loop harness",h2),P("The real firmware sources (887 lines of C including headers, no changes between test and ‘production’ builds apart from compile-time flags used for the experiments) are compiled with GCC together with <font name='DVM'>hal_sim.c</font>. The simulator keeps a <b>virtual clock in microseconds</b>; every blocking hardware operation advances that clock by its true bus time, so scheduler jitter, deadline misses and watchdog expiry are reproduced faithfully. Random noise uses a fixed-seed generator, so every run is bit-for-bit repeatable.")]
rows=[["Device model","What is modelled","Cost charged to virtual time","Fault injection"],
["MPU-6050 (I2C)","Register file, sleep bit, ±2/4/8/16 g sensitivity (so a missing config write gives wrong scaling), burst read, noise ±6 LSB, reset bit","22.5 µs per byte (9 clocks at 400 kHz); 17-byte burst = 0.38 ms","NACK, one-shot NACK, absent device, stuck bus (timeout), frozen data, wrong WHO_AM_I"],
["MAX7219 (SPI)","Register file, framing checker (CS low, exactly 16 clocks, CS high, valid register), code-B decode view","16 µs per frame","Register corruption by the test; protocol violations are counted"],
["HC-SR04","Echo width 5.83 µs/mm, 38 ms when no echo, ±noise, random spikes and lost echoes","Blocking read costs the real echo time; non-blocking costs 10 µs","Stuck-high echo, spike rate, lost-echo rate"],
["ADC","12-bit, 3.3 V, divider model, ±2 LSB noise","10 µs per conversion","Forced counts (open/short)"],
["USART1","TX drained at 87 µs/byte by an ISR model pulling from the firmware ring; RX queue","Polled TX costs 87 µs/byte; ISR TX costs the CPU nothing","Garbage and overlong input from the tests"],
["IWDG / GPIO","100 ms watchdog; time-stamped edge log on every output pin","–","Hang injected via a rogue task"]]
st.append(tbl(rows,[26,62,46,44]))
st+=[P("6.2  Test levels",h2),P(f"<b>Unit tests ({count('Unit')})</b> exercise one module through its public function. <b>Integration tests ({count('Integ')})</b> boot the complete firmware and check that data crosses module boundaries correctly, including each sensor’s degraded modes. <b>System tests ({count('System')})</b> are end-to-end scenarios and long-duration runs. Each test records its ID, requirement trace, expected value and the <i>measured</i> value into a CSV that is the source of the tables in Section 7. Timings in the tests are read from the GPIO edge log (e.g. the first rising edge of the CRIT LED), not inferred from internal variables, so they include every stage of the path.")]
st+=[P("6.3  Experiments and mutation builds",h2),P("Beyond pass/fail, an experiment module measures task jitter, filter quality and traffic for the final design and for deliberately defective variants selected with compile-time flags (blocking range read, polled UART, no filtering, the original median filter, full display refresh). Running the same suite against each variant shows whether the tests would actually catch those defects (Section 8.7).")]

# ---------------- 7 results ----------------
st+=[PageBreak(),P("7  Test cases and results",h1)]
st+=[P(f"<b>Summary:</b> {npass} of {N} test cases passed – unit {sum(1 for r in F if r['level']=='Unit' and r['result']=='PASS')}/{count('Unit')}, integration {sum(1 for r in F if r['level']=='Integ' and r['result']=='PASS')}/{count('Integ')}, system {sum(1 for r in F if r['level']=='System' and r['result']=='PASS')}/{count('System')}. The tables list the <i>measured</i> value for every case, copied from the automated run (<font name='DVM'>results_final.csv</font>).")]
def results_table(level,title):
    rows=[["ID","Req","What is verified","Expected","Measured","Result"]]
    ex=[]
    for r in F:
        if r["level"]!=level: continue
        rows.append([r["id"].replace("TC-",""),r["req"],esc(r["description"]),esc(r["expected"]),esc(r["measured"]),f"<b>{r['result']}</b>"])
    t=tbl(rows,[10,17,52,32,52,15],extra=[("TEXTCOLOR",(5,1),(5,-1),GRN)])
    return [P(title,h2),t]
st+=results_table("Unit","7.1  Unit tests")
st+=results_table("Integ","7.2  Integration tests")
st+=results_table("System","7.3  System tests")
st+=[P("7.4  Key scenario evidence",h2)]
st.append(fig("fig/f4_s01.png",150,"Figure 3 – TC-S01, emergency braking. The tracker follows the true distance through noise and spikes; ADAS steps ADV → WARN → CRIT at the TTC thresholds (2 s, 1 s); the buzzer pulses in WARN and is solid in CRIT; the driver brakes (−0.8 g) at t = 2.4 s; the level falls back to CLEAR after the closing speed vanishes."))
r=byid["TC-S01"]; st+=[P(f"Measured in this scenario: driver reaction time logged by the firmware = <b>{mf['scenario.driver_reaction_ms']:.0f} ms</b> from the WARN onset to the braking event. This metric is produced automatically by the ADAS module and is available over the UART log for field analysis.")]
st.append(fig("fig/f7_batt.png",150,"Figure 4 – TC-S05, battery discharge with periodic load-sag pulses. LOW and CRIT are each raised exactly once; hysteresis and the EMA filter prevent chatter."))
st+=[P(f"Telemetry sanity over the 60 s nominal drive (TC-S02): 300 frames received in 60 s (expected 300), no sequence gaps, no invalid CRC. In the 10-minute soak (TC-S08) the stream delivered {byid['TC-S08']['measured'].split('frames=')[1].split()[0]} frames with no gaps.")]

# ---------------- 8 challenges ----------------
st+=[PageBreak(),P("8  Integration challenges and how they were overcome",h1)]
st+=[P("Every challenge below was found by the test campaign or the experiment module, not by inspection, and every number is a measurement from the build variants described in Section 6.3.")]
bj=M["range_blocking"]; fj=mf
st+=[P("8.1  Blocking ultrasonic read starves the scheduler",h2),
P(f"<b>Symptom.</b> Each module worked alone. Integrated, with the range sensor read in the conventional blocking style (trigger, then wait for the echo pulse, as <font name='DVM'>pulseIn()</font> does), eleven tests failed in the variant build. <b>Root cause.</b> The HC-SR04 holds ECHO high for up to 38 ms when nothing is in front of it, and the blocking read waits for the whole pulse. The ADAS and alert tasks run every 10 ms, so they were delayed by up to {bj['exp_sched.adas_max_jitter_us']/1000:.1f} ms; the scheduler recorded <b>{bj['exp_sched.total_misses']:.0f} deadline misses in 20 s</b> and the range read alone consumed {bj['exp_sched.range_busy_pct']:.0f} % of CPU time. <b>Fix.</b> The measurement was split into <i>trigger</i> and <i>collect</i>: the task fires TRIG, returns, and polls the captured pulse width every 5 ms, with a 45 ms timeout; on the STM32 the width is captured in hardware by TIM1. <b>Result.</b> Range CPU use dropped to {fj['exp_sched.range_busy_pct']:.2f} %, the ADAS-task start jitter to {fj['exp_sched.adas_max_jitter_us']/1000:.2f} ms, and misses to 0.")]
st.append(fig("fig/f6_sched.png",150,"Figure 5 – Left: worst start jitter of the 10 ms ADAS task for three I/O designs (log scale). Right: deadline misses in 20 s when the I2C bus is blocked once per second with a 5 ms versus a 2 ms bus timeout."))
st+=[P("8.2  I2C bus fault turns into a real-time fault",h2),
P(f"<b>Symptom.</b> TC-I08 (stuck I2C bus) checks that the 9-clock bus-clear restores the IMU, but the timing experiment showed that the bus timeout value decides whether this is safe. <b>Root cause.</b> With a 5 ms HAL timeout, the HAL waits 5 ms for each failed transfer; the driver makes two failed attempts before clearing the bus, so one IMU read could block for 10 ms, longer than the 10 ms period of the tasks behind it: <b>{mf['exp_i2c.timeout5000_misses']:.0f} deadline misses in 20 s</b> and a spurious E04. <b>Fix.</b> The bus timeout is set to 2 ms (five times the 0.38 ms worst-case transfer), so the worst blocked read is 4 ms plus recovery. <b>Result.</b> {mf['exp_i2c.timeout2000_misses']:.0f} misses with the bus stuck every second, and the IMU keeps delivering samples (TC-I08: ≥ 20 samples in 0.5 s, no fault).")]
st+=[P("8.3  Outlier filter: from median-of-3 to a gated α-β tracker",h2),
P(f"<b>Symptom.</b> TC-S12 (alert latency over 40 randomised trials) failed: worst case <b>315 ms</b> against the 250 ms limit, and some alerts arrived 182 ms <i>early</i>. <b>Investigation.</b> A trace of one bad trial showed two effects of the original median-of-3 + EMA design: the median of a falling sequence is the <i>previous</i> sample, so the distance lags by one sample (50 ms of TTC); and when a lost echo occurred just before a threshold, the velocity estimate collapsed for several samples and the confirmation counter restarted. <b>First attempt.</b> Changing escalation to ‘2 of the last 3 samples’ (build v1) did not change the result (still 315 ms) – the root cause was the estimator, not the voting. <b>Fix.</b> Replace the median with an α-β tracker (α = ½, β = ⅙) with an innovation gate of 300 mm: an echo that disagrees with the prediction is treated as an outlier and the track <i>coasts</i> for up to three samples; a persistent change is accepted as real. The 2-of-3 vote was kept because it costs nothing in clean conditions.")]
rows=[["Estimator (build tag)","Closing-speed RMS error (1.5 m/s approach, 1 %+2 % corrupt echoes)","Worst error","Samples with error &gt; 300 mm/s","TC-S12 latency min / mean / p95 / max","False alarms in 180 s stress test (5 % + 5 % corrupt)"]]
def lat(tag): 
    r=[x for x in res[tag] if x["id"]=="TC-S12"][0]; return r["measured"].replace(" ms","")
def row(tag,label): m=M[tag]; return [label,f"{m['exp_filter.vel_rms_mmps']:.0f} mm/s",f"{m['exp_filter.vel_max_abs_err_mmps']:.0f} mm/s",f"{m['exp_filter.vel_err_gt300_pct']:.2f} %",lat(tag),f"{m['exp_filter.stress_alert_edges_180s']:.0f}"]
rows+= [row("raw","Raw difference (RANGE_FILTER=0)"),row("v0_median_consec","Median-of-3 + EMA, 2 consecutive (original)"),row("v1_median_vote","Median-of-3 + EMA, 2-of-3 vote"),row("final","<b>α-β tracker + gating, 2-of-3 vote (final)</b>")]
st.append(tbl(rows,[40,34,20,24,36,24]))
st.append(fig("fig/f5_filter.png",160,"Figure 6 – Left: closing-speed estimate for the three estimators during the S01 approach. Right: CRIT alert latency histogram before (median) and after (tracker)."))
st+=[P("Two honest observations from this table. First, the raw difference has an RMS speed error of ~2.9 m/s yet produced no false alarms in the stress test, because the debounce and 2-of-3 vote already suppress isolated spikes; the tracker’s benefit is a <i>trustworthy TTC value</i> (error 22 mm/s) and the removal of the lag, not just fewer false alarms. Second, at 10 % corrupted echoes the final design still raised one alert in 180 s (two logged events), so the false-alarm requirement is verified at the specified 3 % corruption rate (TC-S03: zero) and is a known limitation beyond it.",small)]
st+=[P("8.4  Display: SPI traffic and corrupted registers",h2),
P(f"<b>Symptom.</b> A naive display task (build variant <font name='DVM'>disp_full</font>) rewrites all eight digits every cycle and never re-sends the configuration registers. <b>Findings.</b> Measured SPI traffic was <b>{M['disp_full']['soak.spi_frames_per_s']:.1f} frames/s</b>; and TC-I11 showed that if EMI or a brown-out flips the MAX7219’s decode-mode or scan-limit register, the display stays garbled indefinitely because nothing is ever rewritten. <b>Fix.</b> Only digits that changed are written (a shadow copy is kept), and every 2 s the configuration and all digits are refreshed. <b>Result.</b> {mf['soak.spi_frames_per_s']:.1f} frames/s in normal driving (a {M['disp_full']['soak.spi_frames_per_s']/mf['soak.spi_frames_per_s']:.0f}× reduction) and a corrupted display heals in {mf['latency.display_heal_ms']:.0f} ms (limit 2.1 s). The simulator’s SPI protocol checker also guards the frame format: it flags any transfer without chip-select or not exactly 16 clocks (TC-U16).")]
st+=[P("8.5  Stale or missing data must not look like ‘all clear’",h2),
P("<b>Risk.</b> With the range sensor stuck, an ADAS that only reacts to new samples would keep its last level (or CLEAR) because nothing new arrives – the worst possible silent failure for a warning system. <b>Fix.</b> The shared record carries timestamps; ADAS checks that range data is younger than 300 ms and that no E02 fault is active, otherwise it enters an explicit <b>UNAVAILABLE</b> state, shown as ‘-’ on the display with a 1 Hz LED. The state is cleared by the first valid echo. TC-I06 and TC-S06 verify detection within 400 ms and automatic recovery; TC-I07 shows that an IMU failure does <i>not</i> disable the collision warning (graceful degradation).")]
st+=[P("8.6  UART: framing policy and traffic shaping",h2),
P(f"Two decisions came from integration, not from the standalone UART exercise. (1) <b>All-or-nothing frames</b>: when the TX ring is full the whole frame is dropped and counted, so a receiver cannot be fooled by a truncated line (TC-I10 and the fuzz test TC-S11 check that every transmitted frame has a valid CRC). (2) <b>LOG? streams</b> one entry per 10 ms task call instead of dumping 16 entries at once, so a command cannot overflow the ring or monopolise the CPU. In the experiment a polled blocking UART raised the ADAS-task jitter to {M['uart_blocking']['exp_sched.adas_max_jitter_us']/1000:.2f} ms and the peripheral-blocking load from {mf['soak.busy_pct']:.1f} % to {M['uart_blocking']['soak.busy_pct']:.1f} %, but did <i>not</i> violate any requirement at the current frame size (≈ 40 bytes at 5 Hz). So the interrupt-driven ring buffer is the more scalable design rather than one that the present requirements strictly force; I report it that way.")]
st+=[P("8.7  Would the test suite catch these defects? (mutation check)",h2),
P("To check the tests themselves, the complete suite was run against each defective variant. A defect that no test notices would indicate a gap in the suite.")]
vars_=[("final","Final design",""),("range_blocking","Blocking HC-SR04 read","RANGE_BLOCKING=1"),("v0_median_consec","Original median filter, 2 consecutive","RANGE_FILTER=2, ESC_VOTE=0"),("v1_median_vote","Median filter + 2-of-3 vote","RANGE_FILTER=2"),("raw","No filtering","RANGE_FILTER=0"),("uart_blocking","Polled blocking UART","UART_BLOCKING=1"),("disp_full","Display rewrites all digits","DISPLAY_DIRTY=0")]
rows=[["Build","Flag","Passed","Failed test IDs","Detected?"]]
for tag,label,flag in vars_:
    R=res[tag]; p=sum(r["result"]=="PASS" for r in R); fl=[r["id"].replace("TC-","") for r in R if r["result"]=="FAIL"]
    det="–" if tag=="final" else ("<b>Yes</b>" if fl else "<b>No</b> – not a requirement violation")
    rows.append([label,flag,f"{p}/{len(R)}",(", ".join(fl) if fl else "none"),det])
st.append(tbl(rows,[44,36,14,60,24]))
st+=[P("The suite flags the defects that violate requirements (blocking read, median-filter latency). It deliberately does <i>not</i> fail on the polled UART, the raw estimator or the full display refresh, because those designs still meet the stated requirements; their cost is shown by the metrics in Sections 8.3, 8.4 and 8.6 instead. This is a real boundary of the test suite: requirements on code quality or traffic volume (for example ‘SPI ≤ 20 frames/s’) would have to be added to make them gate failures.",small)]
st+=[P("8.8  Lessons from the test code itself",h2)]
st+=B(["Two of my own tests were wrong, not the firmware: one parsed the SoC digits together with the adjacent ADAS digit (read 693 instead of 69) and one expected 400 telemetry frames in 20 s instead of 100. Both were caught because the failing run printed the measured values next to the expected ones – a reason to record measurements, not just pass/fail.",
"Timing assertions are taken from the GPIO edge log; asserting on internal variables would have hidden the extra task-scheduling delay between the ADAS decision and the buzzer.",
"Fixed random seeds made each failure reproducible, which is what allowed the single bad S12 trial to be traced to its root cause."])

# ---------------- 9 performance ----------------
st+=[P("9  Performance and resource results",h1)]
rows=[["Metric","Measured","Limit","Source"],
["CRIT alert latency (worst of 40 trials)","120 ms","≤ 250 ms","TC-S12"],
["ADV / WARN / CRIT latency, clean 1 m/s approach",f"{mf['latency.adv_ms']:.0f} / {mf['latency.warn_ms']:.0f} / {mf['latency.crit_ms']:.0f} ms","≤ 250 ms","TC-I03"],
["Brake event / impact latch",f"{mf['latency.brake_event_ms']:.0f} ms / {mf['latency.impact_event_ms']:.0f} ms","≤ 100 ms","TC-I01, I02"],
["IMU fault detect / recovery",f"{mf['latency.imu_fault_detect_ms']:.0f} ms / {mf['latency.imu_recover_ms']:.0f} ms","≤ 200 ms / ≤ 1 s","TC-I07"],
["Range sensor fault → UNAVAILABLE / recovery",f"{mf['latency.range_fault_detect_ms']:.0f} ms / {mf['latency.range_recover_ms']:.0f} ms","≤ 400 ms / ≤ 500 ms","TC-I06"],
["Display self-heal",f"{mf['latency.display_heal_ms']:.0f} ms","≤ 2100 ms","TC-I11"],
["Cold boot to first valid telemetry frame",f"{mf['latency.boot_first_frame_ms']:.0f} ms","≤ 500 ms","TC-S13"],
["Peripheral-blocking CPU time (10 min)",f"{mf['soak.busy_pct']:.2f} %","≤ 5 %","TC-S10"],
["Worst start jitter, 10 ms tasks","< 1 ms","≤ 2 ms","TC-S09"]]
st.append(tbl(rows,[72,40,36,30]))
st+=[P("9.1  Resource estimate (indicative)",h2),P("A cross-compiler for Cortex-M was not available in my environment, so I could not build for the target. As an indication, the firmware modules (without the HAL, startup code and C library) compile with GCC <font name='DVM'>-Os</font> for a 64-bit host to <b>about 9.7 KB of code and 2.0 KB of static RAM</b> (code size on Thumb-2 is normally smaller; 64-bit pointers inflate the RAM figure). Allowing 15 KB for the HAL/CMSIS and a newlib-nano <font name='DVM'>snprintf</font> and 2 KB for the stack, the expected totals are ≈ 25 KB flash and ≈ 6 KB RAM, inside the R10 budget of 32 KB / 10 KB. <b>These numbers must be re-measured with arm-none-eabi-size</b>; I therefore treat R10 as ‘indicatively met’, not verified.")]
rows=[["Module","Code (bytes)","Static RAM (bytes)"],["adas","1280","56"],["uart_log + ring + crc8","2281","608"],["sensors (speed, battery, IMU glue)","1211","32"],["range (tracker)","930","53"],["mpu6050","752","30"],["display","802","16"],["sched (task table + statistics)","748","928"],["app, alert, fault, evlog","1702","292"],["<b>Total</b>","<b>≈ 9.7 KB</b>","<b>≈ 2.0 KB</b>"]]
st.append(tbl(rows,[80,48,50]))

# ---------------- 10 limitations ----------------
st+=[P("10  Limitations and next steps",h1)]
st+=B(["<b>Simulation, not hardware.</b> All timing is derived from the cost model of Section 6. Real I2C clock stretching, ADC settling, ultrasonic reflections, buzzer coupling into supply rails and EMI are not modelled.",
"<b>CPU compute time is not measured.</b> Only peripheral-blocking time is charged. On target the DWT cycle counter should be used to measure each task’s execution time; at 72 MHz I expect it to be small compared with the I2C transfer but this is unverified.",
"<b>Scripted stimuli.</b> IMU and range inputs are independent scripts; a physically consistent vehicle model would be a better system-level stimulus.",
"<b>Scaled distances.</b> The ultrasonic sensor limits the demonstrator to metre-scale distances; real ADAS needs radar/camera ranges and calibrated thresholds.",
"<b>SoC accuracy.</b> The voltage-to-SoC table ignores load, temperature and ageing.",
"<b>Beyond 3 % corrupted echoes</b> false alarms are possible (Section 8.3); a longer confirmation window trades off latency.",
"<b>Not yet done:</b> compile for Cortex-M with <font name='DVM'>arm-none-eabi-gcc</font> and record real flash/RAM; implement the target HAL port; repeat the key scenarios on the board and/or in the Wokwi simulator (Appendix C); add static analysis (cppcheck/MISRA subset) to the build."])

# ---------------- 11 conclusion ----------------
st+=[P("11  Conclusion",h1),P(f"The task’s objective was to integrate multiple components and validate the result against the initial requirements. The deliverable achieves this in four steps: a layered architecture in which seven earlier modules communicate through one timestamped data record; a documented, staged integration plan; a deterministic verification environment with fault injection; and a {N}-case test campaign that all passes, supported by experiments that expose the integration problems (blocking I/O, bus timeouts, estimator lag, display robustness, stale data) and show measured before/after improvements. The most valuable lesson was that the defects that mattered were <i>interaction</i> problems – timing and data-age issues between modules that were each correct on their own – which is exactly what an integration phase is for.")]

# ---------------- Appendices ----------------
st+=[PageBreak(),P("Appendix A  Supporting code excerpts",h1),P("Excerpts are copied unchanged from the submitted source archive. They are the places where the integration decisions are implemented.",small)]
a,b=find("src/app.c","void ev_init(void)",30); st+=codeblock("src/app.c",a,b,"A.1  app.c – wiring tasks into the scheduler and starting the watchdog (task table in priority order)")
a,b=find("src/app.c","void ev_step(void)",5); st+=codeblock("src/app.c",a,b,"A.2  app.c – main-loop step: the watchdog is kicked only when no task blocked too long")
a,b=find("src/sched.c","bool sched_run(void)",26); st+=codeblock("src/sched.c",a,b,"A.3  sched.c – cooperative scheduler with jitter / WCET / deadline-miss accounting")
a,b=find("src/range.c","bool track_update",26); st+=codeblock("src/range.c",a,b,"A.4  range.c – gated α-β tracker (fix for the latency requirement, Section 8.3)")
a,b=find("src/range.c","void range_task(uint32_t now)",24); st+=codeblock("src/range.c",a,b,"A.5  range.c – non-blocking trigger/collect structure (fix for Section 8.1)")
a,b=find("src/adas.c","void adas_task(uint32_t now)",40); st+=codeblock("src/adas.c",a,b,"A.6  adas.c – fail-safe on stale data, 2-of-3 escalation vote, de-escalation hysteresis",maxlines=44)
a,b=find("src/mpu6050.c","int mpu_read",20); st+=codeblock("src/mpu6050.c",a,b,"A.7  mpu6050.c – retry, bus recovery and frozen-data detection")
a,b=find("src/uart_log.c","bool uart_send_frame",14); st+=codeblock("src/uart_log.c",a,b,"A.8  uart_log.c – all-or-nothing framed transmit")
a,b=find("src/display.c","void display_task",12); st+=codeblock("src/display.c",a,b,"A.9  display.c – dirty-digit update with periodic full refresh (Section 8.4)")
st+=codeblock("include/hal.h",None,None,"A.10  hal.h – the complete hardware-abstraction interface",maxlines=40)
a,b=find("include/config.h","#define P_IMU_MS",40); st+=codeblock("include/config.h",a,b,"A.11  config.h – timing and threshold configuration (excerpt)",maxlines=42)
a,b=find("tests/integ.inc","/* I03 range",8); st+=codeblock("tests/integ.inc",a,b,"A.12  Test code – TC-I03: latency measured from the GPIO edge log against the analytic threshold crossing",maxlines=10)
st+=[P("Appendix B  Build, run and reproduce",h1)]
st+=[Preformatted("""# Final build + full test suite (49 cases) -> out/results_final.csv, out/metrics_final.csv
make VARIANT=final run

# Mutation / experiment builds (same suite, one defect enabled each)
make VARIANT=range_blocking   DEFS="-DRANGE_BLOCKING=1" run
make VARIANT=v0_median_consec DEFS="-DRANGE_FILTER=2 -DESC_VOTE=0" run
make VARIANT=v1_median_vote   DEFS="-DRANGE_FILTER=2" run
make VARIANT=raw              DEFS="-DRANGE_FILTER=0" run
make VARIANT=uart_blocking    DEFS="-DUART_BLOCKING=1" run
make VARIANT=disp_full        DEFS="-DDISPLAY_DIRTY=0" run

python3 make_figs.py && python3 build_report.py      # regenerate figures and this PDF

Source layout:  include/ (headers, config)   src/ (firmware, 887 lines)   port/hal_sim.c (simulated hardware)
                tests/ (framework, unit.inc, integ.inc, system.inc, exp.inc)""",S("pre",fontName="DVM",fontSize=7,leading=9))]
st+=[P("Appendix C  Hardware / Wokwi validation checklist",h1),P("To move from simulation to bench evidence, repeat these checks on the board or in Wokwi and add the results (serial log, screenshot or logic-analyser capture) next to the corresponding simulated result.",small)]
rows=[["#","Check","How to observe","Pass criterion","Simulated reference"],
["1","I2C WHO_AM_I and ±4 g scaling","Serial print of ax/ay/az at rest; board flat","az = 1000 ± 50 mg","TC-U03/U04"],
["2","Echo timing and range accuracy","Ruler at 20, 50, 100, 200 cm","error ≤ 1 cm; no task jitter","TC-U11, TC-S09"],
["3","Alert latency","Logic analyser: object edge → buzzer pin","≤ 250 ms","TC-S12 (max 120 ms)"],
["4","Display layout and E-codes","Photo of MAX7219 for 0/60/120 km/h, SoC, E01–E03, ‘-’, ‘P’","matches Figure/TC-U15 strings","TC-U15, I06, I07, I13"],
["5","Fault injection on hardware","Unplug IMU; hold ECHO high; disconnect battery divider","E01/E02/E03 and recovery","TC-I06–I13"],
["6","UART frames","PC script verifying CRC-8 and 5 Hz","0 invalid, 5 Hz ± 5 %","TC-U17, S08"],
["7","Task execution time","DWT cycle counter per task, print at 1 Hz","mean exec well below period","Section 10"],
["8","Watchdog","Add a 300 ms delay in a test task","reset, boot cause = IWDG","TC-S07"],
["9","Flash / RAM","arm-none-eabi-size on the ELF","≤ 32 KB / ≤ 10 KB","Section 9.1"]]
st.append(tbl(rows,[6,40,50,42,40]))
doc.build(st); print("built")
