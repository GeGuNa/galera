#!/bin/bash -x

# $Id$

# Fail if any command fails
set -e

have_ccache="false"
#if test -n "`which ccache`"
#then
#    have_ccache="true"
#    if test -n "`which distcc`"
#    then
#	export CCACHE_PREFIX="distcc"
#	export MAKE="make -j6"
#	export DISTCC_HOSTS="192.168.2.1 192.168.2.2 192.168.2.3"
#    fi
#    export CC="ccache gcc"
#    export CXX="ccache g++"
#
#fi

initial_stage="galerautils"
last_stage="galera"
gainroot=""

usage()
{
    echo -e "Usage: build.sh [OPTIONS] \n" \
	"Options:                      \n" \
        "    --stage <initial stage>   \n" \
	"    --last-stage <last stage> \n" \
	"    -s|--scratch    build everything from scratch\n"\
	"    -c|--configure  reconfigure the build system (implies -s)\n"\
	"    -b|--bootstap   rebuild the build system (implies -c)\n"\
	"    -o|--opt        configure build with debug disabled (implies -c)\n" \
	"    -d|--debug      configure build with debug enabled (implies -c)\n" \
        "    --with-spread   configure build with spread backend (implies -c to gcs)\n"
}

while test $# -gt 0 
do
    case $1 in 
	--stage)
	    initial_stage=$2
	    shift
	    ;;
	--last-stage)
	    last_stage=$2
	    shift
	    ;;
	--gainroot)
	    gainroot=$2
	    shift
	    ;;
	-b|--bootstrap)
	    BOOTSTRAP=yes # Bootstrap the build system
	    ;;
	-c|--configure)
	    CONFIGURE=yes # Reconfigure the build system
	    ;;
	-s|--scratch)
	    SCRATCH=yes   # Build from scratch (run make clean)
	    ;;
	-o|--opt)
	    OPT=yes       # Compile without debug
	    ;;
	-d|--debug)
	    DEBUG=yes     # Compile with debug
	    ;;
	--with*-spread)
	    WITH_SPREAD="$1"
	    ;;
	--help)
	    usage
	    exit 0
	    ;;
	*)
	    if test ! -z "$1"; then
		echo "Unrecognized option: $1"
	    fi
	    usage
	    exit 1
	    ;;
    esac
    shift
done

if [ "$OPT"   == "yes" ]; then CONFIGURE="yes"; fi
if [ "$DEBUG" == "yes" ]; then CONFIGURE="yes"; fi
if [ -n "$WITH_SPREAD" ]; then CONFIGURE="yes"; fi

# Be quite verbose
set -x

# Build process base directory
build_base=$(cd $(dirname $0); cd ..; pwd -P)

# Define branches to be used
galerautils_src=$build_base/galerautils
galeracomm_src=$build_base/galeracomm
gcs_src=$build_base/gcs
wsdb_src=$build_base/wsdb
galera_src=$build_base/galera
#mysql_src=$build_base/../../5.1/trunk

# Flags for configure scripts
if test -n GALERA_DEST
then
    conf_flags="--prefix=$GALERA_DEST"
    galera_flags="--with-galera=$GALERA_DEST"
fi

if [ "$OPT" == "yes" ]
then
    conf_flags="$conf_flags --disable-debug"
    CONFIGURE="yes"
fi
if [ "$DEBUG" == "yes" ]
then
    CONFIGURE="yes"
fi


# Function to build single project
build()
{
    local build_dir=$1
    shift
    echo "Building: $build_dir ($@)"
    pushd $build_dir
    export LD_LIBRARY_PATH
    export CPPFLAGS
    export LDFLAGS
    if [ "$BOOTSTRAP" == "yes" ]; then ./bootstrap.sh; CONFIGURE=yes ; fi
    if [ "$CONFIGURE" == "yes" ]; then rm -rf config.status; ./configure $@; SCRATCH=yes ; fi
    if [ "$SCRATCH"   == "yes" ]; then make clean ; fi
    make
#    $gainroot make install
    popd
}

# Updates build flags for the next stage
build_flags()
{
    local build_dir=$1
    LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$build_dir/src/.libs
    CPPFLAGS="$CPPFLAGS -I$build_dir/src "
    LDFLAGS="$LDFLAGS -L$build_dir/src/.libs"
}

building="false"
# Build projects

if test $initial_stage = "scratch"
then
# Commented out, not sure where this does its tricks (teemu)
#    rm -rf
    if test $have_ccache = "true"
    then
	ccache -C
    fi
    building="true"
fi


echo "CPPFLAGS: $CPPFLAGS"

if test $initial_stage = "galerautils" || $building = "true"
then
    build $galerautils_src $conf_flags
    building="true"
fi

build_flags $galerautils_src

if test $initial_stage = "galeracomm" || $building = "true"
then
    build $galeracomm_src $conf_flags $galera_flags
    building="true"
fi

# Galera comm is not particularly easy to handle
CPPFLAGS="$CPPFLAGS -I$galeracomm_src/vs/include" # non-standard location
CPPFLAGS="$CPPFLAGS -I$galeracomm_src/common/include" # non-standard location
LDFLAGS="$LDFLAGS -L$galeracomm_src/common/src/.libs"
LDFLAGS="$LDFLAGS -L$galeracomm_src/transport/src/.libs"
LDFLAGS="$LDFLAGS -L$galeracomm_src/vs/src/.libs"
LD_LIBRARY_PATH="$LD_LIBRARY_PATH:$galeracomm_src/common/src/.libs"
LD_LIBRARY_PATH="$LD_LIBRARY_PATH:$galeracomm_src/transport/src/.libs"
LD_LIBRARY_PATH="$LD_LIBRARY_PATH:$galeracomm_src/vs/src/.libs"

if test $initial_stage = "gcs" || $building = "true"
then
    build $gcs_src $conf_flags $WITH_SPREAD $galera_flags
    building="true"
fi

build_flags $gcs_src

if test $initial_stage = "wsdb" || $building = "true"
then
    build $wsdb_src $conf_flags $galera_flags
    building="true"
fi

build_flags $wsdb_src

if test $initial_stage = "galera" || $building = "true"
then
    build $galera_src $conf_flags $galera_flags
    building="true"
fi

build_flags $galera_src


if test $building != "true"
then
    echo "Warn: Nothing was built!"
fi
