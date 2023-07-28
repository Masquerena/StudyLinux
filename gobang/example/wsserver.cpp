#include <iostream>
#include <string>
#include <websocketpp/server.hpp>
#include <websocketpp/config/asio_no_tls.hpp>

typedef websocketpp::server<websocketpp::config::asio> wsserver_t;
void open_callback(wsserver_t *srv, websocketpp::connection_hdl hdl)//握手成功时被调用
{
    std::cout << "websocket握手成功" << std::endl;
}

void close_callback(wsserver_t *srv, websocketpp::connection_hdl hdl)//连接关闭时被调用
{
    std::cout << "websocket连接断开" << std::endl;
}

void message_callback(wsserver_t *srv, websocketpp::connection_hdl hdl, wsserver_t::message_ptr msg)//websocket消息返回时被调用
{
    wsserver_t::connection_ptr conn = srv->get_con_from_hdl(hdl);//获取websocket通信连接
    std::cout << "wsmsg: " << msg->get_payload() << std::endl;//打印消息

    std::string rsp = "client say: " + msg->get_payload();//设置返回消息
    conn->send(rsp, websocketpp::frame::opcode::text);//以文本方式发送消息
}

void http_callback(wsserver_t *srv, websocketpp::connection_hdl hdl)//http请求时被调用
{
    wsserver_t::connection_ptr conn = srv->get_con_from_hdl(hdl);//获取websocket通信连接

    std::cout << "body: " << conn->get_request_body() << std::endl;//打印此次通信数据的正文
    websocketpp::http::parser::request reg = conn->get_request();//获取http请求对象
    std::cout << "method: " << reg.get_method() << std::endl;//打印http请求方法
    std::cout << "uri: " << reg.get_uri() << std::endl;//打印http请求uri

    std::string body = "<html><body><h1>Hello World</h1></body></html>";//设置网页
    // conn->set_body(body);//设置http响应正文
    // conn->append_header("Content-Type", "text/html");//设置http响应报头字段

    conn->set_body(conn->get_request_body());//将获取到的http请求正文设置为http响应正文
    conn->set_status(websocketpp::http::status_code::ok);

    // wsserver_t::timer_ptr pt = srv->set_timer(10000, message_callback());
    // pt->cancel();
}

int main()
{
    //1. 实例化server对象
    wsserver_t wssrv;
    //2. 设置日志输出等级
    wssrv.set_access_channels(websocketpp::log::alevel::none);
    //3. 初始化asio框架的调度器
    wssrv.init_asio();
    wssrv.set_reuse_addr(true);
    //4. 设置回调函数
    wssrv.set_open_handler(std::bind(open_callback, &wssrv, std::placeholders::_1));//握手成功回调函数
    wssrv.set_close_handler(std::bind(close_callback, &wssrv, std::placeholders::_1));//连接关闭回调函数
    wssrv.set_message_handler(std::bind(message_callback, &wssrv, std::placeholders::_1, std::placeholders::_2));//websocket消息回调函数
    wssrv.set_http_handler(std::bind(http_callback, &wssrv, std::placeholders::_1));//http请求回调函数
    //5. 设置监听端口
    wssrv.listen(8085);
    //6. 获取新连接
    wssrv.start_accept();
    //7. 启动服务器
    wssrv.run();

    return 0; 
}