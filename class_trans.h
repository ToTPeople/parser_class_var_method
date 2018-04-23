/**
 *  作用：
 *      提取类public 方法和变量，并输出到文件中
 */

int start_class_trans(const char* in_file, const char* out_file, const char* rely_file);

int Parser_C_Class(const char* dir_path, bool is_save_in_one_file);
