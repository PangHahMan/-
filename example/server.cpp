#include <functional>
#include <iostream>
#include <string>
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
using namespace std;

//websocketpp::config::asio是基于Asio框架的配置类。Asio是一个跨平台的网络和底层I/O操作的异步编程框架，提供了高效的异步I/O操作和事件处理机制。因此，通过使用websocketpp::config::asio作为配置类，我们可以基于Asio框架来处理WebSocket服务器的底层网络操作。
typedef websocketpp::server<websocketpp::config::asio> server_t;

//connection_hdl是在WebSocket服务器的回调函数中作为参数传递给我们的代码，用于表示与客户端的连接句柄。
void http_callback(server_t *srv, websocketpp::connection_hdl hd1) {
    //给客户端返回一个hello world的页面
    //这一行代码从连接句柄hd1中获取了一个连接对象，并将其存储在名为conn的指针中。这个连接对象代表了与客户端的连接。
    server_t::connection_ptr conn = srv->get_con_from_hdl(hd1);   //获取连接
    std::cout << "body:" << conn->get_request_body() << std::endl;//打印请求体
    websocketpp::http::parser::request req = conn->get_request(); //获取请求
    //获取客户端请求的HTTP方法的方法调用。
    std::cout << "method:" << req.get_method() << std::endl;      //打印请求方法
    std::cout << "uri:" << req.get_uri() << std::endl;            //打印请求路径

    std::string body = "<html><body><h1>Hello World</h1></body></html>";//给客户端返回
    conn->set_body(body);
    conn->append_header("Content-Type", "text/html");
    conn->set_status(websocketpp::http::status_code::ok);
}

void wsopen_callback(server_t *srv, websocketpp::connection_hdl hd1) {
    std::cout << "websocket握手成功!" << std::endl;
}

void wsclose_callback(server_t *srv, websocketpp::connection_hdl hd1) {
    std::cout << "websocket连接断开" << std::endl;
}

void wsmsg_callback(server_t *srv, websocketpp::connection_hdl hd1, server_t::message_ptr msg) {
    server_t::connection_ptr conn = srv->get_con_from_hdl(hd1);          //获取连接
    std::cout << "websocket收到消息:" << msg->get_payload() << std::endl;//收到消息
    std::string rsp = "client say:" + msg->get_payload();                //给客户端返回
    conn->send(rsp, websocketpp::frame::opcode::text);                   //发送回去
}

int main() {
    //1.实例化server对象
    server_t server;
    //2.设置日志等级
    server.set_access_channels(websocketpp::log::alevel::none);
    //3.初始化asio调度器
    server.init_asio();
    server.set_reuse_addr(true); //设置端口复用，key
    //4.设置回调函数
    //http_callback函数与server对象绑定在一起，并使用占位符placeholders::_1表示将来会传入的参数。
    server.set_http_handler(bind(http_callback, &server, placeholders::_1));  
    server.set_open_handler(bind(wsopen_callback, &server, placeholders::_1));
    server.set_close_handler(bind(wsclose_callback, &server, placeholders::_1));
    server.set_message_handler(bind(wsmsg_callback, &server, placeholders::_1, placeholders::_2));

    //5.设置监听端口
    server.listen(9002);
    //6.开始获取新连接
    server.start_accept();
    //7.启动服务器
    server.run();
    return 0;
}