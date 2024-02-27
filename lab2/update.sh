echo Script started
git fetch
mkdir build
cmake -B "build"
cd "build"
make
echo Script finished!
