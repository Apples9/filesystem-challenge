# Vscode's c++ compiling wasn't working (macos is painful), so I made a bash script to compile and run it from the command line.
# This probably completely breaks on any other platform
# I stole most of it from stack overflow and vscode's default build config, though I understand how it works
clang -fcolor-diagnostics fileExtractor.cpp -o fileExtractor.out -l c++ -std=gnu++20
./fileExtractor.out