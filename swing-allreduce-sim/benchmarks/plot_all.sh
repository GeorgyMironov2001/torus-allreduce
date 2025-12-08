#!/bin/bash
mkdir -p plots_paper



# Starting fig.
pushd allreduce
python3 ThreeInOnePlot.py --topo Torus --shape 64x64   --y_zoomed_in 300000
popd
FNAME=$(find allreduce/plots/torus_64x64/pdf -type f -exec ls -t1 {} + | head -1)
mv $FNAME plots_paper/torus_64x64.pdf



# Scaling
pushd recap_plots/scaling
python3 scaling_bw.py
python3 scaling.py
popd
FNAME=$(find recap_plots/scaling/plots/pdf -type f -exec ls -t1 {} + | head -1)
mv $FNAME plots_paper/scaling.pdf
FNAME=$(find recap_plots/scaling/plots_bw/pdf -type f -exec ls -t1 {} + | head -1)
mv $FNAME plots_paper/scaling_bw.pdf



# Global gains
pushd recap_plots/overall
python3 overall.py
popd
FNAME=$(find recap_plots/overall/plots/pdf -type f -exec ls -t1 {} + | head -1)
mv $FNAME plots_paper/global_gains.pdf



# Rectangular
pushd allreduce
python3 ThreeInOnePlot.py --topo Torus --shape 64x16 --y_zoomed_in 310000
python3 ThreeInOnePlot.py --topo Torus --shape 128x8 --y_zoomed_in 600000
python3 ThreeInOnePlot.py --topo Torus --shape 256x4 --y_zoomed_in 1200000
popd
FNAME=$(find allreduce/plots/torus_64x16/pdf -type f -exec ls -t1 {} + | head -1)
mv $FNAME plots_paper/torus_64x16.pdf
FNAME=$(find allreduce/plots/torus_128x8/pdf -type f -exec ls -t1 {} + | head -1)
mv $FNAME plots_paper/torus_128x8.pdf
FNAME=$(find allreduce/plots/torus_256x4/pdf -type f -exec ls -t1 {} + | head -1)
mv $FNAME plots_paper/torus_256x4.pdf



# Higher dimensional    
pushd allreduce
python3 ThreeInOnePlot.py --topo Torus --shape 8x8     --y_zoomed_in 40000
python3 ThreeInOnePlot.py --topo Torus --shape 8x8x8   --y_zoomed_in 45000
python3 ThreeInOnePlot.py --topo Torus --shape 8x8x8x8 --y_zoomed_in 65000
popd
FNAME=$(find allreduce/plots/torus_8x8/pdf -type f -exec ls -t1 {} + | head -1)
mv $FNAME plots_paper/torus_8x8.pdf
FNAME=$(find allreduce/plots/torus_8x8x8/pdf -type f -exec ls -t1 {} + | head -1)
mv $FNAME plots_paper/torus_8x8x8.pdf
FNAME=$(find allreduce/plots/torus_8x8x8x8/pdf -type f -exec ls -t1 {} + | head -1)
mv $FNAME plots_paper/torus_8x8x8x8.pdf



# Other topologies
pushd allreduce
python3 ThreeInOnePlot.py --topo Hx2    --shape 64x64 --y_zoomed_in 175000
python3 ThreeInOnePlot.py --topo Hx4    --shape 64x64 --y_zoomed_in 175000
python3 ThreeInOnePlot.py --topo Hyperx --shape 64x64 --y_zoomed_in 280000
popd
FNAME=$(find allreduce/plots/hx2_32x32/pdf -type f -exec ls -t1 {} + | head -1)
mv $FNAME plots_paper/hx2_32x32.pdf
FNAME=$(find allreduce/plots/hx4_16x16/pdf -type f -exec ls -t1 {} + | head -1)
mv $FNAME plots_paper/hx4_16x16.pdf
FNAME=$(find allreduce/plots/hyperx_64x64/pdf -type f -exec ls -t1 {} + | head -1)
mv $FNAME plots_paper/hyperx_64x64.pdf

'''
pushd allreduce
# Fat Trees
python3 ThreeInOnePlot.py --topo fattree   --shape 32,32:32 --y_zoomed_in 10000
python3 ThreeInOnePlot.py --topo fattree21 --shape 32,16:32 --y_zoomed_in 10000
python3 ThreeInOnePlot.py --topo fattree41 --shape 32,8:32  --y_zoomed_in 10000
python3 ThreeInOnePlot.py --topo fattree81 --shape 32,4:32  --y_zoomed_in 10000

# Square torus
python3 ThreeInOnePlot.py --topo Torus --shape 8x8     --y_zoomed_in 30000
python3 ThreeInOnePlot.py --topo Torus --shape 16x16   --y_zoomed_in 10000
python3 ThreeInOnePlot.py --topo Torus --shape 32x32   --y_zoomed_in 10000
python3 ThreeInOnePlot.py --topo Torus --shape 64x64   --y_zoomed_in 300000
python3 ThreeInOnePlot.py --topo Torus --shape 128x128 --y_zoomed_in 10000

# Rectangular torus
python3 ThreeInOnePlot.py --topo Torus --shape 64x16 --y_zoomed_in 10000
python3 ThreeInOnePlot.py --topo Torus --shape 128x8 --y_zoomed_in 10000
python3 ThreeInOnePlot.py --topo Torus --shape 256x4 --y_zoomed_in 10000

# Higher dimensional torus
python3 ThreeInOnePlot.py --topo Torus --shape 8x8x8   --y_zoomed_in 10000 
python3 ThreeInOnePlot.py --topo Torus --shape 8x8x8x8 --y_zoomed_in 10000

# Other topologies
python3 ThreeInOnePlot.py --topo Hx2    --shape 64x64 --y_zoomed_in 175000
python3 ThreeInOnePlot.py --topo Hx4    --shape 64x64 --y_zoomed_in 175000
python3 ThreeInOnePlot.py --topo Hyperx --shape 64x64 --y_zoomed_in 10000

# Varying bw
python3 ThreeInOnePlot.py --topo Torus --shape 8x8 --netBW 100Gb/s --y_zoomed_in 10000
python3 ThreeInOnePlot.py --topo Torus --shape 8x8 --netBW 200Gb/s --y_zoomed_in 10000
python3 ThreeInOnePlot.py --topo Torus --shape 8x8 --netBW 800Gb/s --y_zoomed_in 10000
python3 ThreeInOnePlot.py --topo Torus --shape 8x8 --netBW 1600Gb/s --y_zoomed_in 10000
python3 ThreeInOnePlot.py --topo Torus --shape 8x8 --netBW 3200Gb/s --y_zoomed_in 10000

popd
'''