#include <stdio.h>
#include <string>
#include <stdlib.h>
#include "class_trans.h"

int main(int argc, char **argv)
{
    if (argc != 2) {
        printf("----- please input ./a.out trans_file_name ----\n");
        return 0;
    }
    
    std::string str_name("");
    int len = strlen(argv[1]);
    bool b_find = false;
    for (int i = len - 1; i >= 0; --i) {
        if (argv[1][i] == '/') {
            b_find = true;
            for (int j = i+1; j < len; ++j) {
                str_name += argv[1][j];
            }
            break;
        }
    }
    if (!b_find) {
        str_name = argv[1];
    }
    
    std::string str_in_file("./need_trans/");
    std::string str_out_file("./trans/sol_trans_");
    std::string str_rely_file("./trans/rely.txt");
    
    str_in_file += str_name;
    str_out_file += str_name;
    
    start_class_trans(str_in_file.c_str(), str_out_file.c_str(), str_rely_file.c_str());
    
    return 0;
}
