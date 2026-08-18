$ErrorActionPreference = "Stop"

# Needed for Invoke-WebRequest to work via CI.
$progressPreference = "silentlyContinue"

git clone https://github.com/theQRL/qrvmone.git
cd qrvmone
git checkout 46b74c67bba54266d12e7c341bb7f8413e9bce56
git submodule update --init --recursive
cmake -S . -B build -DBUILD_SHARED_LIBS=ON
cmake --build build --config Release
cd ..
mkdir deps
mv qrvmone/build/bin/Release/qrvmone.dll deps
