/* src/sst/core/sst_config.h.  Generated from sst_config.h.in by configure.  */
/* src/sst/core/sst_config.h.in.  Generated from configure.ac by autoheader.  */


#ifndef _SST_CONFIG_H_
#define _SST_CONFIG_H_


/* Defines if the backtrace function is avalible */
#define HAVE_BACKTRACE 1

/* Defines whether we have the curses library */
#define HAVE_CURSES 1

/* define if the compiler supports basic C++17 syntax */
#define HAVE_CXX17 1

/* Define to 1 if you have the <c_asm.h> header file. */
/* #undef HAVE_C_ASM_H */

/* Define to 1 if you have the <dlfcn.h> header file. */
#define HAVE_DLFCN_H 1

/* Define to 1 if you have the <execinfo.h> header file. */
#define HAVE_EXECINFO_H 1

/* Defines whether we have the hdf5 library */
/* #undef HAVE_HDF5 */

/* Define to 1 if you have the <intrinsics.h> header file. */
/* #undef HAVE_INTRINSICS_H */

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Defines whether we have the libz library */
#define HAVE_LIBZ 1

/* Define to 1 if you have the <mach/mach_time.h> header file. */
/* #undef HAVE_MACH_MACH_TIME_H */

/* Define to 1 if you have the <Python.h> header file. */
#define HAVE_PYTHON_H 1

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdio.h> header file. */
#define HAVE_STDIO_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/time.h> header file. */
#define HAVE_SYS_TIME_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Define to the sub-directory where libtool stores uninstalled libraries. */
#define LT_OBJDIR ".libs/"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT "wg-sst@sandia.gov"

/* Define to the full name of this package. */
#define PACKAGE_NAME "SSTCore"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "SSTCore 15.0.0"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "sstcore"

/* Define to the home page for this package. */
#define PACKAGE_URL ""

/* Define to the version of this package. */
#define PACKAGE_VERSION "15.0.0"

/* SST-Core Git Branch */
#define SSTCORE_GIT_BRANCH "N/A"

/* SST-Core Git Commit Count */
#define SSTCORE_GIT_COMMITCOUNT "0"

/* SST-Core Git Head SHA */
#define SSTCORE_GIT_HEADSHA "15.0.0"

/* Defines the C compiler used to build SST */
#define SST_CC "gcc"

/* Defines the CFLAGS used to build SST */
#define SST_CFLAGS "-g3 -O0 -fno-omit-frame-pointer -Wall -Wextra -Wvla -Wnon-virtual-dtor -Wsuggest-override"

/* EXPERIMENTAL Tracks clock handler execution time and counters */
/* #undef SST_CLOCK_PROFILING */

/* Defines compile for Mac OS-X */
/* #undef SST_COMPILE_MACOSX */

/* Define if you have the MPI library. */
#define SST_CONFIG_HAVE_MPI 1

/* Set to 1 if Python was found */
#define SST_CONFIG_HAVE_PYTHON 1

/* Set to 1 if Python version is 3 */
#define SST_CONFIG_HAVE_PYTHON3 1

/* Defines the C preprocessor used to build SST */
#define SST_CPP "gcc -E"

/* Defines the CPPFLAGS used to build SST */
#define SST_CPPFLAGS "-I$(top_srcdir)/src -I$(top_builddir)/src "

/* Defines the C++ compiler used to build SST */
#define SST_CXX "g++ -std=c++17"

/* Defines the C++ preprocessor used to build SST */
#define SST_CXXCPP "g++ -E -std=c++17"

/* Defines the CXXFLAGS used to build SST */
#define SST_CXXFLAGS "-g3 -O0 -fno-omit-frame-pointer -std=c++17 -Wall -Wextra -Wvla -Wnon-virtual-dtor -Wsuggest-override"

/* Whether to enable preview build */
/* #undef SST_ENABLE_PREVIEW_BUILD */

/* EXPERIMENTAL Tracks event and communication time and counters */
/* #undef SST_EVENT_PROFILING */

/* EXPERIMENTAL Enables nanosecond resolution clock. Disable for gettimeofday
   microsecond resolution */
/* #undef SST_HIGH_RESOLUTION_CLOCK */

/* Defines the location SST will be installed in */
#define SST_INSTALL_PREFIX "/home/gera/torus-allreduce/local/sstcore-15.0.0"

/* Defines the linker used to build SST */
#define SST_LD "/usr/bin/ld -m elf_x86_64"

/* Defines the LDFLAGS used to build SST */
#define SST_LDFLAGS ""

/* Defines the MPI C compiler used to build SST */
#define SST_MPICC "mpicc"

/* Defines the MPI C++ compilers used to build SST */
#define SST_MPICXX "mpicxx"

/* EXPERIMENTAL Required for all performance tracking. Enables file creation
   and final output */
/* #undef SST_PERFORMANCE_INSTRUMENTING */

/* EXPERIMENTAL Periodically prints performance information to files */
/* #undef SST_PERIODIC_PRINT */

/* EXPERIMENTAL Tune to affect how often files are written. */
/* #undef SST_PERIODIC_PRINT_THRESHOLD */

/* Defines the CPPFLAGS needed to compile Python into SST */
#define SST_PYTHON_CPPFLAGS "-I/usr/include/python3.10 -I/usr/include/python3.10"

/* Defines the LDFLAGS needed to compile Python into SST */
#define SST_PYTHON_LDFLAGS "-L/usr/lib/python3.10/config-3.10-x86_64-linux-gnu -L/usr/lib/x86_64-linux-gnu -lpython3.10 -lcrypt -ldl  -lm -lm "

/* EXPERIMENTAL Tracks execution time for each rank. */
/* #undef SST_RUNTIME_PROFILING */

/* EXPERIMENTAL Tracks synchronization time and count */
/* #undef SST_SYNC_PROFILING */

/* Test Frameworks will be sym-linked instead of copied on install */
/* #undef SST_TESTFRAMEWORK_DEV */

/* Define to 1 if all of the C89 standard headers exist (not just the ones
   required in a freestanding environment). This macro is provided for
   backward compatibility; new code need not use it. */
#define STDC_HEADERS 1

/* Set to 1 to use memory pools in the SST core */
#define USE_MEMPOOL 1

/* Tracks extra information about events and activities. */
/* #undef __SST_DEBUG_EVENT_TRACKING__ */

/* Defines additional debug output is to be printed */
/* #undef __SST_DEBUG_OUTPUT__ */

/* Defines if core should have profiling enabled. */
/* #undef __SST_ENABLE_PROFILE__ */

/* Defines that standard PRI macros should be enabled */
#define __STDC_FORMAT_MACROS 1

/* Define to '__inline__' or '__inline' if that's what the C compiler
   calls it, or to nothing if 'inline' is not supported under any name.  */
#ifndef __cplusplus
/* #undef inline */
#endif


#endif /* _SST_CONFIG_H_ */

