#ifndef __M_ONLINE_H__
#define __M_ONLINE_H__
#include "util.hpp"
#include <unordered_map>
#include <mutex>
#include <websocketpp/server.hpp>
#include <websocketpp/config/asio_no_tls.hpp>

typedef websocketpp::server<websocketpp::config::asio> wsserver_t;
class online_manager
{
private:
    std::mutex _mutex;
    std::unordered_map<int, wsserver_t::connection_ptr> _hall_user;
    std::unordered_map<int, wsserver_t::connection_ptr> _room_user;
public:
    //进入游戏大厅，将用户id和用户连接放入_game_hall中
    void enter_game_hall(int uid, wsserver_t::connection_ptr &conn)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _hall_user.insert(std::make_pair(uid, conn));
    }
    //进入游戏房间，将用户id和用户连接放入_game_room中
    void enter_game_room(int uid, wsserver_t::connection_ptr &conn)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _room_user.insert(std::make_pair(uid, conn));
    }
    //退出游戏大厅，移除_game_hall中的用户id和用户连接
    void exit_game_hall(int uid)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _hall_user.erase(uid);
    }
    //退出游戏房间，移除_game_room中的用户id和用户连接
    void exit_game_room(int uid)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _room_user.erase(uid);
    }
    //通过用户id获取游戏大厅中的用户连接
    wsserver_t::connection_ptr get_conn_from_hall(int uid)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        auto it = _hall_user.find(uid);
        if(it == _hall_user.end())
        {
            DLOG("Get the connection in the game hall failed!!\n");
            return wsserver_t::connection_ptr();
        }
        return it->second;
    }
    //通过用户id获取游戏房间中的用户连接
    wsserver_t::connection_ptr get_conn_from_room(int uid)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        auto it = _room_user.find(uid);
        if(it == _room_user.end())
        {
            DLOG("Get the connection in the game room failed!!\n");
            return wsserver_t::connection_ptr();
        }
        return it->second;
    }
    //判断是否在游戏大厅
    bool is_in_game_hall(int uid)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        auto it = _hall_user.find(uid);
        if(it == _hall_user.end())
        {
            DLOG("The user is not in the game hall!!\n");
            return false;
        }
        return true;
    }
    //判断是否在游戏房间
    bool is_in_game_room(int uid)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        auto it = _room_user.find(uid);
        if(it == _room_user.end())
        {
            DLOG("The user is not in the game room!!\n");
            return false;
        }
        return true;
    }
};

#endif