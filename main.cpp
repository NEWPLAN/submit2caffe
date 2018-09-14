#include <iostream>
#include "hdparm.h"
#include "hugepage.h"
#include "glane_library.h"
#include <string>

std::string man_path = "/home/yang/project/imagenet/lmdb/ilsvrc12/train.txt";
std::string folder_path = "/mnt/dc_p3700/imagenet/train/";

int main(void)
{
    std::cout << "Hello world..." << std::endl;
    hugepage_main();
    glane_main();
    hdparm_main(man_path.c_str(), folder_path.c_str());
    return 0;
}
