# cd /home/gera/scratch/src/openmpi-4.1.6
export MPIHOME=$HOME/torus-allreduce/local/packages/OpenMPI-4.1.6
# ./configure --prefix=$MPIHOME
# make all install
export PATH=$MPIHOME/bin:$PATH
export MPICC=mpicc
export MPICXX=mpicxx
export LD_LIBRARY_PATH=$MPIHOME/lib:$LD_LIBRARY_PATH
export DYLD_LIBRARY_PATH=$MPIHOME/lib:$DYLD_LIBRARY_PATH
export MANPATH=$MPIHOME/share/man:$DYLD_LIBRARY_PATH

cd /home/gera/torus-allreduce/scratch/src/sst-core
make clean
export SST_CORE_HOME=$HOME/torus-allreduce/local/sstcore-15.0.0
export SST_CORE_ROOT=$HOME/torus-allreduce/scratch/src/sst-core
export CFLAGS="-g3 -O0 -fno-omit-frame-pointer"
export CXXFLAGS="-g3 -O0 -fno-omit-frame-pointer"
./configure --prefix=$SST_CORE_HOME
make -j"$(nproc)" --output-sync=target all
make install
export PATH=$SST_CORE_HOME/bin:$PATH

cd /home/gera/torus-allreduce/scratch/src/sst-elements
make clean
export SST_ELEMENTS_HOME=$HOME/torus-allreduce/local/sstelements-15.0.0
export SST_ELEMENTS_ROOT=$HOME/torus-allreduce/scratch/src/sst-elements
export CFLAGS="-g3 -O0 -fno-omit-frame-pointer"
export CXXFLAGS="-g3 -O0 -fno-omit-frame-pointer"
./configure --prefix=$SST_ELEMENTS_HOME --with-sst-core=$SST_CORE_HOME
make -j"$(nproc)" --output-sync=target all
make install    
export PATH=$SST_ELEMENTS_HOME/bin:$PATH

cd $HOME/torus-allreduce
