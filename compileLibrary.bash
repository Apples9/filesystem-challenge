inFile="fileExtractor"
outFile="pylib"

clang -fPIC  $inFile.cpp -o $outFile.o -c -std=gnu++20
g++ -shared -o $outFile.so $outFile.o
rm $outFile.o