This folder contains the scripts used to simulate jobs allocation and to generate the corresponding plots.
It contains the following files:

- plot_paper.py: Generates paper plots starting from the data in logs.tar.gz
- logs.tar.gz: Archive of allocation simulations.
- simulate_allocation.py: Simulates jobs allocation. Run with --help to get a list of command line parameters.
- run_all.sh: Run this to regenerate the data in logs.tar.gz. It calls simulate_allocation.py multiple times with different parameters. ATTENTION: This might take several hours/days to complete due to the very large number of parameters combinations evaluated.
- alibaba_cdf.csv: CDF of the Alibaba jobs dataset.
- plot_old.py: Old plotting script.
