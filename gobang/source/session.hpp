#ifndef __M_SESSION_H__
#define __M_SESSION_H__
#include "util.hpp"
#include <unordered_map>
#include <websocketpp/server.hpp>
#include <websocketpp/config/asio_no_tls.hpp>

typedef websocketpp::server<websocketpp::config::asio> wsserver_t;
typedef enum{UNLOGIN, LOGIN}ss_statu;
class session
{
private:
    int _ssid;//session的id
    ss_statu _statu;//用户状态
    int _uid;//用户id
    wsserver_t::timer_ptr _tp;//session关联的定时器

public:
    session(const int ssid) 
        :_ssid(ssid), _statu(UNLOGIN)
    {DLOG("SESSIN %d 被创建!!\n", _ssid);}
    ~session() 
    {DLOG("SESSION %d 被销毁!!\n", _ssid);}
    
    void set_user(const int uid)//设置用户id
    {_uid = uid;}

    int get_ssid()//返回ssid
    {return _ssid;}

    int get_user()//获取用户id
    {return _uid;}

    void set_statu(ss_statu statu)//设置登录状态
    {_statu = statu;}

    bool is_login()//判断当前用户是否登录
    {return _statu == LOGIN;}

    void set_timer(const wsserver_t::timer_ptr tp)//设置定时器
    {_tp = tp;}

    wsserver_t::timer_ptr& get_timer()//获取定时器
    {return _tp;}
    
};

using session_ptr = std::shared_ptr<session>;
#define SESSION_TIMEOUT 60000
#define SESSION_FOREVER -1
class session_manager
{
private:
    static int _next_ssid;//session的id
    std::mutex _mutex;//互斥锁
    std::unordered_map<int, session_ptr> _session;//session的id与session信息的映射
    wsserver_t *_server;//websocketpp中的server类对象

    void append_session(const session_ptr &sp)//向session管理器_session中添加映射
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _session.insert(std::make_pair(sp->get_ssid(), sp));
    }

public:
    session_manager(wsserver_t *server)
        :_server(server)
    {
        DLOG("session管理器初始化完成!!\n");
    }
    ~session_manager()
    {
        DLOG("session管理器即将销毁!!\n");
    }

    session_ptr create_session(int uid, ss_statu statu)//创建session
    {
        //1. 创建session类对象
        std::unique_lock<std::mutex> lock(_mutex);
        session_ptr sp(new session(_next_ssid));
        //2. 设置session状态和session用户
        sp->set_statu(LOGIN);
        sp->set_user(uid);
        //3. 管理session类对象
        _session.insert(std::make_pair(_next_ssid, sp));
        ++_next_ssid;
        
        return sp;
    }
    session_ptr get_session_by_ssid(int ssid)//通过session的id获取session信息
    {
        std::unique_lock<std::mutex> lock(_mutex);
        auto it = _session.find(ssid);
        if(it == _session.end())
        {
            DLOG("session: %d, 不存在\n", ssid);
            return session_ptr();
        }
        return it->second;
    }
    void remove_session(int ssid)//销毁session
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _session.erase(ssid);
    }
    void set_session_expire_time(int ssid, int ms)//为session设置超时时间
    {
        //要为session设置超时时间，主要依赖于websocketpp提供的定时器。
        //在登录的时候，就要创建session，这个session需要在一定时间之后删除
        //当用户进入游戏大厅或游戏房间之后，这个session的就要变为永久存在状态
        //当用户退出游戏大厅或游戏房间时，就需要将session重新设置为临时状态，在一定时间后删除
        session_ptr sp = get_session_by_ssid(ssid);
        if(sp.get() == nullptr)
        {
            DLOG("session: %d, 信息不存在\n", ssid);
            return;
        }
        //由此可知，session在创建后会存在四种状态转变
            //1. 在session永久存在的情况下，转化为永久存在
        wsserver_t::timer_ptr tp = sp->get_timer();
        if(tp.get() == nullptr && ms == SESSION_FOREVER)//session模块中的定时器为空，且传入的时间为-1，则永久转永久，什么都不用做
            return;
        else if(tp.get() == nullptr && ms != SESSION_FOREVER)//session模块中的定时器为空，但传入时间不为-1，则添加定时任务
        {
            //2. 在session永久存在的情况下，添加“在一定时间后删除session”的临时任务
            wsserver_t::timer_ptr tmp_tp = _server->set_timer(ms, 
                                            std::bind(&session_manager::remove_session, this, ssid));
            //向session模块中添加定时器
            sp->set_timer(tmp_tp);
        }
        else if(tp.get() != nullptr && ms == SESSION_FOREVER)//session模块中的定时器不为空，但传入时间为-1，则取消定时任务
        {
            //3. 在session有定时任务的情况下，转化为永久存在
            //取消定时任务并将session中的定时器清空
            tp->cancel();//定时任务被取消后，其实并不一定会立即执行，这就可能导致定时任务在重新插入映射信息后执行
            sp->set_timer(wsserver_t::timer_ptr());
            //取消定时任务会导致该任务被执行，重新向session的管理器中添加ssid与session信息的映射，添加时需要用定时器添加，不能直接添加
            _server->set_timer(0, std::bind(&session_manager::append_session, this, sp));
        }
        else if(tp.get() != nullptr && ms != SESSION_FOREVER)//session模块中的定时器不为空，且传入时间不为-1，则重置定时任务
        {
            //4. 在session有定时任务的情况下，重新设置“在一定时间后删除session”的临时任务
            //取消定时任务并将session模块中的定时器清空
            tp->cancel();
            sp->set_timer(wsserver_t::timer_ptr());
            //重新向session管理器中添加session信息
            _server->set_timer(0, std::bind(&session_manager::append_session, this, sp));
            //重新设置定时任务
            wsserver_t::timer_ptr tmp_tp = _server->set_timer(ms, 
                                           std::bind(&session_manager::remove_session, this, sp->get_ssid()));
            //重新设置关联的定时器
            sp->set_timer(tmp_tp);
        }
    }
};
int session_manager::_next_ssid = 1;

#endif