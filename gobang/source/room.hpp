#ifndef __M_ROOM_H__
#define __M_ROOM_H__

#include "util.hpp"
#include "db.hpp"
#include "online.hpp"

typedef enum{GAME_START, GAME_OVER} room_status;
#define BOARD_ROW 15
#define BOARD_COL 15//棋盘的长和宽
#define WHITE_CHESS 1
#define BLACK_CHESS 2//表示白棋和黑棋
class room
{
private:
    int _room_id;//房间id
    int _black_id;//黑棋用户id
    int _white_id;//白棋用户id
    int _play_count;//房间内的用户数量
    room_status _statu;//房间状态
    user_table *_tb_user;//数据表句柄
    online_manager *_online_user;//在线用户句柄
    std::vector<std::vector<int>> _board;//棋盘信息

    bool five(int row, int col, int row_off, int col_off, int color)//判断是否有五个颜色相同的连线
    {
        int count = 1;
        int search_row = row + row_off;
        int search_col = col + col_off;
        while(search_row >= 0 && search_row < BOARD_ROW &&
              search_col >= 0 && search_col < BOARD_COL &&
              _board[search_row][search_col] == color)
        {
            ++count;
            search_row += row_off;
            search_col += col_off;
        }

        search_row = row - row_off;
        search_col = col - col_off;
        while(search_row >= 0 && search_row < BOARD_ROW &&
              search_col >= 0 && search_col < BOARD_COL &&
              _board[search_row][search_col] == color)
        {
            ++count;
            search_row -= row_off;
            search_col -= col_off;
        }

        return (count == 5) ? true : false;
    }

    int check_win(int row, int col, int color)//判断该步走棋后当前用户是否胜利
    {
        if(five(row, col, 0, 1, color) ||
           five(row, col, 1, 0, color) || 
           five(row, col, -1, -1, color) || 
           five(row, col, 1, 1, color))
           {
            //任意一个位置为true，则说明该用户获得胜利
            return color == WHITE_CHESS ? _white_id : _black_id;//返回胜利者的id
           }
    
        return 0;
    }

    void resp_create(Json::Value &resp, std::string &reason, int winner_id)//填充Json:Value对象
    {
        resp["result"] = true;
        resp["reason"] = reason;
        resp["winner"] = winner_id;
    }

public:
    room(int room_id, user_table *tb_user, online_manager *online_user)
        :_room_id(room_id), _play_count(0), _statu(GAME_START), _tb_user(tb_user),
         _online_user(online_user), _board(BOARD_ROW, std::vector<int>(BOARD_COL))
    {
        DLOG("房间创建成功, 房间id: %d\n", _room_id);
    }
    ~room()
    {
        DLOG("房间销毁成功, 房间id: %d\n", _room_id);
    }

    int id() {return _room_id;}//返回房间id
    room_status statu() {return _statu;}//返回房间状态
    int play_count() {return _play_count;}//返回当前房间内的用户数量
    void add_white_user(int id) {_white_id = id; ++_play_count;}//添加白棋用户
    void add_black_user(int id) {_black_id = id; ++_play_count;}//添加黑棋用户
    int get_white_user() {return _white_id;}//返回白棋用户id
    int get_black_user() {return _black_id;}//返回黑棋用户id

    //根据请求处理下棋动作
    Json::Value handle_chess(Json::Value &req)
    {
        Json::Value json_resp = req;
        //1. 判断该房间内的两个用户是否都在线，有一个不在线则对方获得胜利
        int chess_row = req["row"].asInt();
        int chess_col = req["col"].asInt();
        int cur_id = req["uid"].asInt();//获取当前用户
        if(_online_user->is_in_game_room(_white_id) == false)//白棋用户不在线，返回给黑棋用户的响应
        {
            std::string reason = "对方掉线，获得胜利";
            resp_create(json_resp, reason, _black_id);
            return json_resp;
        }
        else if(_online_user->is_in_game_room(_black_id) == false)//黑棋用户不在线，返回给白棋用户的响应
        {
            std::string reason = "对方掉线，获得胜利";
            resp_create(json_resp, reason, _white_id);
            return json_resp;
        }
        //2. 走棋，判断该位置是否被占用。被占用则返回，未被占用则设置
        if(_board[chess_row][chess_col] != 0)//该位置不为0，说明被占用
        {
            json_resp["optype"] = "put_chess";
            json_resp["result"] = "false";
            json_resp["reason"] = "当前位置已被占用";

            return json_resp;
        }
        int chess_color = cur_id == _white_id ? WHITE_CHESS : BLACK_CHESS;
        _board[chess_row][chess_col] = chess_color;
        //3. 判断走棋后是否有玩家获得胜利。如果有玩家获得胜利
        int winner_id = check_win(chess_row, chess_col, chess_color);
        if(winner_id != 0)//有玩家胜利
        {
            std::string reason = "五星连珠，获得胜利";
            resp_create(json_resp, reason, winner_id);
            return json_resp;
        }

        std::string reason = "对方走棋";
        resp_create(json_resp, reason, 0);
        return json_resp;
    }

    //根据请求处理聊天动作
    Json::Value handle_chat(Json::Value &req)
    {
        Json::Value json_resp = req;
        //1. 检测发送的信息中是否有敏感词
        std::string msg = req["message"].asString();
        int pos = msg.find("垃圾");
        if(pos != std::string::npos)
        {
            json_resp["message"] = "发送信息中包含敏感词";
            return json_resp;
        }
        //2. 将消息返回，交由外部广播
        json_resp["result"] = true;
        // broadcast(json_resp);
        return json_resp;
    }
    //处理房间内的用户退出房间动作
    void exit_room(int uid)
    {
        Json::Value json_resp;
        //1. 下棋中退出，异常退出，判定对方胜利。下棋结束后退出，正常退出
        if(_statu == GAME_START)
        {
            std::string reason = "对方掉线，获得胜利";
            int winner_id = (uid == _white_id ? _black_id : _white_id);
            resp_create(json_resp, reason, winner_id);
            json_resp["optype"] = "put_chess";
            json_resp["room_id"] = _room_id;
            json_resp["uid"] = uid;
            json_resp["row"] = -1;
            json_resp["col"]= -1;
            json_resp["winner"] = winner_id;

            int lose_id = (winner_id == _white_id ? _black_id : _white_id);//获胜方增加分数，掉线方减少分数
            _tb_user->win(winner_id);
            _tb_user->lose(lose_id);
            _statu = GAME_OVER;
            
            broadcast(json_resp);
        }
        //2. 减少用户数量
        --_play_count;
    }
    //将处理结果向房间内的所有用户广播
    void broadcast(Json::Value &rsp)
    {
        //1. 将传入的Json::Value对象序列化
        std::string body;
        json_util::serialize(rsp, body);
        //2. 获取在线用户连接并发送响应
        wsserver_t::connection_ptr wconn = _online_user->get_conn_from_room(_white_id);//获取白棋用户的连接
        if(wconn != nullptr)
            wconn->send(body);//调用websocketpp库中的connection类中的send接口发送，不填第二个参数默认为文本形式发送
        wsserver_t::connection_ptr bconn = _online_user->get_conn_from_room(_black_id);//获取黑棋用户的连接
        if(bconn != nullptr)
            bconn->send(body);
    }
    //总的处理函数，能够根据不同的请求执行不同的动作，并将结果进行广播
    void handle_request(Json::Value &req)
    {
        Json::Value json_resp;
        std::cout << "req: " << req << std::endl;
        //1. 检查房间是否匹配
        if(req["room_id"].asInt() != _room_id)
        {
            json_resp["optype"] = req["optype"].asString();
            json_resp["result"] = false;
            json_resp["reason"] = "房间号不匹配";
            broadcast(json_resp);
            return;
        }
        //2. 根据传入的Json::Value对象执行不同的操作
        if(req["optype"].asString() == "put_chess")//走棋
        {
            json_resp = handle_chess(req);
            //判断是否胜利
            int winner_id = json_resp["winner"].asInt();
            if(winner_id != 0)
            {
                int lose_id = (winner_id == _white_id ? _black_id : _white_id);
                _tb_user->win(winner_id);
                _tb_user->lose(lose_id);
                _statu = GAME_OVER;
            }
        }
        else if(req["optype"].asString() == "chat")//聊天
            json_resp = handle_chat(req);
        else
        {
            json_resp["optype"] = req["optype"].asString();
            json_resp["result"] = "false";
            json_resp["reason"] = "unknown request";
        }
        broadcast(json_resp);
    }
};

using room_ptr = std::shared_ptr<room>;//生成只能指针对象
class room_manager
{
private:
    user_table *_ut_user;//数据管理模块句柄
    online_manager *_online_user;//在线用户管理模块句柄
    static int _next_rid;//房间id
    std::mutex _mutex;//互斥锁
    std::unordered_map<int, room_ptr> _rooms;//房间id与房间信息的映射
    std::unordered_map<int, int> _user;//用户id与房间id的映射

public:
    room_manager(user_table *ut, online_manager *om)
        :_ut_user(ut), _online_user(om)
    {
        DLOG("房间管理模块创建完成!!\n");
    }
    ~room_manager()
    {
        DLOG("房间管理模块即将销毁!!\n");
    }

    //创建房间,创建完成后返回该房间的房间信息的智能指针
    room_ptr create_room(int uid1, int uid2)
    {
        //当有两个用户在游戏大厅匹配成功后，创建房间
        //1. 验证两个用户是否还在游戏大厅
        if(_online_user->is_in_game_hall(uid1) == false)
        {
            DLOG("用户: %d, 当前不在大厅内, 游戏房间创建失败\n", uid1);
            return room_ptr();
        }
        if(_online_user->is_in_game_hall(uid2) == false)
        {
            DLOG("用户: %d, 当前不在大厅内, 游戏房间创建失败\n", uid2);
            return room_ptr();
        }
        //2. 创建房间，并将用户信息添加到房间中
        std::unique_lock<std::mutex> lock(_mutex);//加锁保护
        room_ptr rp(new room(_next_rid, _ut_user, _online_user));
        rp->add_white_user(uid1);
        rp->add_black_user(uid2);
        ++_next_rid;
        //3. 管理房间信息
        _rooms.insert(std::make_pair(_next_rid, rp));
        _user.insert(std::make_pair(uid1, _next_rid));
        _user.insert(std::make_pair(uid2, _next_rid));
        //3. 返回房间信息
        return rp;
    }
    //通过房间id获取房间信息
    room_ptr get_room_by_rid(int rid)
    {
        //1. 加锁保护
        std::unique_lock<std::mutex> lock(_mutex);
        //2. 查找房间信息
        auto it = _rooms.find(rid);
        if(it == _rooms.end())
        {
            ELOG("房间:%d 信息获取失败\n", rid);
            return room_ptr();
        }
        return it->second;
    }
    //通过用户id获取房间信息
    room_ptr get_room_by_uid(int uid)
    {
        //1. 加锁保护
        std::unique_lock<std::mutex> lock(_mutex);
        //2. 通过用户id找到房间id
        auto uit = _user.find(uid);
        if(uit == _user.end())
        {
            ELOG("通过用户id:%d 查找房间id失败\n", uid);
            return room_ptr();
        }
        //3. 通过房间id查找房间信息(不能调用上面的get_room_by_id()，因为这两个接口用了同一把锁加锁，调用会造成死锁)
        auto rit = _rooms.find(uit->second);
        if(rit == _rooms.end())
        {
            ELOG("房间:%d 信息获取失败\n", uit->second);
            return room_ptr();
        }
        return rit->second;
    }
    //通过房间id销毁房间
    void remove_room()
    {
        //创建的房间内的用户信息是用智能指针指向的，因此在离开作用域后自动销毁，不用管
        //1. 通过房间id获取房间信息
        room_ptr rp = get_room_by_rid(_next_rid);
        if(rp.get() == nullptr)
        {
            ELOG("房间:%d 中不存在房间信息\n", _next_rid);
            return;
        }
        //2. 通过房间信息获取用户id
        int uid1 = rp->get_white_user();
        int uid2 = rp->get_black_user();
        //3. 通过用户id，移除用户id与房间id的映射
        std::unique_lock<std::mutex> lock(_mutex);
        _user.erase(uid1);
        _user.erase(uid2);
        //4. 通过房间id，移除房间id与房间信息的映射
        _rooms.erase(_next_rid);
    }
    //删除房间中的指定用户，当房间内没有用户时，销毁房间
    void remove_room_user(int uid)
    {
        //1. 通过用户id获取房间信息
        room_ptr rp = get_room_by_uid(uid);
        //2. 销毁该房间内的特定用户
        rp->exit_room(uid);
        //3. 判断房间内的人数，为0则销毁房间
        if(rp->play_count() == 0)
            remove_room();
    }
};
int room_manager::_next_rid(1);//初始化静态成员变量

#endif