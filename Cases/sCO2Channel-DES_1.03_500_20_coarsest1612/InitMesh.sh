#!/bin/bash
source /storage/home/mmn5264/OpenFOAM/SetupOpenFOAM4-Personal.sh
m4 constant/polyMesh/blockMeshDict.m4 > constant/polyMesh/blockMeshDict
blockMesh
mirrorMesh -noFunctionObjects -overwrite
checkMesh
decomposePar -force
