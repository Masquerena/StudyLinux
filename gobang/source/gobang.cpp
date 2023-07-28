#include "server.hpp"

void logger_test()
{
    // LOG("%s, %d", "hello", 10);
    // LOG("hello");

    ILOG("ILOG");
    DLOG("DLOG");
    ELOG("ELOG");
}

const std::string host = "localhost";//当使用本地登录时，本地换回和localhost都是可行的
const std::string user = "connector";
const std::string password = "123456";
const std::string db = "gobang";
const unsigned int port = 3306;

void mysql_util_test()
{
    MYSQL *ms = mysql_util::mysql_create(host, user, password, db, port);
    const std::string sql = "insert into user values (1, '张三');";
    bool ret = mysql_util::mysql_exec(ms, sql);
    if(ret == false)
        return;
    mysql_util::mysql_destory(ms);
}

void json_util_test()
{
    Json::Value root;
    root["姓名"] = "张三";
    root["年龄"] = 18;
    root["成绩"].append(98);
    root["成绩"].append(80);
    root["成绩"].append(90);

    std::string str;
    json_util::serialize(root, str);
    std::cout << str << std::endl;

    Json::Value test;
    json_util::unserialize(str, test);
    std::cout << "姓名: " << test["姓名"] << std::endl;
    std::cout << "年龄: " << test["年龄"] << std::endl;
    int sz = test["成绩"].size();
    for(int i = 0; i < sz; ++i)
        std::cout << "成绩: " << test["成绩"][i] << std::endl;
}

void string_split_util_test()
{
    std::string str = ".123...456.789.";
    std::vector<std::string> res;
    string_split_util::split(str, ".", res);
    for(const auto &str : res)
        DLOG("%s", str.c_str());
}

void file_read_util_test()
{
    std::string filename = "./makefile";
    std::string body;
    file_read_util::read(filename, body);

    std::cout << body << std::endl;
}

void db_test()
{
    user_table ut(host, user, password, db, port);
    Json::Value user;
    user["username"] = "张三";
    // user["password"] = "123456";
    // ut.insert(user);
    ut.login(user);
    // ut.select_by_name("张三", user);
    // ut.select_by_id(5, user);
    // ut.win(5);
    // ut.lose(5);
    // ut.select_by_name("张三", user);

    // std::cout << "用户名: " << user["username"] << std::endl;
    // std::cout << "id: " << user["id"] << std::endl;
    // std::cout << "分数: " << user["score"] << std::endl;
    // std::cout << "总场次: " << user["total_count"] << std::endl;
    // std::cout << "获胜场次: " << user["win_count"] << std::endl;
}

void online_manager_test()
{
    online_manager om;
    int uid = 2;
    wsserver_t::connection_ptr conn;
    om.enter_game_hall(uid, conn);
    if(om.is_in_game_hall(uid))
        DLOG("enter: in game hall\n");
    else
        DLOG("enter: not int game hall\n");

    om.exit_game_hall(uid);
    if(om.is_in_game_hall(uid))
        DLOG("exit: in game hall\n");
    else
        DLOG("exit: not in game hall\n");
}

void room_test()
{
    user_table ut(host, user, password, db, port);
    online_manager om;
    room rm(10, &ut, &om);
}

void room_manager_test()
{
    user_table ut(host, user, password, db, port);
    online_manager om;
    room_manager rm(&ut, &om);
    room_ptr rp = rm.create_room(10, 20);
}

void matcher_test()
{
    user_table ut(host, user, password, db, port);
    online_manager om;
    room_manager rm(&ut, &om);
    // matcher(&rm, &om, &ut);
}

void server_test()
{
    gobang_server gs(host, user, password, db, port);
    gs.start(8081);
}

int main()
{
    // mysql_util_test();
    // json_util_test();
    // string_split_util_test();
    // file_read_util_test();
    // db_test();
    // online_manager_test();
    // room_test();
    // room_manager_test();
    // matcher_test();
    server_test();
    return 0;
}