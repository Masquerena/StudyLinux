#ifndef __M_DB_H__
#define __M_DB_H__
#include "util.hpp"
#include <mutex>
#include <cassert>

class user_table
{
private:
    MYSQL *_mysql;
    std::mutex _mutex; 

public:
    //构造MYSQL对象
    user_table(const std::string host, const std::string username, 
               const std::string password, const std::string dbname, const int port = 3306)
    {
        _mysql = mysql_util::mysql_create(host, username, password, dbname, port);
        assert(_mysql != nullptr);
    }
    //销毁MYSQL对象
    ~user_table()
    {
        mysql_util::mysql_destory(_mysql);
        _mysql = nullptr;
    }

    //注册时新增用户
    bool insert(Json::Value &user)
    {
        if(user["username"].isNull() || user["password"].isNull())
        {
            DLOG("Please enter a username or password\n");
            return false;
        }
        //1. 定义要插入的语句
        #define INSERT_USER "insert into user values (null, '%s', password('%s'), 1000, 0, 0);"
        //2. 根据注册时用的用户名判断该用户是否已经存在
        Json::Value val;
        bool ret = select_by_name(user["username"].asString(), val);
        if(ret)//ret为true说明已经存在同名用户
        {
            ELOG("user: %s already exists!!\n", user["username"].asCString());
            return false;
        }
        //3. 格式化字符串，形成sql语句
        char sql[1024] = {0};
        sprintf(sql, INSERT_USER, user["username"].asCString(), user["password"].asCString());
        //4. 执行sql语句
        ret = mysql_util::mysql_exec(_mysql, sql);
        if(ret == false)
        {
            ELOG("insert user info failed!!\n");
            return false;
        }

        return true;
    }
    //以用户名和用户密码作为标准进行登录验证
    bool login(Json::Value &user)
    {
        if(user["username"].isNull() || user["password"].isNull())
        {
            DLOG("Please enter a username or password\n");
            return false;
        }
        //1. 定义sql语句
        #define LOGIN_USER "select id, score, total_count, win_count from user where username='%s' and password=password('%s');"
        //2. 格式化字符串，形成sql语句
        char sql[1024] = {0};
        sprintf(sql, LOGIN_USER, user["username"].asCString(), user["password"].asCString());
        //加锁
        MYSQL_RES *res = nullptr;
        {
            std::unique_lock<std::mutex> lock(_mutex);//使用C++库中提供的锁类中的智能锁，让其离开作用域后自行销毁
            //3. 执行sql语句
            bool ret = mysql_util::mysql_exec(_mysql, sql);
            if(ret == false)
            {
                ELOG("user login failed!!\n");
                return false;
            }
            //4. 获取查询结果
            res = mysql_store_result(_mysql);
            if(res == nullptr)
            {
                DLOG("user: %s does not exist!!\n", user["username"].asCString());
                return false;
            }
        }
        //5. 处理查询结果
        int row_num = mysql_num_rows(res);//获取查询结果的行数
        if(row_num != 1)
        {
            ELOG("There are multiple user: %s\n", user["username"].asCString());
            return false;
        }
        MYSQL_ROW row = mysql_fetch_row(res);
        user["id"] = std::stoi(row[0]);
        user["score"] = std::stoi(row[1]);
        user["total_count"] = std::stoi(row[2]);
        user["win_count"] = std::stoi(row[3]);

        return true;
    }
    //根据用户名找到用户信息
    bool select_by_name(const std::string username, Json::Value &user)
    {
        //1. 定义sql语句
        #define USER_BY_NAME "select id, score, total_count, win_count from user where username='%s';"
        //2. 格式化字符串，生成sql语句
        char sql[1024] = {0};
        sprintf(sql, USER_BY_NAME, username.c_str());
        //加锁
        MYSQL_RES *res = nullptr;
        {
            std::unique_lock<std::mutex> lock(_mutex);
            //3. 执行sql语句
            bool ret = mysql_util::mysql_exec(_mysql, sql);
            if(ret == false)
            {
                DLOG("get user by name failed!!\n");
                return false;
            }
            //4. 获取查询结果
            res = mysql_store_result(_mysql);
            if(res == nullptr)
            {
                DLOG("User information does not exist!!\n");
                return false;
            }
        }
        //5. 处理查询结果
        int row_num = mysql_num_rows(res);
        if(row_num != 1)
        {
            DLOG("There are multiple or don`t exists. user: %s\n", username.c_str());
            return false;
        }
        MYSQL_ROW row = mysql_fetch_row(res);
        user["username"] = username.c_str();
        user["id"] = std::stoi(row[0]);
        user["score"] = std::stoi(row[1]);
        user["total_count"] = std::stoi(row[2]);
        user["win_count"] = std::stoi(row[3]);
        mysql_free_result(res);
        return true;
    }
    //根据用户id找到用户信息
    bool select_by_id(const int id, Json::Value &user)
    {
        //1. 定义sql语句
        #define USER_BY_ID "select username, score, total_count, win_count from user where id='%d';"
        //2. 格式化字符串，生成sql语句
        char sql[1024] = {0};
        sprintf(sql, USER_BY_ID, id);
        //加锁
        MYSQL_RES *res = nullptr;
        {
            std::unique_lock<std::mutex> lock(_mutex);
            //3. 执行sql语句
            bool ret = mysql_util::mysql_exec(_mysql, sql);
            if(ret == false)
            {
                DLOG("get user by id failed!!\n");
                return false;
            }
            //4. 获取查询结果
            res = mysql_store_result(_mysql);
            if(res == nullptr)
            {
                DLOG("User information does not exist!!\n");
                return false;
            }
        }
        //5. 处理查询结果
        int row_num = mysql_num_rows(res);
        if(row_num != 1)
        {
            ELOG("There are multiple or don`t exists. id: %d\n", id);
            return false;
        }
        MYSQL_ROW row = mysql_fetch_row(res);
        user["id"] = id;
        user["username"] = row[0];
        user["score"] = std::stoi(row[1]);
        user["total_count"] = std::stoi(row[2]);
        user["win_count"] = std::stoi(row[3]);

        return true;
    }
    //获胜时，增加分数、对战总场次和获胜场次
    bool win(const int id)
    {
        //1. 定义sql语句
        #define USER_WIN "update user set score=score+30, total_count=total_count+1, win_count=win_count+1 where id=%d;"
        //2. 格式化字符串，形成sql语句
        char sql[1024] = {0};
        sprintf(sql, USER_WIN, id);
        //3. 执行sql语句
        bool ret = mysql_util::mysql_exec(_mysql, sql);
        if(ret == false)
        {
            ELOG("upadate win user infomation failed, id: %d !!\n", id);
            return false;
        }

        return true;
    }
    //失败时，减少分数，增加对战总场次
    bool lose(const int id)
    {
        //1. 定义sql语句
        #define USER_LOSE "update user set score=score-30, total_count=total_count+1 where id=%d;"
        //2. 格式化字符串，生成sql语句
        char sql[1024] = {0};
        sprintf(sql, USER_LOSE, id);
        //3. 执行sql语句
        bool ret = mysql_util::mysql_exec(_mysql, sql);
        if(ret == false)
        {
            ELOG("update lose user infomation failed, id: %d !!\n", id);
            return false;
        }

        return true;
    }
};

#endif