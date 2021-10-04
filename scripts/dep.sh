#!/bin/bash
set -euo pipefail

INSTALL_DIR=$1

echo "Installing CITRUS dependencies..."
sudo apt-get install make build-essential lcov -y
sudo apt-get install libtinfo-dev libncurses5 libxml2-dev parallel -y

# Subject dependencies
sudo apt-get install libboost-all-dev -y

echo "Installing gcov_for_clang.sh, lcov-filt, and llvm-11..."
pushd ${INSTALL_DIR}

# gcov_for_clang.sh
echo -e '#!/bin/bash\nexec llvm-cov gcov "$@"' > gcov_for_clang.sh
chmod +x gcov_for_clang.sh
sudo mv gcov_for_clang.sh /usr/bin

# lcov-filt
rm -rf lcov-filt
git clone --branch diffcov_initial https://github.com/henry2cox/lcov.git lcov-filt
cd lcov-filt
mkdir install
make install DESTDIR=install
sudo cp install/usr/local/lib/lcovutil.pm /usr/lib/
sudo ln -s $(pwd)/install/usr/local/bin/lcov /usr/bin/lcov-filt
lcov-filt --help
cd ..

# llvm-11.0.1
rm -f clang+llvm-11.0.1-x86_64-linux-gnu-ubuntu-16.04.tar.xz
wget https://github.com/llvm/llvm-project/releases/download/llvmorg-11.0.1/clang+llvm-11.0.1-x86_64-linux-gnu-ubuntu-16.04.tar.xz
tar -xf clang+llvm-11.0.1-x86_64-linux-gnu-ubuntu-16.04.tar.xz
mv clang+llvm-11.0.1-x86_64-linux-gnu-ubuntu-16.04 llvm-11


popd
