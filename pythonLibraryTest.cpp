#include <iostream>
#include <stdio.h>

int runSum = 0;

int addTestC(int a, int b) {
    std::cout << a + b << '\n';
    return a + b;
}

extern "C" {
    int addTest(int a, int b){
        return addTestC(a, b);
    }
    int sum() {
        runSum += 1;
        return runSum;
    }
    char* strTest() {
        char str[] = "";
        return "testing testing \n123";
    }
}
