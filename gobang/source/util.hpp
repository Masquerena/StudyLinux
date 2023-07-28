#ifndef __M_UTIL_H__
#define __M_UTIL_H__

#include "logger.hpp"
#include <iostream>
#include <string>
#include <mysql/mysql.h>
#include <jsoncpp/json/json.h>
#include <memory>
#include <sstream>
#include <vector>
#include <fstream>

//mysql工具类
class mysql_util
{
public:
    //生成MYSQL对象
    static MYSQL *mysql_create(const std::string host, const std::string username, 
                               const std::string password, const std::string dbname, const int port = 3306)
    {
        //1. 初始化
        MYSQL *ms = mysql_init(nullptr);
        if(ms == nullptr)
        {
            ELOG("init MYSQL error!!\n");
            return nullptr;
        }
        //2. 连接mysql
        if(mysql_real_connect(ms, host.c_str(), username.c_str(), password.c_str(), dbname.c_str(), port, nullptr, 0) == nullptr)
        {
            ELOG("connect MYSQL error!!\n");
            return nullptr;
        }
        //3. 设置字符集
        mysql_set_character_set(ms, "utf8");
        //4. 返回MYSQL对象
        return ms;
    }

    //执行sql语句
    static bool mysql_exec(MYSQL *mysql, const std::string &sql)
    {
        //1. 执行sql语句
        int ret = mysql_query(mysql, sql.c_str());
        if(ret != 0)
        {
            ELOG("%s\n", sql.c_str());
            ELOG("mysql query faild: %s\n", mysql_error(mysql));
            mysql_close(mysql);//sql执行失败，直接返回
            return false;
        }

        return true;
    }

    //销毁MYSQL对象
    static void mysql_destory(MYSQL *mysql)
    {
        if(mysql != nullptr)
            mysql_close(mysql);
    }
};

//json工具类
class json_util
{
public:
    //将传入的Json::Value对象中的内容序列化形成字符串放入str中
    static bool serialize(const Json::Value &root, std::string &str)
    {
        //1. 实例化一个StreamWriterBuilder对象
        Json::StreamWriterBuilder swb;
        //2. 使用StreamWriterBuilder对象生产一个StreamWriter对象
        std::unique_ptr<Json::StreamWriter> sw(swb.newStreamWriter());
        //3. 通过StreamWriter对象对传入的Json::Value对象进行序列化
        std::stringstream ss;//使用stringstream类来存储数据
        int ret = sw->write(root, &ss);
        if(ret != 0)
        {
            ELOG("json serialize failed!!\n");
            return false;
        }
        str = ss.str();

        return true;
    }

    //将传入的字符串str反序列后放入Json::Value对象中
    static bool unserialize(const std::string &str, Json::Value &root)
    {
        //1. 实例化一个CharReaderBuilder工厂类对象
        Json::CharReaderBuilder crb;
        //2. 使用CharReaderBuilder对象生产一个CharReader对象
        std::unique_ptr<Json::CharReader> cr(crb.newCharReader());
        //3. 调用CharReader对象中的parse成员函数进行反序列化
        std::string err;
        bool ret = cr->parse(str.c_str(), str.c_str() + str.size(), &root, &err);
        if(ret == false)
        {
            ELOG("josn unserialize failed!!\n");
            return false;
        }

        return true;
    }
};

//字符串分割工具类
class string_split_util
{
public:
    static int split(const std::string &src, const std::string sep, std::vector<std::string> &res)
    {   
        //.123...123.123.
        size_t pos = 0, idx = 0;
        while(idx < src.size())
        {   
            pos = src.find(sep, idx);//从idx位置找sep
            if(pos == std::string::npos)
            {
                res.push_back(src.substr(idx));//未找到，说明到末尾，后面没有分隔符，直接全部放入res中
                break;
            }
            if(idx == pos)//两个位置相等，说明起始位置就是分隔符，跳过
            {
                idx += sep.size();
                continue;
            }
            res.push_back(src.substr(idx, pos - idx));//找到了，将指定范围内的字符串放入res中
            idx = pos + sep.size();//更新idx的起始位置
        }

        return res.size();//返回切割出来的字符串个数
    }    
};

//文件读取工具类
class file_read_util
{
public:
    static bool read(const std::string &filename, std::string &body)
    {
        //1. 打开文件
        std::ifstream ifs(filename, std::ios_base::binary);//以二进制方式打开文件
        if(ifs.is_open() == false)//检查文件是否打开
        {
            ELOG("%s file open failed!!\n", filename.c_str());
            return false;
        }
        //2. 获取文件大小
        size_t fsize = 0;
        ifs.seekg(0, std::ios_base::end);//将读写位置跳转到文件末尾
        fsize = ifs.tellg();//获取当前读写位置与文件起点的偏移量，以此获得文件大小
        ifs.seekg(0, std::ios_base::beg);//将读写位置跳转到文件起点
        body.resize(fsize);//空间扩容
        //3. 读取文件内容
        ifs.read(&body[0], fsize);//此处不能直接用body.c_str()来获取地址，因为这个函数的返回值是const的，此处读取时需要移动位置
        if(ifs.good() == false)//检查此次读取是否成功
        {
            ELOG("read %s file content filed!!\n", filename.c_str());
            ifs.close();
            return false;
        }
        //4. 关闭文件
        ifs.close();
        return true;
    }
};

#endif