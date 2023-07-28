#ifndef __M_MATCHER_H__
#define __M_MATCHER_H__
#include "util.hpp"
#include "room.hpp"
#include <condition_variable>
#include <list>
#include <thread>

template <class T>
class match_queue
{
private:
    std::list<T> _list;//保存不同的阻塞队列
    std::mutex _mutex;//C++提供的互斥锁
    std::condition_variable _cond;//C++提供的条件变量

public:
    size_t size()//获取队列中的元素个数
    {
        std::unique_lock<std::mutex> lock(_mutex);
        return _list.size();
    }
    bool empty()//判断队列是否为空
    {
        std::unique_lock<std::mutex> lock(_mutex);
        return _list.empty();
    }
    void wait()//阻塞线程
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _cond.wait(lock);//阻塞线程
    }
    void push(const T &data)//入队
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _list.push_back(data);
        _cond.notify_all();//唤醒线程
    }
    bool pop(T &data)//出队
    {
        std::unique_lock<std::mutex> lock(_mutex);
        if(_list.empty() == true)
        {
            DLOG("队列为空，无法出队\n");
            return false;
        }
        data = _list.front();
        _list.pop_front();
        
        return true;
    }
    bool remove(T &data)//移除指定数据
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _list.remove(data);
    }
};

class matcher
{
private:
    match_queue<int> _q_bronze;
    match_queue<int> _q_silver;
    match_queue<int> _q_gold;//三个匹配队列
    std::thread _th_bronze;
    std::thread _th_silver;
    std::thread _th_gold;//三个匹配队列对应使用的线程
    room_manager *_rm;//房间管理模块句柄
    online_manager *_om;//在线用户管理模块句柄
    user_table *_ut;//数据管理模块句柄

private:
    void handle_match(match_queue<int> &mq)//用于对战匹配
    { 
        while(true)
        {
            //1. 判断当前匹配队列中的用户是否大于2，小于2则阻塞等待
            while(mq.size() < 2)
            {
                mq.wait();
            }
            //2. 将队列中的前两个玩家出队
            int uid1, uid2;
            bool ret = mq.pop(uid1);
            if(ret == false) {continue;}
            ret = mq.pop(uid2);
            if(ret == false) {this->add(uid1); continue;}
            //3. 判断这两个玩家是否在线，不在线则重新入队，并不再执行后面的内容
            wsserver_t::connection_ptr conn1 = _om->get_conn_from_hall(uid1);
            if(conn1.get() == nullptr)//用户1掉线
            {
                this->add(uid2);
                continue;
            }
            wsserver_t::connection_ptr conn2 = _om->get_conn_from_hall(uid2);
            if(conn2.get() == nullptr)//用户2掉线
            {
                this->add(uid1);
                continue;
            }
            //4. 为这两个玩家创建房间，并将玩家加入房间
            room_ptr rp = _rm->create_room(uid1, uid2);
            if(rp.get() == nullptr) 
            {
                this->add(uid1);
                this->add(uid2);
                continue;
            }
            //5. 向两个用户返回响应
            Json::Value resp;
            resp["optype"] = "match_success";
            resp["result"] = true;
            std::string body;
            json_util::serialize(resp, body);
            conn1->send(body);
            conn2->send(body);
        }
    }

public:
    void th_bronze_entry() {handle_match(_q_bronze);}//青铜玩家队列线程入口   
    void th_silver_entry() {handle_match(_q_silver);}//白银玩家队列线程入口  
    void th_gold_entry() {handle_match(_q_gold);}//黄金玩家队列线程入口    

public:
    matcher(room_manager *rm, online_manager *om, user_table *ut)
        :_rm(rm), _om(om), _ut(ut)
    {
        _th_bronze = std::thread(&matcher::th_bronze_entry, this);
        _th_silver = std::thread(&matcher::th_silver_entry, this);
        _th_gold = std::thread(&matcher::th_gold_entry, this);

        DLOG("游戏匹配模块初始化完毕...\n");
    }
    ~matcher()
    {
        _th_bronze.join();
        _th_silver.join();
        _th_gold.join();
    }

    //添加用户
    bool add(int uid)
    {
        //1. 根据用户id获取用户信息
        Json::Value user;
        bool ret = _ut->select_by_id(uid, user);
        if(ret == false)
        {
            ELOG("获取玩家:%d 信息失败\n", uid);
            return false;
        }
        //2. 获取用户信息中的用户分数
        int score = user["score"].asInt();
        //3. 根据用户分数将用户添加进指定的匹配队列
        if(score < 2000)
            _q_bronze.push(uid);
        else if(score >= 2000 && score < 3000)
            _q_silver.push(uid);
        else if(score >= 3000)
            _q_gold.push(uid);

        return true;
    }
    //移除用户
    bool del(int uid)
    {
        //1. 根据用户id获取用户信息
        Json::Value user;
        bool ret = _ut->select_by_id(uid, user);
        if(ret == false)
        {
            ELOG("获取玩家:%d 信息失败\n", uid);
            return false;
        }
        //2. 获取用户信息中的分数
        int score = user["score"].asInt();
        //3. 根据用户分数移除指定匹配队列中的用户
        if(score < 2000)
            _q_bronze.remove(uid);
        else if(score >= 2000 && score < 3000)
            _q_silver.remove(uid);
        else if(score >= 3000)
            _q_gold.remove(uid);

        return true;
    }
};

#endif