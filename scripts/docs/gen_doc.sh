#!/bin/bash
sudo apt install -y doxygen

# Output directories (kept outside the repo tree).
mkdir -p ../../../ream_tmp
mkdir -p ../../../ream_tmp/html

# Get ENV variables (REAM_VERSION, REAM_BUILD, ...).
cd ..
chmod +x env.sh
source env.sh
cd ../doxygen

doxygen Doxyfile

cd ..
