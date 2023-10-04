#pragma once
#include "db.hpp"
#include "logger.hpp"
#include "matcher.hpp"
#include "online.hpp"
#include "room.hpp"
#include "session.hpp"
#include "util.hpp"
#include <functional>
#include <string>
#include <vector>
#define WWWROOT "./wwwroot/"

class server {
private:
    std::string _web_root;//静态资源根目录 ./wwwroot/    /register.html ->  ./wwwroot/register.html
    server_t _server;
    user_table _ut;
    online_manager _om;
    room_manager _rm;
    matcher _mm;
    session_manager _sm;

private:
    //静态资源请求的处理
    void file_handler(server_t::connection_ptr &conn) {
        //1.获取到请求uri-资源路径，了解客户端请求的页面文件名称
        websocketpp::http::parser::request req = conn->get_request();
        std::string uri = req.get_uri();
        //2.组合出文件的实际路径  相对根目录 + uri
        std::string realpath = _web_root + uri;
        //3.如果请求的是个目录，增加一个后缀  login.html,    /  ->  /login.html
        if (realpath.back() == '/') {
            realpath += "login.html";
        }
        //4.读取文件内容
        std::string body;
        bool ret = file_util::read(realpath, body);
        //4.1文件不存在，读取文件内容失败，返回404
        if (!ret) {
            body += "<html>";
            body += "<head>";
            body += "<meta charset = 'UTF-8/>";
            body += "</head>";
            body += "<body>";
            body += "<h1> Not Found </h1>";
            body += "</body>";
            conn->set_status(websocketpp::http::status_code::not_found);
            conn->set_body(body);
            return;
        }

        //5.设置响应文件
        conn->set_body(body);
        conn->set_status(websocketpp::http::status_code::ok);
    }

    void http_resp(server_t::connection_ptr &conn, bool result,
                   websocketpp::http::status_code::value code, const std::string &reason) {
        Json::Value resp_json;
        resp_json["result"] = result;
        resp_json["reason"] = reason;
        std::string resp_body;
        json_util::serialize(resp_json, resp_body);
        conn->set_status(code);
        conn->set_body(resp_body);
        conn->append_header("Content-Type", "application/json");

        //设置HTTP响应的状态码、正文和头部。一旦这些设置完成，WebSocket++库就会自动将这个HTTP响应发送给客户端。
        return;
    }

    //用户注册功能请求的处理
    void reg(server_t::connection_ptr &conn) {
        //1.获取到请求正文
        std::string req_body = conn->get_request_body();
        //2.对正文进行json反序列化，得到用户名和密码
        Json::Value login_info;
        bool ret = json_util::unserialize(req_body, login_info);
        if (!ret) {
            DBG_LOG("反序列化注册信息失败");
            return http_resp(conn, false, websocketpp::http::status_code::bad_request, "请求的正文格式错误");
        }
        //3.进行数据库的用户新增操作
        if (login_info["username"].isNull() || login_info["password"].isNull()) {
            DBG_LOG("用户名密码不完整");
            return http_resp(conn, false, websocketpp::http::status_code::bad_request, "请输入用户名/密码");
        }

        ret = _ut.insert(login_info);
        if (!ret) {
            DBG_LOG("向数据库插入数据失败");
            return http_resp(conn, false, websocketpp::http::status_code::bad_request, "用户名已经被占用");
        }
        //如果成功了，则返回200
        return http_resp(conn, true, websocketpp::http::status_code::ok, "用户注册成功");
    }

    //用户登录功能请求的处理
    void login(server_t::connection_ptr &conn) {
        //1.获取请求正文，并进行json反序列化，得到用户名和密码
        std::string req_body = conn->get_request_body();
        Json::Value login_info;
        bool ret = json_util::unserialize(req_body, login_info);
        if (!ret) {
            DBG_LOG("反序列化登录信息失败");
            return http_resp(conn, false, websocketpp::http::status_code::bad_request, "请求的正文格式错误");
        }
        //2.检验正文完整性，进行数据库的用户信息验证
        if (login_info["username"].isNull() || login_info["password"].isNull()) {
            DBG_LOG("用户名或者密码不完整");
            return http_resp(conn, false, websocketpp::http::status_code::bad_request, "请输入用户名/密码");
        }
        ret = _ut.login(login_info);
        if (!ret) {
            //  1. 如果验证失败，则返回400
            DBG_LOG("用户名或者密码错误");
            return http_resp(conn, false, websocketpp::http::status_code::bad_request, "用户名或者密码错误");
        }

        //3. 如果验证成功，给客户端创建session
        uint64_t uid = login_info["id"].asUInt64();
        session_ptr ssp = _sm.create_sesson(uid, LOGIN);
        if (ssp.get() == nullptr) {
            DBG_LOG("创建会话失败");
            return http_resp(conn, false, websocketpp::http::status_code::internal_server_error, "创建会话失败");
        }
        _sm.set_session_expire_time(ssp->get_ssid(), SESSION_TIMEOUT);
        //4. 设置响应头部：Set-Cookie,将sessionid通过cookie返回
        std::cout << "ssid:" << ssp->get_ssid() << std::endl;
        std::string cookie_ssid = "SSID=" + std::to_string(ssp->get_ssid());
        conn->append_header("Set-Cookie", cookie_ssid);
        std::cout << "Set-Cookie:" << cookie_ssid << std::endl;
        return http_resp(conn, true, websocketpp::http::status_code::ok, "登录成功");
    }

    //获取cookie的值
    bool get_cookie_val(const std::string &cookie_str, const std::string &key, std::string &val) {
        // Cookie: SSID=XXX; path=/;
        //1. 以 ; 作为间隔，对字符串进行分割，得到各个单个的cookie信息
        std::string sep = "; ";
        std::vector<std::string> cookie_arr;
        string_util::split(cookie_str, sep, cookie_arr);
        for (auto str: cookie_arr) {
            //2. 对单个cookie字符串，以 = 为间隔进行分割，得到key和val
            std::vector<std::string> tmp_arr;
            string_util::split(str, "=", tmp_arr);
            if (tmp_arr.size() != 2) { continue; }
            if (tmp_arr[0] == key) {
                val = tmp_arr[1];
                return true;
            }
        }
        return false;
    }

    //用户信息获取功能请求的处理
    void info(server_t::connection_ptr &conn) {
        // 1. 获取请求信息中的Cookie，从Cookie中获取ssid
        std::string cookie_str = conn->get_request_header("Cookie");
        if (cookie_str.empty()) {
            //如果没有cookie，返回错误：没有cookie信息，让客户端重新登录
            return http_resp(conn, false, websocketpp::http::status_code::bad_request, "找不到cookie信息，请重新登录");
        }

        //1.5. 从cookie中取出ssid
        std::string ssid_str;
        bool ret = get_cookie_val(cookie_str, "SSID", ssid_str);
        if (!ret) {
            //cookie中没有ssid，返回错误：没有ssid信息，让客户端重新登录
            return http_resp(conn, false, websocketpp::http::status_code::bad_request, "找不到ssid信息，请重新登录");
        }

        std::cout << "ssid_str:" << ssid_str << std::endl;
        // 2.在session管理中查找对应的会话信息
        session_ptr ssp = _sm.get_sesson((uint64_t) (std::stol(ssid_str)));
        if (ssp.get() == nullptr) {
            //没有找到session，则认为登录已经过期，需要重新登录
            return http_resp(conn, false, websocketpp::http::status_code::bad_request, "登录过期，请重新登录");
        }

        //3.从数据库中取出用户信息
        uint64_t uid = ssp->get_user();
        Json::Value user_info;
        ret = _ut.select_by_id(uid, user_info);
        if (!ret) {
            //获取用户信息失败，返回错误：找不到用户信息
            return http_resp(conn, false, websocketpp::http::status_code::bad_request, "找不到用户信息，请重新登录");
        }

        //4.进行序列化发送给客户端
        std::string body;
        json_util::serialize(user_info, body);
        conn->set_body(body);
        conn->append_header("Content-Type", "application/json");
        conn->set_status(websocketpp::http::status_code::ok);

        //5.用户操作后，需要刷新session的过期时间
        _sm.set_session_expire_time(ssp->get_ssid(), SESSION_TIMEOUT);
    }

    //
    void http_callback(websocketpp::connection_hdl hd1) {
        //使用连接句柄 hd1 获取相应的连接对象。这个连接对象包含了关于当前连接（也即当前的 HTTP 请求）的所有信息，例如请求的方法（GET, POST等），请求的 URI，请求的头部和体部内容等。
        server_t::connection_ptr conn = _server.get_con_from_hdl(hd1);
        websocketpp::http::parser::request req = conn->get_request();
        std::string method = req.get_method();
        std::string uri = req.get_uri();
        if (method == "POST" && uri == "/reg") {
            return reg(conn);
        } else if (method == "POST" && uri == "/login") {
            return login(conn);
        } else if (method == "GET" && uri == "/info") {
            return info(conn);
        } else {
            return file_handler(conn);
        }
    }

    void wsopen_callback(websocketpp::connection_hdl hd1) {
    }

    void wsclose_callback(websocketpp::connection_hdl hd1) {
    }

    void wsmsg_callback(websocketpp::connection_hdl hd1, server_t::message_ptr msg) {
    }

public:
    //进行成员初始化，以及服务器回调函数的设置
    server(const std::string &host, const std::string &username, const std::string &password, const std::string &dbname, uint16_t port = 3306, const std::string &wwwroot = WWWROOT)
        : _web_root(wwwroot),
          _ut(host, username, password, dbname, port),
          _om(),
          _rm(&_ut, &_om),
          _sm(&_server),
          _mm(&_rm, &_ut, &_om) {
        _server.set_access_channels(websocketpp::log::alevel::none);
        _server.init_asio();
        _server.set_reuse_addr(true);
        //当HTTP请求到来时，WebSocket++库将自动调用这个处理函数(http_callback)，并自动传入一个websocketpp::connection_hdl参数给占位符-1
        _server.set_http_handler(std::bind(&server::http_callback, this, std::placeholders::_1));
        _server.set_open_handler(std::bind(&server::wsopen_callback, this, std::placeholders::_1));
        _server.set_close_handler(std::bind(&server::wsclose_callback, this, std::placeholders::_1));
        _server.set_message_handler(std::bind(&server::wsmsg_callback, this, std::placeholders::_1, std::placeholders::_2));
    }

    //启动服务器
    void start(int port) {
        _server.listen(port);
        _server.start_accept();
        _server.run();
    }
};