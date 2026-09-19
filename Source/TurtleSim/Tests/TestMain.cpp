#include "TestFramework.h"

#if !defined(__UNREAL__)
int main() {
    std::printf("TurtleSim tests\n");
    return turtlesim_test::RunAll();
}
#endif
