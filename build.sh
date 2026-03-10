#!/bin/sh

# Toolchain can either be GCCSDK or ARCHIESDK
export TOOLCHAIN=ARCHIESDK
# Set this to your hostfs path
# e.g., export TARGETCOPY=/mnt/c/dev/ACORN/Arculator_V2.1_Windows/hostfs
export TARGETCOPY=/home/nick/git/arculator/install/hostfs

# Build options
export USE_256_COLORS=yes
export TARGET_A5000=no

# Handle "asm" mode: compile C sources to assembly and exit
if [ "$1" = "asm" ]; then
    # C source files (excluding poly.s which is already assembly)
    C_SOURCES="main.c math3d.c palette.c mesh.c render.c"

    ASMDIR=../
    mkdir -p "$ASMDIR"

    # Determine compiler and flags based on toolchain
    if [ "$TOOLCHAIN" = "GCCSDK" ]; then
        CC=$(ls ${GCCSDK_INSTALL_CROSSBIN}/*gcc 2>/dev/null | head -1)
        TFLAGS="-O2 -mlibscl"
        TINCLUDES="-I${GCCSDK_INSTALL_ENV}/include"
    elif [ "$TOOLCHAIN" = "ARCHIESDK" ]; then
        . ${ARCHIESDK}/config.mk 2>/dev/null || true
        CC="${ARCHIECC}"
        TFLAGS="-O2"
        TINCLUDES=""
    else
        echo "Unknown TOOLCHAIN: $TOOLCHAIN"
        exit 1
    fi

    # Apply feature flags
    if [ "$TARGET_A5000" = "yes" ]; then
        TFLAGS="$TFLAGS -DA5000"
    fi
    if [ "$USE_256_COLORS" = "yes" ]; then
        TFLAGS="$TFLAGS -DPAL_256"
    fi

    echo "Generating assembly files into asm/ using $TOOLCHAIN toolchain"
    cd src
    for src in $C_SOURCES; do
        outfile="$ASMDIR/$(echo "$src" | sed 's/\.c$/.s/')"
        echo "  $src -> $outfile"
        $CC -S $TFLAGS $TINCLUDES "$src" -o "$outfile"
    done
    echo "Done. Assembly files written to asm/"
    exit 0
fi

cd src
if [ "$TOOLCHAIN" = "GCCSDK" ]; then
    echo "Using GCCSDK toolchain"
    make clean
    make -j4 USE_256_COLORS=$USE_256_COLORS TARGET_A5000=$TARGET_A5000
    
    # Uncomment RM lines in !Run,feb for GCCSDK builds (in case they were commented by ARCHIESDK)
    sed -i.bak 's/^| RMLoad /RMLoad /' ./!SpaceGame/!Run,feb
    sed -i.bak 's/^| RMEnsure /RMEnsure /' ./!SpaceGame/!Run,feb
    rm -f ./!SpaceGame/!Run,feb.bak
elif [ "$TOOLCHAIN" = "ARCHIESDK" ]; then
    echo "Using ARCHIESDK toolchain"
    make -f Makefile clean
    make -f Makefile -j4 USE_256_COLORS=$USE_256_COLORS TARGET_A5000=$TARGET_A5000
    
    # Comment out RM lines in !Run,feb for ARCHIESDK builds
    sed -i.bak 's/^RMLoad /| RMLoad /' ./!SpaceGame/!Run,feb
    sed -i.bak 's/^RMEnsure /| RMEnsure /' ./!SpaceGame/!Run,feb
    rm -f ./!SpaceGame/!Run,feb.bak
else
    echo "Unknown TOOLCHAIN: $TOOLCHAIN"
    exit 1
fi

# Copy the !SpaceGame folder to hostfs, creating it if necessary
mkdir -p ${TARGETCOPY}/!SpaceGame
cp -rf ./!SpaceGame/* ${TARGETCOPY}/!SpaceGame/
