#!/bin/bash
source config.sh

echo "/kron.sg: $KRON" > $KRON.manifest
./dilos/scripts/gen-rofs-img.py -o $KRON.raw -m $KRON.manifest
