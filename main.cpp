#include <iostream>
#include "hdparm.h"

int main(int argc, char const *argv[])
{
    std::cout<<"Hello world..."<<std::endl;
    hdparm_main(argv[1],argc[2]);
    return 0;
}
