#ifndef __M_SERVER_H__
#define __M_SERVER_H__

#include "util.hpp"
#include "db.hpp"
#include "matcher.hpp"
#include "online.hpp"
#include "room.hpp"
#include "session.hpp"

typedef websocketpp::server<websocketpp::config::asio> wsserver_t;
#define WWWROOT "./wwwroot"
class gobang_server
{
private:
    std::string _web_root;//静态资源根目录
    user_table _ut;//数据管理模块句柄
    online_manager _om;//在线用户管理模块句柄
    room_manager _rm;//游戏房间管理模块句柄
    session_manager _sm;//session管理模块句柄
    matcher _mm;//用户匹配模块句柄
    wsserver_t _wssrv;//websocketpp中的server类对象

private:
//静态资源请求处理
void file_hanlder(wsserver_t::connection_ptr &conn)
{
    //1. 获取http请求，拿到要请求的uri资源路径
    websocketpp::http::parser::request req = conn->get_request();
    std::string uri = req.get_uri();
    //2. 将uri与web根目录组合出对应的路径
    std::string pathname = WWWROOT + uri;
    //3. 如果路径的最后一个字符为"/"，则自动跳转登录页面
    if(pathname.back() == '/')
        pathname += "login.html";
    //4. 读取文件内容，如果该文件不存在，则返回一个404页面
    std::string body;
    bool ret = file_read_util::read(pathname, body);
    if(ret == false)
    {
        std::string path = "/404.html";
        pathname = WWWROOT + path;
        body.clear();
        file_read_util::read(pathname, body);
        conn->set_status(websocketpp::http::status_code::not_found);
    }
    else
        conn->set_status(websocketpp::http::status_code::ok);
    //5. 设置http响应正文
    conn->set_body(body);
}

void http_resp(bool result, const std::string &reason, 
               websocketpp::http::status_code::value code, wsserver_t::connection_ptr &conn)//设置http响应
{
    //形成新的json格式的响应
    Json::Value json_resp;
    json_resp["result"] = result;
    json_resp["reason"] = reason;
    //序列化
    std::string body;
    json_util::serialize(json_resp, body);
    //设置http响应信息
    conn->set_status(code);
    conn->set_body(body);
    conn->append_header("Content-Type", "application/json");//声明正文采用的是json格式的字符串
}

//用户注册功能请求处理
void reg(wsserver_t::connection_ptr &conn)
{
    //1. 获取http请求正文，拿到用户名和密码
    std::string req_body = conn->get_request_body();
    //2. 反序列化，得到用户名和密码的Json::Value格式的字符串
    Json::Value reg_info;
    bool ret = json_util::unserialize(req_body, reg_info);
    if(ret == false)//反序列化失败
    {
        DLOG("反序列失败\n");
        std::string reason = "请求的正文格式错误";
        http_resp(false, reason, websocketpp::http::status_code::bad_request, conn);
        return;
    }
    //3. 向数据库中新增用户
    if(reg_info["username"].isNull() || reg_info["password"].isNull())//有一个数据不存在，则返回
    {
        DLOG("用户名或密码不完整\n");
        std::string reason = "请输入用户名或密码";
        http_resp(false, reason, websocketpp::http::status_code::bad_request, conn);
        return;
    }
    ret = _ut.insert(reg_info);
    if(ret == false)//插入数据失败
    {
        DLOG("新增用户失败\n");
        std::string reason = "用户名已被占用";
        http_resp(false, reason, websocketpp::http::status_code::bad_request, conn);
        return;
    }
    std::string reason = "用户注册成功";
    http_resp(true, reason, websocketpp::http::status_code::ok, conn);
}
//用户登录功能请求处理
void login(wsserver_t::connection_ptr &conn)
{
    //1. 获取hhtp响应正文，得到字符串格式的用户名和密码
    std::string req_body = conn->get_request_body();
    //2. 反序列化，得到json格式的用户名和密码
    Json::Value log_info;
    bool ret = json_util::unserialize(req_body, log_info);
    if(ret == false)
    {
        DLOG("用户登录信息反序列化失败\n");
        std::string reason = "请求的正文格式错误";
        http_resp(false, reason, websocketpp::http::status_code::bad_request, conn);
        return;
    }
    //3. 到数据库中验证该用户是否存在
    if(log_info["username"].isNull() || log_info["password"].isNull())
    {
        DLOG("登录信息的用户名或密码不完整\n");
        std::string reason = "请输入用户名或密码";
        http_resp(false, reason, websocketpp::http::status_code::bad_request, conn);
        return;
    }
    ret = _ut.login(log_info);
    if(ret == false)
    {
        DLOG("用户名或密码错误\n");
        std::string reason = "登录失败，用户名或密码错误";
        http_resp(false, reason, websocketpp::http::status_code::bad_request, conn);
        return;
    }
    //4. 为该用户创建session
    int uid = log_info["id"].asInt();//用户id在登录验证时已经被填入了log_info中
    session_ptr sp = _sm.create_session(uid, LOGIN);
    if(sp.get() == nullptr)
    {
        DLOG("session创建失败\n");
        std::string reason = "session创建失败";
        http_resp(false, reason, websocketpp::http::status_code::internal_server_error, conn);
        return;
    }
    //5. 为session设置生命周期
    _sm.set_session_expire_time(sp->get_ssid(), SESSION_TIMEOUT);
    //6. 设置响应头部中的Cookie字段
    std::string cookie_ssid = ("SSID=" + std::to_string(sp->get_ssid()));
    conn->append_header("Set-Cookie", cookie_ssid);
    std::string reason = "登录成功";
    http_resp(true, reason, websocketpp::http::status_code::ok, conn);
}

bool get_cookie_val(const std::string &cookie_str, const std::string &key, std::string &val)//获取Cookie中的key的值
{
    //http请求中的Cookie字段格式：Cookie: name=value; name2=value2; name3=value3
    //1. 根据请求格式，按照“; ”为分隔符，取出里面的name和value的组合
    std::string sep = "; ";
    std::vector<std::string> res_arr;
    string_split_util::split(cookie_str, sep, res_arr);
    //2. 取出name=value后，按照“=”为分隔符，得到每个值的name，与key相比对，相等则取走value后退出
    sep = "=";
    for(const auto &str : res_arr)
    {
        std::vector<std::string> tmp_arr;
        string_split_util::split(str, sep, tmp_arr);
        if(tmp_arr.size() != 2) 
            continue;
        if(tmp_arr[0] == key)//和key相等，说明找到了Cookie中要求的值
        {
            val = tmp_arr[1];
            return true;
        }
    }
    return false;
}

//用户信息获取功能请求处理
void info(wsserver_t::connection_ptr &conn)
{
    //1. 获取http请求中的Cookie字段
    std::string cookie_str = conn->get_request_header("Cookie");
    if(cookie_str.empty())
    {
        //没有获取到cookie信息，返回错误信息
        DLOG("cookie信息获取失败\n");
        std::string reason = "cookie信息获取失败";
        http_resp(false, reason, websocketpp::http::status_code::bad_request, conn);
        return;
    }
    //2. 从Cookie字段中获取SSID
    std::string ssid_str;
    bool ret = get_cookie_val(cookie_str, "SSID", ssid_str);
    if(ret == false)
    {
        //Cookie中不存在SSID
        DLOG("Cookie中无法找到SSID\n");
        std::string reason = "Cookie中无法找到SSID";
        http_resp(false, reason, websocketpp::http::status_code::bad_request, conn);
        return;
    }
    //3. 在session管理中查找对应的会话信息
    int ssid = std::stoi(ssid_str);
    session_ptr sp = _sm.get_session_by_ssid(ssid);
    if(sp.get() == nullptr)
    {
        //如果sp为空，说明没有找到对应的会话信息
        DLOG("当前登录信息过期，请重新登录\n");
        std::string reason = "当前登录信息过期，请求重新登录";
        http_resp(false, reason, websocketpp::http::status_code::bad_request, conn);
        return;
    }
    //4. 根据会话信息到数据库中查找用户信息
    int uid = sp->get_user();
    Json::Value user;
    ret = _ut.select_by_id(uid, user);
    if(ret == false)
    {
        //查找失败，说明该用户不存在
        DLOG("无法找到该用户的信息，用户不存在\n");
        std::string reason("无法找到该用户的信息，用户不存在");
        http_resp(false, reason, websocketpp::http::status_code::bad_request, conn);
        return;
    }
    //5. 设置正文和报头
    std::string body;
    json_util::serialize(user, body);
    conn->set_body(body);
    conn->append_header("Content-Type", "application/json");
    conn->set_status(websocketpp::http::status_code::ok);
    //6. 刷新session过期时间
    _sm.set_session_expire_time(ssid, SESSION_TIMEOUT);
}

private:
//http请求时被调用
void http_callback(websocketpp::connection_hdl hdl)
{
    //1. 获取通信连接
    wsserver_t::connection_ptr conn = _wssrv.get_con_from_hdl(hdl);//获取通信连接
    //2. 获取http请求对象
    websocketpp::http::parser::request req = conn->get_request();
    //3. 获取请求方法和uri
    std::string method = req.get_method();
    std::string uri = req.get_uri();
    //4. 根据请求方法和uri进行不同的请求处理
    if(method == "POST" && uri == "/reg")
        return reg(conn);
    else if(method == "POST" && uri == "/login")
        return login(conn);
    else if(method == "GET" && uri == "/info")
        return info(conn);
    else
        return file_hanlder(conn);
}

void wsopen_resp(const std::string reason, wsserver_t::connection_ptr &conn)//游戏大厅长连接的错误响应
{
    Json::Value err_resp;
    err_resp["optype"] = "hall_ready";
    err_resp["result"] = false;
    err_resp["reason"] = reason;
    std::string body;
    json_util::serialize(err_resp, body);
    conn->send(body);
}

session_ptr get_session_by_cookie(wsserver_t::connection_ptr &conn)//通过session获取cookie信息
{
    //1. 验证当前用户是否登录
    //1.1 获取http请求中的Cookie字段
    std::string cookie_str = conn->get_request_header("Cookie");
    if(cookie_str.empty())
    {
        //没有获取到cookie信息，返回错误信息
        std::string reason = "没有找到cookie信息，需要重新登录";
        wsopen_resp(reason, conn);
        return session_ptr();
    }
    //1.2 从Cookie字段中获取SSID
    std::string ssid_str;
    bool ret = get_cookie_val(cookie_str, "SSID", ssid_str);
    if(ret == false)
    {
        //Cookie中不存在SSID
        std::string reason = "没有找到ssid信息，需要重新登录";
        wsopen_resp(reason, conn);
        return session_ptr();
    }
    //1.3 在session管理中查找对应的会话信息
    int ssid = std::stoi(ssid_str);
    session_ptr sp = _sm.get_session_by_ssid(ssid);
    if(sp.get() == nullptr)
    {
        //如果sp为空，说明没有找到对应的会话信息
        std::string reason = "没有找到session信息，需要重新登录";
        wsopen_resp(reason, conn);
        return session_ptr();
    }

    return sp;
}

void wsopen_game_hall(wsserver_t::connection_ptr &conn)//游戏大厅建立了长连接后的处理
{
    //1. 获取session信息
    session_ptr sp = get_session_by_cookie(conn);
    if(sp.get() == nullptr)
        return;
    //2. 判断当前用户是否重复登录，即是否已经存在游戏大厅或游戏房间
    if(_om.is_in_game_hall(sp->get_user()) || _om.is_in_game_room(sp->get_user()))
    {
        std::string reason = "玩家重复登录";
        wsopen_resp(reason, conn);
        return;
    }
    //3. 将当前用户加入游戏大厅
    _om.enter_game_hall(sp->get_user(), conn);
    //4. 给客户端返回一个游戏大厅连接成功的响应
    Json::Value resp_json;
    resp_json["optype"] = "hall_ready";
    resp_json["reasult"] = true;
    std::string body;
    json_util::serialize(resp_json, body);
    conn->send(body);
    //5. 将session设置为永久存在
    _sm.set_session_expire_time(sp->get_ssid(), SESSION_FOREVER);
}

void wsopen_room_resp(std::string &reason, wsserver_t::connection_ptr &conn)//打开游戏房间时的错误响应
{
    Json::Value resp_json;
    std::string resp_body;
    resp_json["optype"] = "room_ready";
    resp_json["result"] = false;
    resp_json["reason"] = reason;
    json_util::serialize(resp_json, resp_body);
    conn->send(resp_body);
}

void wsopen_game_room(wsserver_t::connection_ptr &conn)//握手成功进入游戏房间的请求处理
{
    //1. 获取当前用户的session信息
    session_ptr sp = get_session_by_cookie(conn);
    if(sp.get() == nullptr)
        return;
    //2. 判断当前用户是否已经在在线用户管理模块的游戏大厅或游戏房间中
    int uid = sp->get_user();
    if(_om.is_in_game_hall(uid) || _om.is_in_game_room(uid))
    {   
        std::string reason = "玩家已存在与游戏大厅或游戏房间中，重复登录";
        wsopen_room_resp(reason, conn);
        return;
    }
    //3. 判断在匹配成功后是否创建了游戏房间
    room_ptr rp = _rm.get_room_by_uid(uid);
    if(rp.get() == nullptr)
    {
        std::string reason = "未找到对应的房间信息";
        wsopen_room_resp(reason, conn);
        return;
    }
    //4. 将用户加入在线用户管理模块的游戏房间中
    _om.enter_game_room(uid, conn);
    //5. 将session设置为永久存在
    _sm.set_session_expire_time(sp->get_ssid(), SESSION_FOREVER);
    //6. 返回响应
    Json::Value resp_json;
    std::string resp_body;
    resp_json["optype"] = "room_ready";
    resp_json["result"] = true;
    resp_json["room_id"] = rp->id();
    resp_json["self_id"] = uid;
    resp_json["white_id"] = rp->get_white_user();
    resp_json["black_id"] = rp->get_black_user();
    json_util::serialize(resp_json, resp_body);
    conn->send(resp_body);
    return;
}

//websocket握手成功时被调用
void wsopen_callback(websocketpp::connection_hdl hdl)
{
    //1. 获取通信连接
    wsserver_t::connection_ptr conn = _wssrv.get_con_from_hdl(hdl);//获取通信连接
    //2. 获取http请求对象
    websocketpp::http::parser::request req = conn->get_request();
    //2. 获取uri，通过url判断此次的websocket长连接是游戏房间中建立还是游戏大厅中建立
    std::string uri = req.get_uri();
    if(uri == "/hall")//建立了游戏大厅长连接
    {
        wsopen_game_hall(conn);
        return;
    }
    else if(uri == "/room")//建立了游戏房间长连接
    {
        wsopen_game_room(conn);
        return;
    }
}

void wsclose_game_hall(wsserver_t::connection_ptr &conn)//关闭游戏大厅长连接的处理
{
    //1. 获取session信息
    session_ptr sp = get_session_by_cookie(conn);
    if(sp.get() == nullptr)
        return;
    //2. 判断当前用户是否在游戏大厅中
    if(_om.is_in_game_hall(sp->get_user()) == false)
    {
        std::string reason = "该用户当前并不在游戏大厅中";
        wsopen_resp(reason, conn);
        return;
    }
    //3. 将用户从游戏大厅中移除
    _om.exit_game_hall(sp->get_user());
    //4. 恢复session的生命周期，设置定时任务
    _sm.set_session_expire_time(sp->get_ssid(), SESSION_TIMEOUT);
}

void wsclose_game_room(wsserver_t::connection_ptr &conn)//游戏房间关闭时的请求处理
{
    //1. 获取session信息
    session_ptr sp = get_session_by_cookie(conn);
    if(sp.get() == nullptr)
        return;
    //2. 将玩家从在线用户管理中移除
    _om.exit_game_room(sp->get_user());
    //3. 恢复session的生命周期，重置定时任务
    _sm.set_session_expire_time(sp->get_ssid(), SESSION_TIMEOUT);
    //4. 将用户从游戏房间中移除，如果游戏房间中没有用户，则销毁房间
    _rm.remove_room_user(sp->get_user());
}

//连接关闭时被调用
void close_callback(websocketpp::connection_hdl hdl)
{
    //1. 获取通信连接
    wsserver_t::connection_ptr conn = _wssrv.get_con_from_hdl(hdl);//获取通信连接
    //2. 获取http请求对象
    websocketpp::http::parser::request req = conn->get_request();
    //3. 通过http请求对象获取uri
    std::string uri = req.get_uri();
    //4. 通过rui判断此次是建立了websocket长连接还是关闭了websocket长连接
    if(uri == "/hall")
    {
        wsclose_game_hall(conn);
        return;
    }
    else if(uri == "/room")
    {
        wsclose_game_room(conn);
        return;
    }
}

void wsmessage_game_hall(wsserver_t::connection_ptr &conn, wsserver_t::message_ptr &msg)//对客户端发来的请求信息做处理
{
    Json::Value resp_json;
    std::string resp_body;
    //1. 获取session信息
    session_ptr sp = get_session_by_cookie(conn);
    if(sp.get() == nullptr)
        return;
    //2. 获取请求信息
    std::string req_body = msg->get_payload();
    Json::Value req_json;
    bool ret = json_util::unserialize(req_body, req_json);
    if(ret == false)
    {
        resp_json["result"] = false;
        resp_json["reason"] = "请求信息反序列化失败";
        json_util::serialize(resp_json, resp_body);
        conn->send(resp_body);
        return;
    }
    //3. 处理请求：开始对战匹配和停止对战匹配
    if(!req_json["optype"].isNull() && req_json["optype"].asString() == "match_start")
    {
        //开始对战匹配请求处理
        //1. 加入匹配队列
        _mm.add(sp->get_user());
        //2. 返回响应
        resp_json["optype"] = "match_start";
        resp_json["result"] = true;
        json_util::serialize(resp_json, resp_body);
        conn->send(resp_body);
        return;
    }
    else if(!req_json["optype"].isNull() && req_json["optype"].asString() == "match_stop")
    {
        //停止对战匹配请求处理
        //1. 将用户从匹配队列中清除
        _mm.del(sp->get_user());
        //2. 返回响应
        resp_json["optype"] = "match_stop";
        resp_json["result"] = true;
        json_util::serialize(resp_json, resp_body);
        conn->send(resp_body);
        return;
    }
    resp_json["optype"] = "unknown";
    resp_json["result"] = false;
    json_util::serialize(resp_json, resp_body);
    conn->send(resp_body); 
}

void wsmessage_game_room(wsserver_t::connection_ptr &conn, wsserver_t::message_ptr &msg)//房间中执行的操作的处理
{
    //1. 获取session信息
    session_ptr sp = get_session_by_cookie(conn);
    if(sp.get() == nullptr)
        return;
    //2. 获取房间信息
    Json::Value resp_json;
    std::string resp_body;
    room_ptr rp = _rm.get_room_by_uid(sp->get_user());
    if(rp.get() == nullptr)
    {
        resp_json["optype"] = "unknown";
        resp_json["reault"] = false;
        resp_json["reason"] = "没有找到对应玩家的房间信息";
        json_util::serialize(resp_json, resp_body);
        conn->send(resp_body);
        return;
    }
    //3. 获取此次请求的正文，确定要执行的操作
    std::string req_body = msg->get_payload();
    //4. 对获取的正文做反序列化
    Json::Value req_json;
    bool ret = json_util::unserialize(req_body, req_json);
    if(ret == false)
    {
        resp_json["optype"] = "unknown";
        resp_json["reault"] = false;
        resp_json["reason"] = "请求反序列化失败";
        json_util::serialize(resp_json, resp_body);
        conn->send(resp_body);
        return;
    }
    //5. 执行对应的操作
    DLOG("req_body: %s\n", req_body.c_str());
    std::cout << "req_json: " << req_json << std::endl;
    rp->handle_request(req_json);
}
//websocket消息返回时被调用
void message_callback(websocketpp::connection_hdl hdl, wsserver_t::message_ptr msg)
{
    //1. 获取通信连接
    wsserver_t::connection_ptr conn = _wssrv.get_con_from_hdl(hdl);//获取通信连接
    //2. 获取http请求对象
    websocketpp::http::parser::request req = conn->get_request();
    //3. 通过http请求对象获取uri
    std::string uri = req.get_uri();
    //4. 通过uri判断此次是要处理游戏大厅的长连接信息还是游戏房间的长连接信息
    if(uri == "/hall")
    {
        wsmessage_game_hall(conn, msg);
        return;
    }
    else if(uri == "/room")
    {
        wsmessage_game_room(conn, msg);
        return;
    }
}

public:
    gobang_server(const std::string &host, const std::string &username, const std::string &password,
                  const std::string &db, const int &port = 3306, const std::string &wwwroot = WWWROOT)
        :_web_root(wwwroot), _ut(host, username, password, db, port), 
         _rm(&_ut, &_om), _sm(&_wssrv), _mm(&_rm, &_om, &_ut)
    {
        //1. 实例化sever类对象(已在成员变量中完成)
        //2. 设置日志输出等级
        _wssrv.set_access_channels(websocketpp::log::alevel::none);
        //3. 初始化asio框架的调度器
        _wssrv.init_asio();
        _wssrv.set_reuse_addr(true);//设置地址重用
        //4. 设置回调函数
        _wssrv.set_http_handler(std::bind(&gobang_server::http_callback, this, std::placeholders::_1));//http请求回调函数
        _wssrv.set_open_handler(std::bind(&gobang_server::wsopen_callback, this, std::placeholders::_1));//握手成功回调函数
        _wssrv.set_close_handler(std::bind(&gobang_server::close_callback, this, std::placeholders::_1));//连接关闭回调函数
        _wssrv.set_message_handler(std::bind(&gobang_server::message_callback, this, std::placeholders::_1, std::placeholders::_2));//websocket消息回调函数
    }
    ~gobang_server()
    {}

    void start(int port)
    {
        //5. 设置监听端口
        _wssrv.listen(port);
        //6. 获取新连接
        _wssrv.start_accept();
        //7. 启动服务器
        _wssrv.run();
    }
};

#endif