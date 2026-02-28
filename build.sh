#!/bin/sh

# Toolchain can either be GCCSDK or ARCHIESDK
export TOOLCHAIN=GCCSDK
# Set this to your hostfs path
# e.g., export TARGETCOPY=/mnt/c/dev/ACORN/Arculator_V2.1_Windows/hostfs
export TARGETCOPY=/home/nick/git/arculator/install/hostfs

# Build options
export USE_256_COLORS=yes
export TARGET_A5000=no

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
