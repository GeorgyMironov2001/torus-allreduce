AC_DEFUN([SST_ELEMENT_CONFIG_OUTPUT], [
   SST_ariel_CONFIG([config_ariel=1],[config_ariel=0])
   AS_IF([test $config_ariel -eq 1], [active_element_libraries="$active_element_libraries ariel"])
   AC_CONFIG_FILES([src/sst/elements/ariel/Makefile])
   dist_element_libraries="$dist_element_libraries ariel"
   
   SST_balar_CONFIG([config_balar=1],[config_balar=0])
   AS_IF([test $config_balar -eq 1], [active_element_libraries="$active_element_libraries balar"])
   AC_CONFIG_FILES([src/sst/elements/balar/Makefile])
   dist_element_libraries="$dist_element_libraries balar"
   
   active_element_libraries="$active_element_libraries cacheTracer"
   AC_CONFIG_FILES([src/sst/elements/cacheTracer/Makefile])
   dist_element_libraries="$dist_element_libraries cacheTracer"
   
   active_element_libraries="$active_element_libraries cassini"
   AC_CONFIG_FILES([src/sst/elements/cassini/Makefile])
   dist_element_libraries="$dist_element_libraries cassini"
   
   active_element_libraries="$active_element_libraries cramSim"
   AC_CONFIG_FILES([src/sst/elements/cramSim/Makefile])
   dist_element_libraries="$dist_element_libraries cramSim"
   
   SST_ember_CONFIG([config_ember=1],[config_ember=0])
   AS_IF([test $config_ember -eq 1], [active_element_libraries="$active_element_libraries ember"])
   AC_CONFIG_FILES([src/sst/elements/ember/Makefile])
   dist_element_libraries="$dist_element_libraries ember"
   
   active_element_libraries="$active_element_libraries firefly"
   AC_CONFIG_FILES([src/sst/elements/firefly/Makefile])
   dist_element_libraries="$dist_element_libraries firefly"
   
   active_element_libraries="$active_element_libraries gensa"
   AC_CONFIG_FILES([src/sst/elements/gensa/Makefile])
   dist_element_libraries="$dist_element_libraries gensa"
   
   SST_golem_CONFIG([config_golem=1],[config_golem=0])
   AS_IF([test $config_golem -eq 1], [active_element_libraries="$active_element_libraries golem"])
   AC_CONFIG_FILES([src/sst/elements/golem/Makefile])
   dist_element_libraries="$dist_element_libraries golem"
   
   active_element_libraries="$active_element_libraries hermes"
   AC_CONFIG_FILES([src/sst/elements/hermes/Makefile])
   dist_element_libraries="$dist_element_libraries hermes"
   
   SST_iris_CONFIG([config_iris=1],[config_iris=0])
   AS_IF([test $config_iris -eq 1], [active_element_libraries="$active_element_libraries iris"])
   AC_CONFIG_FILES([src/sst/elements/iris/Makefile])
   dist_element_libraries="$dist_element_libraries iris"
   
   active_element_libraries="$active_element_libraries kingsley"
   AC_CONFIG_FILES([src/sst/elements/kingsley/Makefile])
   dist_element_libraries="$dist_element_libraries kingsley"
   
   active_element_libraries="$active_element_libraries mask-mpi"
   AC_CONFIG_FILES([src/sst/elements/mask-mpi/Makefile])
   dist_element_libraries="$dist_element_libraries mask-mpi"
   
   SST_memHierarchy_CONFIG([config_memHierarchy=1],[config_memHierarchy=0])
   AS_IF([test $config_memHierarchy -eq 1], [active_element_libraries="$active_element_libraries memHierarchy"])
   AC_CONFIG_FILES([src/sst/elements/memHierarchy/Makefile])
   dist_element_libraries="$dist_element_libraries memHierarchy"
   
   SST_mercury_CONFIG([config_mercury=1],[config_mercury=0])
   AS_IF([test $config_mercury -eq 1], [active_element_libraries="$active_element_libraries mercury"])
   AC_CONFIG_FILES([src/sst/elements/mercury/Makefile])
   dist_element_libraries="$dist_element_libraries mercury"
   
   active_element_libraries="$active_element_libraries merlin"
   AC_CONFIG_FILES([src/sst/elements/merlin/Makefile])
   dist_element_libraries="$dist_element_libraries merlin"
   
   SST_messier_CONFIG([config_messier=1],[config_messier=0])
   AS_IF([test $config_messier -eq 1], [active_element_libraries="$active_element_libraries messier"])
   AC_CONFIG_FILES([src/sst/elements/messier/Makefile])
   dist_element_libraries="$dist_element_libraries messier"
   
   SST_miranda_CONFIG([config_miranda=1],[config_miranda=0])
   AS_IF([test $config_miranda -eq 1], [active_element_libraries="$active_element_libraries miranda"])
   AC_CONFIG_FILES([src/sst/elements/miranda/Makefile])
   dist_element_libraries="$dist_element_libraries miranda"
   
   SST_mmu_CONFIG([config_mmu=1],[config_mmu=0])
   AS_IF([test $config_mmu -eq 1], [active_element_libraries="$active_element_libraries mmu"])
   AC_CONFIG_FILES([src/sst/elements/mmu/Makefile])
   dist_element_libraries="$dist_element_libraries mmu"
   
   active_element_libraries="$active_element_libraries osseous"
   AC_CONFIG_FILES([src/sst/elements/osseous/Makefile])
   dist_element_libraries="$dist_element_libraries osseous"
   
   SST_prospero_CONFIG([config_prospero=1],[config_prospero=0])
   AS_IF([test $config_prospero -eq 1], [active_element_libraries="$active_element_libraries prospero"])
   AC_CONFIG_FILES([src/sst/elements/prospero/Makefile])
   dist_element_libraries="$dist_element_libraries prospero"
   
   active_element_libraries="$active_element_libraries rdmaNic"
   AC_CONFIG_FILES([src/sst/elements/rdmaNic/Makefile])
   dist_element_libraries="$dist_element_libraries rdmaNic"
   
   SST_samba_CONFIG([config_samba=1],[config_samba=0])
   AS_IF([test $config_samba -eq 1], [active_element_libraries="$active_element_libraries samba"])
   AC_CONFIG_FILES([src/sst/elements/samba/Makefile])
   dist_element_libraries="$dist_element_libraries samba"
   
   active_element_libraries="$active_element_libraries shogun"
   AC_CONFIG_FILES([src/sst/elements/shogun/Makefile])
   dist_element_libraries="$dist_element_libraries shogun"
   
   active_element_libraries="$active_element_libraries simpleElementExample"
   AC_CONFIG_FILES([src/sst/elements/simpleElementExample/Makefile])
   dist_element_libraries="$dist_element_libraries simpleElementExample"
   
   active_element_libraries="$active_element_libraries simpleSimulation"
   AC_CONFIG_FILES([src/sst/elements/simpleSimulation/Makefile])
   dist_element_libraries="$dist_element_libraries simpleSimulation"
   
   active_element_libraries="$active_element_libraries thornhill"
   AC_CONFIG_FILES([src/sst/elements/thornhill/Makefile])
   dist_element_libraries="$dist_element_libraries thornhill"
   
   active_element_libraries="$active_element_libraries vanadis"
   AC_CONFIG_FILES([src/sst/elements/vanadis/Makefile])
   dist_element_libraries="$dist_element_libraries vanadis"
   
   active_element_libraries="$active_element_libraries vaultsim"
   AC_CONFIG_FILES([src/sst/elements/vaultsim/Makefile])
   dist_element_libraries="$dist_element_libraries vaultsim"
   
   SST_zodiac_CONFIG([config_zodiac=1],[config_zodiac=0])
   AS_IF([test $config_zodiac -eq 1], [active_element_libraries="$active_element_libraries zodiac"])
   AC_CONFIG_FILES([src/sst/elements/zodiac/Makefile])
   dist_element_libraries="$dist_element_libraries zodiac"
   
   SST_ACTIVE_ELEMENT_LIBRARIES="$active_element_libraries"
   SST_DIST_ELEMENT_LIBRARIES="$dist_element_libraries"
   AC_SUBST(SST_ACTIVE_ELEMENT_LIBRARIES)
   AC_SUBST(SST_DIST_ELEMENT_LIBRARIES)
])
