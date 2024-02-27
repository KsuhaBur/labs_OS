echo Script started
git fetch
mkdir build
cmake -G "MinGW Makefiles" -B "build"
cd "build"
mingw32-make
echo Script finished!

