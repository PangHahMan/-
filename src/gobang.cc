#include "db.hpp"
#include "logger.hpp"
#include "online.hpp"
#include "room.hpp"
#include "util.hpp"
#include <iostream>

#define HOST "127.0.0.1"
#define PORT 3306
#define USER "taeyeon"
#define PASS "2002Phw@"
#define DBNAME "gobang"
//日志宏测试
void testLog() {
    DBG_LOG("%s-%d\n", "Hello World", 2023);
}
//mysql测试
void testmysql() {
    MYSQL *mysql = mysql_util::mysql_create(HOST, USER, PASS, DBNAME, 3306);
    const char *sql = "insert stu values(null ,'小黑', 18, 53, 68, 87)";
    bool ret = mysql_util::mysql_exec(mysql, sql);
    if (ret == false) {
        ERR_LOG("%s err\n", sql);
        return;
    }
    mysql_util::mysql_destroy(mysql);
}

void testjson() {
    Json::Value root;
    std::string body;
    root["姓名"] = "小明";
    root["年龄"] = 18;
    root["成绩"].append(98);
    root["成绩"].append(88.5);
    root["成绩"].append(78.5);
    json_util::serialize(root, body);
    printf("%s", body.c_str());

    Json::Value val;
    json_util::unserialize(body, val);
    std::cout << "姓名:" << val["姓名"].asString() << std::endl;
    std::cout << "年龄:" << val["年龄"].asInt() << std::endl;
    int sz = val["成绩"].size();
    for (int i = 0; i < sz; i++) {
        std::cout << "成绩: " << val["成绩"][i].asFloat() << std::endl;
    }
}

void str_test() {
    std::string str = ",...,,123,234,,,,,345,,,,";
    std::vector<std::string> arry;
    string_util::split(str, ",", arry);
    for (auto &s: arry) {
        DBG_LOG("%s", s.c_str());
    }
}

void file_test() {
    std::string filename = "./makefile";
    std::string body;
    file_util::read(filename, body);

    std::cout << body << std::endl;
}

void db_test() {
    user_table user(HOST, USER, PASS, DBNAME, PORT);
    Json::Value root;
    root["username"] = "xiaoming";
    root["password"] = "2002Phw@";
    user.insert(root);
}

void db2_test() {
    user_table user(HOST, USER, PASS, DBNAME, PORT);
    Json::Value root;
    root["username"] = "xiaoming";
    root["password"] = "2002Phw@";
    bool ret = user.login(root);
    if (!ret) {
        DBG_LOG("login failed!");
    }
}

void db3_test() {
    user_table user(HOST, USER, PASS, DBNAME, PORT);
    Json::Value root;
    bool ret = user.select_by_name("xiaoming", root);
    std::string body;
    json_util::serialize(root, body);
    std::cout << body << std::endl;

    ret = user.select_by_id(1, root);
    json_util::serialize(root, body);
    std::cout << body << std::endl;
}

void db4_test() {
    user_table user(HOST, USER, PASS, DBNAME, PORT);
    user.win(1);
    user.win(1);
    user.win(1);
    user.win(1);
    user.win(1);
    user.lose(1);
    user.lose(1);
    //赢了五次加了150  输了2次-60 = 90分数
    user.lose(2);
}

//测试添加和删除
void online_test() {
    online_manager om;
    server_t::connection_ptr conn;
    uint64_t uid = 2;
    om.enter_game_hall(uid, conn);
    if (om.is_in_game_hall(uid)) {
        DBG_LOG("is_in_game_hall");
    } else {
        DBG_LOG("not_in_game_hall");
    }

    om.exit_game_hall(uid);
    if (om.is_in_game_hall(uid)) {
        DBG_LOG("is_in_game_hall");
    } else {
        DBG_LOG("not_in_game_hall");
    }
}

void room_test() {
    user_table user(HOST, USER, PASS, DBNAME, PORT);
    online_manager om;
    room_manager rm(&user, &om);
    room_ptr rp = rm.create_room(10, 20);
}
int main() {
    room_test();
    return 0;
}