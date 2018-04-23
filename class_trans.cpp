#include "class_trans.h"
#include <string>
#include <map>
#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <dirent.h>
#include <unistd.h>

//#define DEBUG_SS
//#define DEBUG_PARAMS
#define OUT_TO_FILE                 // 是否写到文件
//#define DEBUG_LEFT_BRACKETS         // {} 测试

#define NORMAL_LEFT_DISTANCE    "                "              // 普通函数、变量左侧间隔
#define OVERLOAD_LEFT_DISTANCE    "                        "    // 重载函数左侧间隔

namespace {
    const int g_max_overload_count = 10;        // 最多重载函数个数
    const int g_line_max_char_count = 512;
    
    enum AttriType {
        AttriTypePublic = 0,
        AttriTypeOther = 1,
    };
    
    struct ParserFuncVarSaveData {
        std::string return_type;            // 函数返回值类型
        int         cnt_param;              // 函数参数个数
        std::string params[16];             // 函数参数列表
        std::string func_discript;          // func后面 const等修示
    };
    
    struct ParserSaveData {
        int                     cnt;                        // 重载函数个数，若是变量则为0
        std::string             func_var_name;              // 函数名 或 变量名
        ParserFuncVarSaveData   data[g_max_overload_count]; // 函数数据结构
    };
    
    std::map< std::string, ParserSaveData > mp_func_var_data;       // 解析 变量、函数 数据结构保存
    std::map< std::string, int > mp_inter_type;
    std::map< std::string, int > mp_outer_type;                     // 外部依赖 数据结构
    std::string                  str_class_name;                    // 类名
    int                          left_brackets;                     // 左(个数
    AttriType                    attr_type;                         // 当前属性：public protected private
    bool                         has_operator_overload;             // 是否有 操作符重载
    bool                         has_std_function;                  // 是否有 std::function 类型
}

// 去掉左右空格、换行符 或 指定去掉字符
std::string trim_string(std::string s, std::string filter = " \t")
{
    if (s.empty())
    {
        return s;
    }
    
    s.erase(0, s.find_first_not_of(filter));
    s.erase(s.find_last_not_of(filter) + 1);
    
    return s;
}

// 内部类型初始化
void init_inter_type()
{
    mp_inter_type.clear();
    
    mp_inter_type["int"] = 1;
    mp_inter_type["float"] = 1;
    mp_inter_type["double"] = 1;
    mp_inter_type["std::string"] = 1;
    mp_inter_type["bool"] = 1;
    mp_inter_type["long"] = 1;
    mp_inter_type["char"] = 1;
}

void init_class_data()
{
    mp_func_var_data.clear();
    mp_outer_type.clear();
    left_brackets = 0;
    attr_type = AttriTypePublic;
    str_class_name.clear();
}

void write_to_file(const char* out_file, const char* rely_file)
{
    if (NULL == out_file || NULL == rely_file) {
        printf("[write_to_file] error: NULL == out_file || NULL == rely_file \n");
        exit(-1);
    }
    
    // 变量、函数 解析输出文件
    std::ofstream out;
    out.open(out_file, std::ios::app);
    if (!out.is_open()) {
        printf("[write_to_file] error: cannot open file[%s] \n", out_file);
        exit(-1);
    }
    // 类型依赖 输出文件
    std::ofstream rely_out;
    rely_out.open(rely_file, std::ios::app);
    if (!rely_out.is_open()) {
        printf("[write_to_file] error: cannot open rely_file[%s] \n", rely_file);
        exit(-1);
    }
    
    out << "\ng_pCSolObjectMgr->trans_type_2_lua<" << str_class_name << ">(\"" << str_class_name << "\"";
    // 空的constructs
    out << ", sol::constructors<()>()" << std::endl;
    
    ///////
    std::map< std::string, ParserSaveData >::iterator iter = mp_func_var_data.begin();
    int cnt = 0;
    for (; iter != mp_func_var_data.end(); ++iter) {
        cnt = iter->second.cnt;
        if (cnt <= 0) {
            // 变量
            std::string var = iter->first;
            out << NORMAL_LEFT_DISTANCE << ", \"" << var << "\", &" << str_class_name << "::" << var << std::endl;
        } else {
            // 函数
            ParserSaveData funcData = iter->second;
            std::string strP("");
            std::string strConst("");
            int pa_cnt;
            bool is_overload = (cnt > 1) ? true : false;
            if (is_overload) {
                out << NORMAL_LEFT_DISTANCE << ", \"" << funcData.func_var_name << "\", sol::overload(" << std::endl;
            }
            
            for (int i = 0; i < cnt; ++i ) {
                strConst = funcData.data[i].func_discript;
                strP = "";
                pa_cnt = funcData.data[i].cnt_param;
                for (int j = 0; j < pa_cnt; ++j) {
                    if (j > 0) {
                        strP += ", ";
                    }
                    strP += funcData.data[i].params[j];
                    
                    // 依赖参数处理 ******************
                    std::string strParam = funcData.data[i].params[j];
                    if (mp_inter_type.end() == mp_inter_type.find(strParam)
                        || mp_outer_type.end() == mp_outer_type.find(strParam)) {
                        mp_outer_type[strParam] = 1;
                    }
                }
                
                // 依赖返回值处理 ******************
                std::string strRT = funcData.data[i].return_type;
                if (mp_inter_type.end() == mp_inter_type.find(strRT)
                    || mp_outer_type.end() == mp_outer_type.find(strRT)) {
                    mp_outer_type[strRT] = 1;
                }

                // 输出
                if (is_overload) {
                    // 重载
                    out << OVERLOAD_LEFT_DISTANCE;
                    if (i > 0) {
                        out << ", ";
                    }
                    out << "sol::resolve<" << funcData.data[i].return_type << "(" << strP << ")" << strConst << ">(&" << str_class_name << "::" << funcData.func_var_name << ")" << std::endl;
                } else {
                    // 普通
                    out << NORMAL_LEFT_DISTANCE << ", \"" << funcData.func_var_name << "\", &" << str_class_name << "::" << funcData.func_var_name << std::endl;
                }
                // 构造
                // 父类
            }
            if (is_overload) {
                out << OVERLOAD_LEFT_DISTANCE << ")" << std::endl;
            }
        }
    }
    
    // 空的继承
    out<< NORMAL_LEFT_DISTANCE << ", sol::base_classes, sol::bases<>()" << std::endl;
    
    out << NORMAL_LEFT_DISTANCE << ");" << std::endl;
    
    
    // 类型依赖 输出到文件
    rely_out << "\n" << str_class_name << "-----rely type below: " << std::endl;
    std::map< std::string, int >::iterator rely_iter = mp_outer_type.begin();
    for (; rely_iter != mp_outer_type.end(); ++ rely_iter) {
        rely_out << "\t\t" << rely_iter->first << std::endl;
    }
    // 是否 有std::function 、 操作符重载 记录
    if (has_std_function) {
        rely_out << "\thas std::function params, need check by yourself!!!\n" << std::endl;
    }
    if (has_operator_overload) {
        rely_out << "\thas operator overload params, need check by yourself!!!\n" << std::endl;
    }
    ///////
    
    out.close();
    rely_out.close();
}

void update_left_brackets(char* buf_bak, int &left_brackets, int line)
{
    if (NULL == buf_bak) {
        printf("[update_left_brackets] error: NULL == buf_bak\n");
        exit(-1);
    }
    
    // 更新 { } 个数
    int len = strlen(buf_bak);
    bool b_update = false;
    for (int i = 0; i < len; ++i) {
        if (buf_bak[i] == '{') ++ left_brackets, b_update = true;
        else if (buf_bak[i] == '}') -- left_brackets, b_update = true;
    }
    
#ifdef DEBUG_LEFT_BRACKETS
    if (b_update) {
        printf("#### now state line[%d], left_brackets[%d], buf_bak[%s] ####\n", line, left_brackets, buf_bak);
    }
#endif
}

int start_class_trans(const char* in_file, const char* out_file, const char* rely_file)
{
    if (NULL == in_file || NULL == out_file) {
        printf("[start_class_trans] error: NULL == in_file || NULL == out_file");
        exit(-1);
    }
    
    init_inter_type();
    init_class_data();
    has_operator_overload = false;
    has_std_function = false;
    
    // 打开文件读、解析
    char buf[g_line_max_char_count];
    std::ifstream in(in_file);
    if (!in.is_open()) {
        std::cout << "Error opening in_file";
        exit (1);
    }
    
    char tmp[g_line_max_char_count];
    char buf_bak[g_line_max_char_count];
    bool is_class_name_parser;
    bool is_star_tips = false;              // 是否是 /**/注释内
    int line = 0;
    while ( !in.eof() ) {
        ++ line;
        in.getline(buf, g_line_max_char_count - 1);
        strcpy(buf_bak, buf);
#ifdef DEBUG_SS
        printf("================ before trim [%s] =============\n", buf);
#endif
        
        std::string tmp_buf = trim_string(std::string(buf));
        strcpy(buf, tmp_buf.c_str());
#ifdef DEBUG_SS
        printf("================ after trim [%s] =============\n", buf);
#endif
        
        is_class_name_parser = false;
        if (0 == strncmp(buf, "friend class", 12)) {
            continue;
        } else if (0 == strncmp(buf, "class ", 6)) {
            int len = strlen(buf);
            if (';' == buf[len - 1]) {
                // class XXX;  声明，跳过，不解析
                continue;
            }
            
            if (left_brackets > 0) {
                printf("not support: class in class!\n");
                exit(1);
            }
            sscanf(buf, "%*s %s ", tmp);
            str_class_name = tmp;
            is_class_name_parser = true;
            
        } else if (left_brackets <= 0 && !str_class_name.empty()) {
            // 更新 { } 个数
            update_left_brackets(buf_bak, left_brackets, line);
            
            continue;
        } else if (left_brackets <= 0) {
            continue;
        } else if (0 == strcmp(buf, "};") && !str_class_name.empty()) {
            // out result **********************
            -- left_brackets;
            if (left_brackets > 0) {
                printf("******** maybe error: line[%d], left_brackets[%d], buf_bak[%s] ***********\n", line, left_brackets, buf_bak);
                continue;
            }
            // 写文件
            printf("class: %s left_brackets[%d], line[%d]\n", str_class_name.c_str(), left_brackets, line);
#ifdef OUT_TO_FILE
            write_to_file(out_file, rely_file);
#else
            ////////////////////
            std::map< std::string, ParserSaveData >::iterator iter = mp_func_var_data.begin();
            int cnt = 0;
            for (; iter != mp_func_var_data.end(); ++iter) {
                cnt = iter->second.cnt;
                if (cnt <= 0) {
                    printf("\t var[%s]\n", iter->first.c_str());
                } else {
                    ParserSaveData funcData = iter->second;
                    std::string strP("");
                    std::string strConst("");
                    int pa_cnt;
                    for (int i = 0; i < cnt; ++i ) {
                        strConst = funcData.data[i].func_discript;
                        strP = "";
                        pa_cnt = funcData.data[i].cnt_param;
                        for (int j = 0; j < pa_cnt; ++j) {
                            if (j > 0) {
                                strP += ", ";
                            }
                            strP += funcData.data[i].params[j];
                        }
                            
                        printf("\t %s %s(%s)%s;\n", funcData.data[i].return_type.c_str(), funcData.func_var_name.c_str(), strP.c_str(), strConst.c_str());
                    }
                }
            }
            ////////////////////
#endif
            // 初始化
            init_class_data();
            
            continue;
        } else if (0 == strncmp(buf, "public:", 7)) {
            attr_type = AttriTypePublic;
        } else if (0 == strncmp(buf, "protected:", 10)
                   || 0 == strncmp(buf, "private:", 8)) {
            attr_type = AttriTypeOther;
            continue;
        } else if (AttriTypePublic != attr_type) {
            continue;
        }
        
#ifdef DEBUG_SS
        printf("================ before find ( [%s] =============\n", buf);
#endif
        
        // 解析处理
        if (is_class_name_parser) {
            // 不处理
        } else if (is_star_tips) {
            // 前面有/* 查找后面 */
            if (NULL != strstr(buf, "*/")) {
                is_star_tips = false;
            }
            continue;
        } else if (strlen(buf) > 1 && buf[0] == '/' && buf[1] == '*') {
            // 注释不解析
            if (NULL == strstr(buf + 2, "*/")) {
                is_star_tips = true;
            }
            continue;
        } else if (strlen(buf) > 1 && buf[0] == '/' && buf[1] == '/') {
            // 注释//不解析
            continue;
        } else if (NULL == strstr(buf, "(")) {
            // 函数中内容不解析，只解析类中函数和变量
            if (left_brackets == 1) {
                // 变量解析
                char *p = NULL;
                p = strtok(buf, " ");
                // 吃掉 static const inline 关键字
                while (NULL != p) {
                    if (0 == strncmp(p, "static", 6)) {
                        p = strtok(NULL, " ");
                        continue;
                    } else if (0 == strncmp(p, "const", 5)) {
                        p = strtok(NULL, " ");
                        continue;
                    } else if (0 == strncmp(p, "inline", 6)) {
                        p = strtok(NULL, " ");
                        continue;
                    }
                    break;
                }
                p = strtok(NULL, " ");
                if (NULL != p) {
                    if (0 == strncmp(p, "Vec2", 4)) {
                        printf("^^^^^^^^^^^^^^ var line[%d], buf_bak[%s] ===========\n\n", line, buf_bak);
                    } else if (0 == strncmp(p, "const", 5)) {
                        printf("^^^^^^^^^^^^^^ var line[%d], buf_bak[%s] ===========\n\n", line, buf_bak);
                    }
                    // 去除空格、;号
                    std::string str_var = trim_string(std::string(p));
                    str_var = trim_string(str_var, ";");
                    // 保存变量数据
                    ParserSaveData varData;
                    varData.cnt = 0;
                    varData.func_var_name = str_var;
                    mp_func_var_data[varData.func_var_name] = varData;
                }
            }
        } else {
            // 函数中内容不解析，只解析类中函数和变量
            if (left_brackets == 1) {
                // 参数多行情况，转化成单行 ===========================
                std::string mul_buf(buf_bak);
                while ( !in.eof() ) {
                    if (NULL != strstr(buf, ")")) {
                        break;
                    }
                    in.getline(buf, g_line_max_char_count - 1);
                    mul_buf += buf;
                    mul_buf = trim_string(mul_buf, "\n");
                    mul_buf = trim_string(mul_buf, "\r");
                    mul_buf = trim_string(mul_buf, "\t");
                    mul_buf = trim_string(mul_buf, " ");
                    
                    int len = mul_buf.length();
                    strncpy(buf, mul_buf.c_str(), len);
                    strncpy(buf_bak, mul_buf.c_str(), len);
                    buf[len] = mul_buf[len] = '\0';
                    
//                    printf("========== buf[%s], buf_bak[%s], mul_buf[%s]  =========\n\n", buf, buf_bak, mul_buf.c_str());
                }
                
                // 单行情况
                ParserSaveData varData;
                varData.cnt = 0;
                varData.func_var_name = "";
#ifdef DEBUG_SS
                printf("************* buf[%s] ******\n", buf);
#endif
                // ************* 函数参数有std::function pass
                if (NULL != strstr(buf, "std::function")) {
                    has_std_function = true;
                    update_left_brackets(buf_bak, left_brackets, line);
                    continue;
                }
                // ************* 函数参数有std::function pass
                
                // 函数
                char *left_b = NULL;
                char *right_b = NULL;
                char *param = NULL;
                left_b = strtok(buf, "(");
                param = strtok(NULL, ")");
                right_b = strtok(NULL, ")");
                if (right_b == NULL) {
                    right_b = param;
                    param = NULL;
                }
#ifdef DEBUG_SS
                printf("------------ line[%d], left[%s], param[%s], right[%s] ------\n", line, left_b, param, right_b);
#endif
                char *p = NULL;
                std::string strType("");
                std::string strName("");
                // return function 函数返回值、函数名处理 --------------------
                // 从后往前扫，遇到 ' '、'>'、'*'、'&' 等截断
                std::string str_left(left_b);
                str_left = trim_string(str_left);
                int l_len = str_left.length();
                bool is_find = false;
                for (int i = l_len - 2; i >= 0; --i) {      // 最后一个不判断，，可能是operator *
                    char ch = str_left.at(i);
                    if (' ' == ch || '>' == ch || '*' == ch || '&' == ch) {
                        // name
                        for (int j = i+1; j < l_len; ++j) {
                            strName += str_left.at(j);
                        }
                        str_left = str_left.substr(0, i+1);
                        is_find = true;
                        break;
                    } else if ('~' == ch) {
                        break;
                    }
                }
                if (0 == strncmp(strName.c_str(), "operator", 8)) {
                    // ************* 操作符重载 pass
                    has_operator_overload = true;
                    update_left_brackets(buf_bak, left_brackets, line);
                    // ************* 操作符重载 pass end
                    continue;
                }
                // type
                if (is_find) {
                    strncpy(left_b, str_left.c_str(), str_left.length());
                    left_b[str_left.length()] = '\0';
                    p = strtok(left_b, " ");
                    bool should_ignore = false;
//                    printf("----- line[%d], buf_bak[%s], left_b[%s], str_left[%s] +++++++++++ \n\n", line, buf_bak, left_b, str_left.c_str());
                    while (NULL != p) {
                        strcpy(p, trim_string(std::string(p)).c_str());
//                        printf("+++++++ p[%s] ++++\n", p);
                        if (0 == strncmp(p, "static", 6)) {
                            p = strtok(NULL, " ");
                            continue;
                        } else if (0 == strncmp(p, "virtual", 7)) {
                            p = strtok(NULL, " ");
                            continue;
                        } else if (0 == strncmp(p, "inline", 6)) {
                            p = strtok(NULL, " ");
                            continue;
                        } else if (0 == strncmp(p, "operator", 8)) {
                            // ************* 操作符重载 pass
                            has_operator_overload = true;
                            update_left_brackets(buf_bak, left_brackets, line);
                            should_ignore = true;
                            // ************* 操作符重载 pass end
                            break;
                        }
                        strType += p;
                        strType += " ";
                        p = strtok(NULL, " ");
                    }
                    
                    strType = trim_string(strType);
                    
                    if (should_ignore) {
                        continue;
                    }
                    
                    if (mp_func_var_data.end() != mp_func_var_data.find(strName)) {
                        // 已存在
                        ++ mp_func_var_data[strName].cnt;
                        mp_func_var_data[strName].data[mp_func_var_data[strName].cnt - 1].return_type = strType;
                    } else {
                        // 不存在
                        mp_func_var_data[strName].cnt = 1;
                        mp_func_var_data[strName].func_var_name = strName;
                        mp_func_var_data[strName].data[0].return_type = strType;
                        mp_func_var_data[strName].data[0].cnt_param = 0;
                        mp_func_var_data[strName].data[0].func_discript = "";
                    }
                } else {
                    printf("===== maybe error: line[%d], buf_bak[%s] -----\n", line, buf_bak);
                    update_left_brackets(buf_bak, left_brackets, line);
                    continue;
                }
                // --------------------
                if (strName.empty()) {
                    continue;
                }
                int idx = mp_func_var_data[strName].cnt - 1;
                // 函数参数处理 --------------------
                if (NULL != param) {
#ifdef DEBUG_PARAMS
                    printf("----------- buf_bak[%s] --------\n", buf_bak);
#endif
                    // 以,分隔出参数；再从后往前分离出类型
                    p = strtok(param, ",");
                    while (NULL != p) {
#ifdef DEBUG_PARAMS
                        printf("----------- p[%s] --------\n", p);
#endif
                        // 从后往前分离出类型，  空格 * & ... >
                        int len = strlen(p);
                        std::string strParamType("");
                        for (int i = len-1; i >= 0; --i) {
                            char ch = p[i];
                            if ('*' == ch || '.' == ch || '>' == ch) {
                                for (int j = 0; j <= i && p[j] != '='; ++j) {
                                    strParamType += p[j];
                                }
                                break;
                            } else if (' ' == ch) {
                                for (int j = 0; j < i && p[j] != '='; ++j) {
                                    strParamType += p[j];
                                }
                                break;
                            }
                        }
                        // 更新保存数据
                        strParamType = trim_string(strParamType);
                        int pa_idx = mp_func_var_data[strName].data[idx].cnt_param;
                        mp_func_var_data[strName].data[idx].params[pa_idx] = strParamType;
                        ++ mp_func_var_data[strName].data[idx].cnt_param;
                        
#ifdef DEBUG_PARAMS
                        printf("************** pa_idx[%d], strParamType[%s] --------\n", pa_idx, strParamType.c_str());
#endif
                        
                        p = strtok(NULL, ",");
                    }
                }
                // 后面修示const解析 --------------------
                if (NULL != right_b) {
                    std::string tmp_buf = trim_string(std::string(right_b));
                    tmp_buf = trim_string(tmp_buf, ";");
                    std::string func_dis("");
                    int ll = tmp_buf.length();
                    for (int i = 0; i < ll; ++i) {
                        char ch = tmp_buf.at(i);
                        if (' ' == ch || '{' == ch || ';' == ch) {
                            break;
                        }
                        
                        func_dis += ch;
                    }
                    
                    mp_func_var_data[strName].data[idx].func_discript = func_dis;
                }
            }
        }
        
        // 更新 { } 个数
        update_left_brackets(buf_bak, left_brackets, line);
        
#ifdef DEBUG_LEFT_BRACKETS
        if (left_brackets <= 0) {
            printf("-------- line[%d], buf_bak[%s], left_brackets[%d], str_class_name[%s] =========\n", line, buf_bak, left_brackets, str_class_name.c_str());
        }
#endif
    }
    
    in.close();
    
    return 0;
}

// 解析指定目录下文件的类
int Parser_C_Class(const char* dir_path, bool is_save_in_one_file)
{
    if (NULL == dir_path || 0 == strcmp(dir_path, "")) {
        printf("[Parser_C_Class] error: NULL == dir_path || dir_path is empty.\n");
        exit(-1);
    }
    
    struct dirent *ptr;
    DIR *dir;
    char in_file[1024], out_file[1024], rely_file[1024];
    char cur_path[1024];
    
    dir = opendir(dir_path);
    if (NULL == dir) {
        printf("[Parser_C_Class] warning: open directory[%s] failed.\n", dir_path);
        exit(-1);
    }
    
    getcwd(cur_path, 1024);
    
    strcpy(rely_file, cur_path);
    strcat(rely_file, "/result/rely.txt");
    
    while ((ptr = readdir(dir)) != NULL) {
        // 跳过 .开头文件 及 非file类型文件
        if (ptr->d_name[0] == '.' || DT_REG != ptr->d_type) {
            continue;
        }
        
        printf("[Parser_C_Class] info: file[%s], type[%d]\n", ptr->d_name, ptr->d_type);
        // 读取.lua文件加载
        char* pos = strrchr(ptr->d_name, '.');
        if (0 == strcmp(pos+1, "h") || 0 == strcmp(pos+1, "hpp")) {
            strcpy(in_file, dir_path);
            strcat(in_file, "/");
            strcat(in_file, ptr->d_name);
            
            strcpy(out_file, cur_path);
            strcat(out_file, "/result/sol_trans_");
            if (is_save_in_one_file) {
                strcat(out_file, "result.cpp");
            } else {
                strcat(out_file, ptr->d_name);
            }
            
            start_class_trans(in_file, out_file, rely_file);
        }
    }
    
    closedir(dir);
    
    printf("[Parser_C_Class] info: well done! it's the end.\n");
    
    return 0;
}

