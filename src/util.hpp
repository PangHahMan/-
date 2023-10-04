#pragma once
#include "logger.hpp"
#include <cassert>
#include <fstream>
#include <jsoncpp/json/json.h>
#include <memory>
#include <mysql/mysql.h>
#include <sstream>
#include <string>
#include <vector>

#include <websocketpp/server.hpp>
#include <websocketpp/config/asio_no_tls.hpp>

/* 
在websocketpp::server<websocketpp::config::asio>中，websocketpp::server是一个模板类，它需要一个模板参数来决定该服务器的配置。websocketpp::config::asio就是这个模板参数，它配置了此服务器使用ASIO库来处理异步I/O。

ASIO(Asynchronous Input/Output)是一个跨平台的C++库，用于编写使用TCP、UDP、串行端口等的网络和低级I/O应用。在这个上下文中，websocketpp::config::asio配置的服务器将使用ASIO来处理网络连接。 
*/
//定义了一个使用ASIO进行网络操作的WebSocket服务器。
typedef websocketpp::server<websocketpp::config::asio> server_t;  

class mysql_util {
public:
    static MYSQL *mysql_create(const std::string &host, const std::string &user, const std::string &password, const std::string &dbname, uint16_t port) {
        //1.初始化mysql句柄
        MYSQL *mysql = mysql_init(NULL);
        if (mysql == NULL) {
            ERR_LOG("mysql_init failed");
            return nullptr;
        }
        //2.连接服务器
        if (mysql_real_connect(mysql, host.c_str(), user.c_str(), password.c_str(), dbname.c_str(), port, NULL, 0) == NULL) {
            ERR_LOG("connect mysql server failed:%s", mysql_error(mysql));
            mysql_close(mysql);
            return nullptr;
        }
        //3.设置客户端字符集,成功返回0
        if (mysql_set_character_set(mysql, "utf8") != 0) {
            ERR_LOG("set client character failed:%s", mysql_error(mysql));
            mysql_close(mysql);
            return nullptr;
        }

        return mysql;
    }

    //mysql执行语句
    static bool mysql_exec(MYSQL *mysql, const std::string &sql) {
        int ret = mysql_query(mysql, sql.c_str());
        if (ret != 0) {
            ERR_LOG("%s\n", sql.c_str());
            ERR_LOG("mysql_query failed:%s", mysql_error(mysql));
            //mysql_close(mysql);
            return false;
        }
        return true;
    }

    //销毁句柄
    static void mysql_destroy(MYSQL *mysql) {
        if (mysql) {
            mysql_close(mysql);
        }
        return;
    }
};

class json_util {
public:
    static bool serialize(const Json::Value &root, std::string &str) {
        Json::StreamWriterBuilder swb;
        std::unique_ptr<Json::StreamWriter> sw(swb.newStreamWriter());
        std::stringstream ss;
        int ret = sw->write(root, &ss);
        if (ret != 0) {
            ERR_LOG("json serialize failed!!");
            return false;
        }
        str = ss.str();
        return true;
    }

    static bool unserialize(const std::string &str, Json::Value &root) {
        Json::CharReaderBuilder crb;
        std::unique_ptr<Json::CharReader> cr(crb.newCharReader());
        std::string err;
        bool ret = cr->parse(str.c_str(), str.c_str() + str.size(), &root, &err);
        if (ret == false) {
            ERR_LOG("json unserialize failed: %s", err.c_str());
            return false;
        }
        return true;
    }
};

class string_util {
public:
    static int split(const std::string &str, const std::string &sep, std::vector<std::string> &vec) {
        //123,234,,,345
        size_t pos, index = 0;
        while (index < str.size()) {
            //从index位置开始查找
            pos = str.find(sep, index);
            if (pos == std::string::npos) {
                //没有找到,字符串中没有间隔字符了，则跳出循环
                //从index开始剩余的字符串插入到vec中
                vec.push_back(str.substr(index));
                break;
            }

            //如果找到了,但是pos和index位置相同,说明sep在第一个位置,那么跳过sep.size()步
            if (pos == index) {
                index += sep.size();
                continue;
            }

            vec.push_back(str.substr(index, pos - index));
            index = pos + sep.size();
        }
        return str.size();
    }
};

class file_util {
public:
    static bool read(const std::string &filename, std::string &body) {
        //打开文件
        std::ifstream ifs(filename, std::ios::binary);
        if (ifs.is_open() == false) {
            ERR_LOG("%s file open failed!!", filename.c_str());
            return false;
        }
        //获取文件大小
        size_t fsize = 0;
        ifs.seekg(0, std::ios::end);
        fsize = ifs.tellg();
        ifs.seekg(0, std::ios::beg);
        body.resize(fsize);
        //将文件所有数据读取出来
        ifs.read(&body[0], fsize);
        if (ifs.good() == false) {
            ERR_LOG("read %s file content failed!", filename.c_str());
            ifs.close();
            return false;
        }
        //关闭文件
        ifs.close();
        return true;
    }
};