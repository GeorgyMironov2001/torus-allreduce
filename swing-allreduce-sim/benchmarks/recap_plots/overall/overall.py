import pandas as pd
import numpy as np
import seaborn as sns
import matplotlib.pyplot as plt
from pathlib import Path    
from datetime import datetime
from matplotlib.ticker import FuncFormatter
import numpy as np
import statistics
import matplotlib.patches as mpatches

list_my_colors = ["#44739d", "#d48640", "#539045", "#b14743", "#8e73ae", "#7e5d55", "#cb88ba", "#7f7f7f"]

#index_x = ["Torus\n64x64", "Torus\n64x16", "Torus\n128x8", "Torus\n256x4", "Torus\n8x8", "Torus\n8x8x8", "Torus\n8x8x8x8", "Hx2Mesh\n4,096 nodes", "Hx4Mesh\n4,096 nodes", "HyperX\n4,096 nodes", "Fattree\n2:1 blocking", "Fattree\n4:1 blocking", "Fattree\n8:1 blocking"]
index_x = ["Torus\n16x16", "Torus\n32x32", "Torus\n64x64", "Torus\n128x128", "Torus\n64x16", "Torus\n128x8", "Torus\n256x4", "Torus 8x8\n(100Gbit/s)", "Torus 8x8\n(200Gbit/s)", "Torus 8x8\n(800Gbit/s)", "Torus 8x8\n(1.6Tbit/s)", "Torus 8x8\n(3.2Tbit/s)", "Torus\n8x8", "Torus\n8x8x8", "Torus\n8x8x8x8", "Hx2Mesh\n4k nodes", "Hx4Mesh\n4k nodes", "HyperX\n4k nodes"]
index_x.reverse()

diff_files = {}
diff_files["Torus\n16x16"] = "../../allreduce/output/torus_16x16/diff"
diff_files["Torus\n32x32"] = "../../allreduce/output/torus_32x32/diff"
diff_files["Torus\n64x64"] = "../../allreduce/output/torus_64x64/diff"
diff_files["Torus\n128x128"] = "../../allreduce/output/torus_128x128/diff"

diff_files["Torus\n64x16"] = "../../allreduce/output/torus_64x16/diff"
diff_files["Torus\n128x8"] = "../../allreduce/output/torus_128x8/diff"
diff_files["Torus\n256x4"] = "../../allreduce/output/torus_256x4/diff"

diff_files["Torus 8x8\n(100Gbit/s)"] = "../../allreduce/output/torus_8x8_100Gb/diff"
diff_files["Torus 8x8\n(200Gbit/s)"] = "../../allreduce/output/torus_8x8_200Gb/diff"
diff_files["Torus 8x8\n(800Gbit/s)"] = "../../allreduce/output/torus_8x8_800Gb/diff"
diff_files["Torus 8x8\n(1.6Tbit/s)"] = "../../allreduce/output/torus_8x8_1600Gb/diff"
diff_files["Torus 8x8\n(3.2Tbit/s)"] = "../../allreduce/output/torus_8x8_3200Gb/diff"

diff_files["Torus\n8x8"] = "../../allreduce/output/torus_8x8/diff"
diff_files["Torus\n8x8x8"] = "../../allreduce/output/torus_8x8x8/diff"
diff_files["Torus\n8x8x8x8"] = "../../allreduce/output/torus_8x8x8x8/diff"

diff_files["Hx2Mesh\n4k nodes"] = "../../allreduce/output/hx2_32x32/diff"
diff_files["Hx4Mesh\n4k nodes"] = "../../allreduce/output/hx4_16x16/diff"
diff_files["HyperX\n4k nodes"] = "../../allreduce/output/hyperx_64x64/diff"


#torus64x64 = [60.175429877745756, 60.175429877745756, 60.175429877745756, 60.11177347242922, 59.58853238265003, 50.24823438920355, 45.97402597402596, 165.6746854693765, 142.35087583918516, 80.43108762913329, 22.386260266281333, -8.297106036054288]
#torus64x16 = [42.29142051301597, 42.29142051301597, 42.29142051301597, 42.080225300654575, 41.20428155376528, 36.55592276917723, 86.1889541366238, 292.6449040787815, 231.79969362421423, 116.8392284286324, 31.72919048795472, -1.0868405565669905]
#torus128x8 = [44.62647129473938, 44.62647129473938, 44.62647129473938, 44.30755956090312, 43.56430921836999, 40.064780242026174, 122.39948795751945, 419.05405858488774, 335.13807084734015, 209.66428559460715, 67.04236300038866, 6.962223814618581]
#torus256x4 = [48.54707737735723, 48.54707737735723, 48.54707737735723, 47.525359565988204, 46.99632094141398, 44.670297981416226, 143.13553580679869, 497.67198887272207, 433.1982347628249, 287.8216175473549, 88.99769949695849, -4.113530050136059]
#torus8x8 = [22.74954500909983, 22.74954500909983, 22.74954500909983, 22.686608890131396, 23.44464458916447, 19.157223796034, 25.14777785886302, 47.89895555015789, 26.60727185167349, 7.453012991227825, -0.6909050025698125, -3.093092584676102]
#torus8x8x8 = [24.715325742019782, 24.715325742019782, 24.715325742019782, 24.738610903659445, 23.986267050199498, 24.23941500406248, 18.394841016472974, 61.20951516825121, 36.621884104888316, 18.404415454089314, 8.296379522608568, 4.4950506854944265]
#torus8x8x8x8 = [64.69108191221694, 64.69108191221694, 64.69108191221694, 64.69108191221694, 64.17473698878284, 60.74766355140188, 14.834307992202728, 68.37566635308873, 46.399194225281164, 27.02457143620646, 13.332805471290074, 7.609811976615578]
#hx2 = [88.24882143267793, 88.24882143267793, 88.24882143267793, 86.3207010356208, 80.12982726372539, 33.36591723688498, 25.562111306011804, 182.2609208972845, 272.93765046921834, 100.11142268194395, 27.533551009528896, 6.988761633803012]
#h4 = [105.31966341038785, 105.31966341038785, 105.31966341038785, 105.18074618209936, 104.33150447588795, 35.54689994253242, 53.84160218479745, 292.91523653186096, 181.59635323560957, 62.382074461248436, 11.287280370195038, -3.6682772308987746]
#hyperx = [53.25406190432965, 53.25406190432965, 53.25406190432965, 52.865550022634665, 51.060212937281904, 23.34891876095849, 4.904980806596834, 76.209620538863, 256.27448361823366, 188.4573934029089, 55.81685945833621, 15.46908952172684]
#ft_nb = [28.501204819277092, 28.501204819277092, 28.37746750120365, 27.995391705069117, 24.52877190068919, 17.551975617720693, 17.408817928812066, 12.140879196953962, 11.575008191491179, 7.980031853418682, 2.10264511437115]
#ft_2 = [28.501204819277092, 28.501204819277092, 28.37746750120365, 27.995391705069117, 24.52877190068919, 21.268900386798606, 17.408817928812066, 15.647834610586337, 14.48450840823679, 9.33668060507583, -1.00210644534662]
#ft_4 = [28.625180897250353, 28.625180897250353, 28.37746750120365, 27.995391705069117, 16.71449623826882, 13.38787823384223, 17.851512373968834, 28.382816460578802, 15.645175477174774, 15.013640007913281, 28.543249133467835]
#ft_8 = [28.519054510371454, 28.519054510371454, 28.271545498314886, 27.889784946236567, 21.9202363367799, 26.40095072675749, 17.97921393544611, 27.70250664047677, 26.183919461875306, 22.708606604762792, 26.906755088684363]

#index_y =  [torus64x64,
#    torus64x16,
#    torus128x8,
#    torus256x4,
#    torus8x8,
#    torus8x8x8,
#    torus8x8x8x8,
#    hx2,
#    h4,
#    hyperx]
#index_y = index_y[::-1]
#print(len(index_y))
#print(len(index_x))

index_y = []

for b in index_x:
    performance_diff = []
    with open(diff_files[b]) as f:
        performance_diff = f.read().splitlines()
    performance_diff = [int(float(x)) for x in performance_diff][0:13] # Crop to 512MiB
    index_y += [performance_diff]

print(index_y)
f, ax = plt.subplots()

df = pd.DataFrame(index_y, index=index_x)
df = df.T
#df.fillna(0, inplace=True)
print(df)
#bxx = plt.boxplot(df, vert=False, showmeans=False, patch_artist=True, showfliers=False, whis=[0, 100])
bxx = plt.boxplot(df, vert=False, showmeans=False, patch_artist=True, showfliers=True)

idxa = 0
for patch in bxx['boxes']:
    if idxa < 1:
        patch.set_facecolor(list_my_colors[0])
    elif idxa < 3:
        patch.set_facecolor(list_my_colors[4])
    else:
        patch.set_facecolor(list_my_colors[2])
    idxa += 1

for median in bxx['medians']:
    median.set(color=list_my_colors[1], linewidth=0)

    
list_mean_geo = []
idx = 1
for elems in index_y:
    #list_mean_geo.append(statistics.geometric_mean(elems))
    n = len(elems)
    multiply = 1

    for i in elems:
        multiply = (multiply)*(i)

    geometric_mean = (multiply)**(1/n)
    list_mean_geo.append(geometric_mean)
    plt.scatter(statistics.median(elems), idx, marker="^", color=list_my_colors[1], zorder=1000)
    #plt.scatter(geometric_mean, idx, color="black", zorder=1000)
    idx += 1

print(list_mean_geo)

#plt.set_xlim([-100, 500])
plt.axvspan(xmin=-509, xmax=0, facecolor="red", alpha=0.15)
plt.axvspan(xmin=0, xmax=590, facecolor='green', alpha=0.15)
plt.axvline(x=0, ls='--', c='black', alpha=0.55)
plt.title("Goodput Gain vs. Best Known Algo. for Allreduce <= 512MiB", fontsize=11.6)
plt.grid(alpha=0.47, linewidth = 0.35)
plt.xlim(-80,250)
ax.set_yticklabels(index_x)

ax.xaxis.set_major_formatter(FuncFormatter('{:.0f}%'.format))



plt.show()
plt.gcf().set_size_inches(6, 8)
plt.tight_layout()
Path("plots/png").mkdir(parents=True, exist_ok=True)
Path("plots/pdf").mkdir(parents=True, exist_ok=True)
file_name = datetime.now().strftime('%Y-%m-%d|%H:%M:%S')
plt.savefig(Path("plots/png") / (str("speedup") + file_name))
plt.savefig(Path("plots/pdf") / (str("speedup") + file_name + ".pdf"))
# Finally Show the plot
plt.show()
