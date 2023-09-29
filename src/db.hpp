#pragma once
#include "util.hpp"
#include <algorithm>
#include <mutex>
#define INSERT_USER "insert user values(null,'%s',password('%s'),1000,0,0);"
#define LOGIN_USER "select id,score,total_count,win_count from user where username='%s' and password=password('%s');"
#define USER_BY_NAME "select id,score,total_count,win_count from user where username='%s';"
#define USER_BY_ID "select username, score, total_count, win_count from user where id=%d;"
#define USER_WIN "update user set score=score+30, total_count=total_count+1, win_count=win_count+1 where id=%d;"
#define USER_LOSE "update user set score=score-30, total_count=total_count+1 where id=%d;"
class user_table {
public:
    user_table(const std::string &host, const std::string &username, const std::string &password, const std::string &dbname, uint16_t port = 3306) {
        // 使用mysql_util命名空间中的mysql_create函数创建MySQL连接
        _mysql = mysql_util::mysql_create(host, username, password, dbname, port);
        // 检查连接是否成功创建
        assert(_mysql);
    }

    // 用户表对象的析构函数，用于释放与MySQL连接相关的资源
    ~user_table() {
        // 调用mysql_util命名空间中的mysql_destroy函数释放MySQL连接
        mysql_util::mysql_destroy(_mysql);
        // 将_mysql指针置为nullptr，防止出现悬挂指针
        _mysql = nullptr;
    }

    //注册时新增用户
    bool insert(Json::Value &user) {
        // 构造插入数据库的SQL语句
        char sql[4096] = {0};
        //数据校验，用户名和密码不能为空
        if (user["password"].isNull() || user["username"].isNull()) {
            DBG_LOG("INPUT PASSWORD OR USERNAME");
            return false;
        }
        sprintf(sql, INSERT_USER, user["username"].asCString(), user["password"].asCString());

        // 执行SQL语句，插入新的用户
        bool ret = mysql_util::mysql_exec(_mysql, sql);
        // 如果插入失败，打印错误日志并返回false
        if (!ret) {
            ERR_LOG("insert user failed:%s", mysql_error(_mysql));
            return false;
        }
        // 如果插入成功，返回true
        return true;
    }

    // 用户登录函数，用于验证用户登录并获取用户信息
    // 参数:
    //   user: 包含用户登录信息和用户信息的Json::Value对象
    // 返回值:
    //   如果登录成功，返回true；否则返回false
    bool login(Json::Value &user) {
        // 构建SQL查询语句，使用用户提供的用户名和密码
        char sql[4096] = {0};
        sprintf(sql, LOGIN_USER, user["username"].asCString(), user["password"].asCString());
        MYSQL_RES *res = nullptr;
        
        {
            // 使用互斥锁保护MySQL连接，防止多线程并发访问
            std::unique_lock<std::mutex> lock(_mutex);

            // 执行MySQL查询
            bool ret = mysql_util::mysql_exec(_mysql, sql);
            if (!ret) {
                ERR_LOG("login failed:%s", mysql_error(_mysql));
                return false;
            }

            // 将查询结果存储在res中
            res = mysql_store_result(_mysql);
            if (!res) {
                DBG_LOG("获取用户结果失败");
                return false;
            }
        }

        // 获取查询结果的行数
        int row_num = mysql_num_rows(res);

        // 如果行数不等于1，表示用户信息不唯一，登录失败
        if (row_num != 1) {
            DBG_LOG("用户信息不唯一");
            return false;
        }

        // 从查询结果中提取用户信息并存储在Json::Value对象中
        MYSQL_ROW row = mysql_fetch_row(res);
        user["id"] = (Json::UInt64) std::stol(row[0]);
        user["score"] = (Json::UInt64) std::stol(row[1]);
        user["total_count"] = std::stoi(row[2]);
        user["win_count"] = std::stoi(row[3]);

        // 释放查询结果的内存
        mysql_free_result(res);

        // 登录成功
        return true;
    }

    //通过用户名获取用户信息
    bool select_by_name(const std::string &name, Json::Value &user) {
        char sql[4096] = {0};
        sprintf(sql, USER_BY_NAME, name.c_str());
        MYSQL_RES *res = nullptr;
        {
            std::unique_lock<std::mutex> lock(_mutex);
            bool ret = mysql_util::mysql_exec(_mysql, sql);
            if (ret == false) {
                DBG_LOG("get user by name failed!!\n");
                return false;
            }
            //按理说要么有数据，要么没有数据，就算有数据也只能有一条数据
            res = mysql_store_result(_mysql);
            if (res == NULL) {
                DBG_LOG("have no user info!!");
                return false;
            }
        }
        int row_num = mysql_num_rows(res);
        if (row_num != 1) {
            DBG_LOG("the user information queried is not unique!!");
            return false;
        }
        MYSQL_ROW row = mysql_fetch_row(res);
        user["id"] = (Json::UInt64) std::stol(row[0]);
        user["username"] = name;
        user["score"] = (Json::UInt64) std::stol(row[1]);
        user["total_count"] = std::stoi(row[2]);
        user["win_count"] = std::stoi(row[3]);
        mysql_free_result(res);
        return true;
    }

    //通过id获取用户信息
    bool select_by_id(int id, Json::Value &user) {
        char sql[4096] = {0};
        sprintf(sql, USER_BY_ID, id);
        MYSQL_RES *res = NULL;
        {
            std::unique_lock<std::mutex> lock(_mutex);
            bool ret = mysql_util::mysql_exec(_mysql, sql);
            if (ret == false) {
                DBG_LOG("get user by id failed!!\n");
                return false;
            }
            //按理说要么有数据，要么没有数据，就算有数据也只能有一条数据
            res = mysql_store_result(_mysql);
            if (res == NULL) {
                DBG_LOG("have no user info!!");
                return false;
            }
        }
        int row_num = mysql_num_rows(res);
        if (row_num != 1) {
            DBG_LOG("the user information queried is not unique!!");
            return false;
        }
        MYSQL_ROW row = mysql_fetch_row(res);
        user["id"] = (Json::UInt64) id;
        user["username"] = row[0];
        user["score"] = (Json::UInt64) std::stol(row[1]);
        user["total_count"] = std::stoi(row[2]);
        user["win_count"] = std::stoi(row[3]);
        mysql_free_result(res);
        return true;
    }
    //胜利时天梯分增加30，战斗场次增加1，胜利场次增加1
    bool win(uint64_t id) {
        char sql[4096] = {0};
        sprintf(sql, USER_WIN, id);
        bool ret = mysql_util::mysql_exec(_mysql, sql);
        if (ret == false) {
            DBG_LOG("update win user info failed!!");
            return false;
        }
        return true;
    }
    //失败时天梯分数减少30，战斗场次增加1，其他不变。
    bool lose(uint64_t id) {
        char sql[4096] = {0};
        sprintf(sql, USER_LOSE, id);
        bool ret = mysql_util::mysql_exec(_mysql, sql);
        if (ret == false) {
            DBG_LOG("update lose user info failed!!");
            return false;
        }
        return true;
    }

private:
    MYSQL *_mysql;    //mysql操作句柄
    std::mutex _mutex;//互斥锁保护数据库的访问操作
};