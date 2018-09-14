#include <iostream>
#include "hdparm.h"
#include "hugepage.h"
#include "glane_library.h"

int main(int argc, char const *argv[])
{
    std::cout<<"Hello world..."<<std::endl;
    hugepage_main();
    hdparm_main(argv[1],argv[2]);
    return 0;
}
