#pragma once
#include "db.hpp"
#include "logger.hpp"
#include "online.hpp"
#include "util.hpp"
#include <unordered_map>
#include <vector>

#define BOARD_ROW 15
#define BOARD_COL 15
#define CHESS_WHITE 1
#define CHESS_BLACK 2

//定义房间状态
typedef enum {
    GAME_START,
    GAME_OVER
} room_status;

class room {
private:
    uint64_t _room_id;                   //房间id
    room_status _status;                 //房间状态
    int _player_num;                     //房间人数
    uint64_t _white_id;                  //白棋id
    uint64_t _black_id;                  //黑棋id
    user_table *_user;                   //用户管理
    online_manager *_online_user;        //在线用户管理
    std::vector<std::vector<int>> _board;//棋盘

private:
    bool five(int row, int col, int row_off, int col_off, int color) {
        // row和col是棋子位置，row_off和col_off是偏移量，也就是需要检查的方向，color是棋子颜色。
        int count = 1;                 // 初始化同色棋子数量为1，因为已经有一个棋子在(row, col)的位置上。
        int search_row = row + row_off;// 计算向指定方向偏移后的行位置。
        int search_col = col + col_off;// 计算向指定方向偏移后的列位置。

        // 当偏移后的位置在棋盘内，并且该位置的棋子颜色与指定颜色相同时，继续偏移。
        while (search_row >= 0 && search_row < BOARD_ROW &&
               search_col >= 0 && search_col < BOARD_COL &&
               _board[search_row][search_col] == color) {
            count++;// 同色棋子数量加1。
            // 继续向指定方向偏移。
            search_row += row_off;
            search_col += col_off;
        }

        // 更改搜索方向，向相反方向偏移。
        search_row = row - row_off;
        search_col = col - col_off;

        // 同样的，当偏移后的位置在棋盘内，并且该位置的棋子颜色与指定颜色相同时，继续偏移。
        while (search_row >= 0 && search_row < BOARD_ROW &&
               search_col >= 0 && search_col < BOARD_COL &&
               _board[search_row][search_col] == color) {
            count++;// 同色棋子数量加1。
            // 继续向指定方向偏移。
            search_row -= row_off;
            search_col -= col_off;
        }

        // 如果同色棋子数量大于等于5，返回true，表示形成五子连线；否则，返回false。
        return (count >= 5);
    }

    //函数返回胜利的颜色,1为白棋,2为黑棋,0为平局
    uint64_t check_win(int row, int col, int color) {
        // 从下棋位置的四个不同方向上检测是否出现了5个及以上相同颜色的棋子（横行，纵列，正斜，反斜）
        if (five(row, col, 0, 1, color) ||
            five(row, col, 1, 0, color) ||
            five(row, col, -1, 1, color) ||
            five(row, col, -1, -1, color)) {
            //任意一个方向上出现了true也就是五星连珠，则设置返回值
            return color == CHESS_WHITE ? _white_id : _black_id;
        }
        return 0;
    }

public:
    room(uint64_t room_id, user_table *user, online_manager *online_user)
        : _room_id(room_id),
          _status(GAME_START),
          _player_num(0),
          _user(user),
          _online_user(online_user),
          _board(BOARD_ROW, std::vector<int>(BOARD_COL, 0)) {
        DBG_LOG("room create:%d", _room_id);
    }

    ~room() {
        DBG_LOG("room destroy:%d", _room_id);
    }

    uint64_t get_room_id() {
        return _room_id;
    }

    room_status get_status() {
        return _status;
    }

    int get_player_num() {
        return _player_num;
    }

    void add_white_user(uint64_t user_id) {
        _white_id = user_id;
        _player_num++;
    }

    void add_black_user(uint64_t user_id) {
        _black_id = user_id;
        _player_num++;
    }

    uint64_t get_white_id() {
        return _white_id;
    }

    uint64_t get_black_id() {
        return _black_id;
    }

    //处理下棋动作
    Json::Value handle_chess(Json::Value &req) {
        Json::Value json_rsp = req;// 使用请求数据初始化响应数据。
        // 1. 判断房间中两个玩家是否都在线，任意一个不在线，就是另一方胜利。
        if (_online_user->is_in_game_room(_white_id) == false) {
            json_rsp["result"] = "true";                        // 结果设为true，表示有玩家获胜。
            json_rsp["reason"] = "white is offline , black win";// 原因设为"白方离线，黑方胜利"。
            json_rsp["winner"] = (Json::UInt64) _black_id;      // 获胜方设为黑方。
            return json_rsp;                                    // 返回响应数据。
        }

        if (_online_user->is_in_game_room(_black_id) == false) {
            json_rsp["result"] = "true";                        // 结果设为true，表示有玩家获胜。
            json_rsp["reason"] = "black is offline , white win";// 原因设为"黑方离线，白方胜利"。
            json_rsp["winner"] = (Json::UInt64) _white_id;      // 获胜方设为白方。
            return json_rsp;                                    // 返回响应数据。
        }

        // 2. 获取走棋位置，判断当前走棋是否合理(位置是否被占用)
        int chess_row = req["row"].asInt();             // 获取棋子行位置。
        int chess_col = req["col"].asInt();             // 获取棋子列位置。
        if (_board[chess_row][chess_col] != 0) {        // 如果指定位置已经有棋子，则走棋不合理。
            json_rsp["result"] = false;                 // 结果设为false，表示走棋不合理。
            json_rsp["reason"] = "position is occupied";// 原因设为"位置被占用"。
            return json_rsp;                            // 返回响应数据。
        }

        // 3. 获取当前用户的id和颜色，进行落子
        uint64_t cur_uid = req["uid"].asUInt64();                        // 获取当前用户id。
        int cur_color = cur_uid == _white_id ? CHESS_WHITE : CHESS_BLACK;// 判断当前用户颜色。
        _board[chess_row][chess_col] = cur_color;                        // 在指定位置落子。

        // 4. 判断是否有玩家胜利（从当前走棋位置开始判断是否存在五星连珠）
        uint64_t winner_id = check_win(chess_row, chess_col, cur_color);
        if (winner_id != 0) {
            json_rsp["reason"] = "five in a row";// 原因设为"五星连珠"
        }
        json_rsp["result"] = "true";                  // 结果设为true，表示有玩家获胜。
        json_rsp["winner"] = (Json::UInt64) winner_id;// 获胜方设为获胜者。
        return json_rsp;
    }

    // 处理聊天动作
    Json::Value handle_chat(Json::Value &req) {
        // 定义响应的Json对象
        Json::Value json_rsp = req;
        // 获取请求中的消息
        std::string msg = req["message"].asString();
        // 搜索是否存在敏感词
        size_t pos = msg.find("垃圾");
        if (pos != std::string::npos) {
            // 如果存在敏感词，则标记结果为false，添加不合适的原因
            json_rsp["result"] = false;
            json_rsp["reason"] = "消息中包含敏感词";
            return json_rsp;
        }
        // 如果不存在敏感词，则标记结果为true
        json_rsp["result"] = true;

        return json_rsp;
    }

    // 处理玩家退出房间
    void handle_exit(uint64_t uid) {
        // 定义响应的Json对象
        Json::Value json_rsp;
        // 如果游戏已经开始，且玩家退出
        if (_status == GAME_START) {
            // 确定赢家的id，如果退出的是白棋玩家，那么黑棋玩家就是赢家，反之亦然
            uint64_t winner_id = (Json::UInt64)(uid == _white_id ? _black_id : _white_id);
            // 设置响应的各项参数
            json_rsp["optype"] = "put_chess";
            json_rsp["result"] = "true";
            json_rsp["reason"] = "对方掉线";
            json_rsp["room_id"] = (Json::UInt64) _room_id;
            json_rsp["uid"] = (Json::UInt64) uid;
            json_rsp["row"] = -1;
            json_rsp["col"] = -1;
            json_rsp["winner"] = (Json::UInt64) winner_id;
            // 确定输家的id，如果赢家是白棋玩家，那么黑棋玩家就是输家，反之亦然
            uint64_t loser_id = (Json::UInt64)(winner_id == _white_id ? _black_id : _white_id);
            // 更新用户数据库的输赢信息
            _user->win(winner_id);
            _user->lose(loser_id);
            // 更改游戏状态为结束
            _status = GAME_OVER;
            // 广播响应
            broadcast(json_rsp);
        }
        // 房间内玩家数量减一
        _player_num--;

        return;
    }

    void broadcast(Json::Value &rsp) {
        //1. 首先，对要响应的信息进行序列化操作，将Json::Value类型的数据转换成json格式的字符串
        std::string body;
        json_util::serialize(rsp, body);

        //2. 然后，从在线用户中获取房间中白棋玩家的通信连接
        server_t::connection_ptr wconn = _online_user->get_conn_from_room(_white_id);
        // 检查获取到的白棋玩家的连接是否为空，如果不为空，则向白棋玩家发送广播消息
        if (wconn.get() != nullptr) {
            wconn->send(body);
        } else {
            // 如果白棋玩家的连接为空，则打印错误日志
            DBG_LOG("房间-白棋玩家连接获取失败");
        }

        //3. 从在线用户中获取房间中黑棋玩家的通信连接
        server_t::connection_ptr bconn = _online_user->get_conn_from_room(_black_id);
        // 检查获取到的黑棋玩家的连接是否为空，如果不为空，则向黑棋玩家发送广播消息
        if (bconn.get() != nullptr) {
            bconn->send(body);
        } else {
            // 如果黑棋玩家的连接为空，则打印错误日志
            DBG_LOG("房间-黑棋玩家连接获取失败");
        }
        return;
    }

    //总的请求处理函数，在函数内部，区分请求类型，根据不同的请求调用不同的处理函数，得到响应进行广播
    void handle_request(Json::Value &req) {
        // 初始化响应的json对象
        Json::Value json_rsp;
        // 从请求中取出房间号
        uint64_t room_id = req["room_id"].asUInt64();

        // 如果请求的房间号与当前房间号不匹配
        if (room_id != _room_id) {
            // 设置响应的操作类型为请求的操作类型
            json_rsp["optype"] = req["optype"].asString();
            // 设置响应结果为失败
            json_rsp["result"] = false;
            // 设置失败原因为"房间号不匹配"
            json_rsp["reason"] = "房间号不匹配！";
            // 广播响应结果并返回
            return broadcast(json_rsp);
        }

        // 根据请求类型调用不同的处理函数
        if (req["optype"].asString() == "put_chess") {
            // 如果请求类型为"put_chess"，调用下棋处理函数
            json_rsp = handle_chess(req);
            // 如果有胜利者
            if (json_rsp["winner"].asUInt64() != 0) {
                // 获取胜利者和失败者的id
                uint64_t winner_id = json_rsp["winner"].asUInt64();
                uint64_t loser_id = winner_id == _white_id ? _black_id : _white_id;
                // 更新胜利者和失败者的状态
                _user->win(winner_id);
                _user->lose(loser_id);

                // 设置游戏状态为"游戏结束"
                _status = GAME_OVER;
            }
        } else if (req["optype"].asString() == "chat") {
            // 如果请求类型为"chat"，调用聊天处理函数
            json_rsp = handle_chat(req);
        } else {
            // 如果请求类型未知，设置响应的操作类型为请求的操作类型
            json_rsp["optype"] = req["optype"].asString();
            // 设置响应结果为失败
            json_rsp["result"] = false;
            // 设置失败原因为"未知请求类型"
            json_rsp["reason"] = "未知请求类型";
        }
        // 将响应结果序列化为字符串
        std::string body;
        json_util::serialize(json_rsp, body);
        // 打印广播动作的日志
        DBG_LOG("房间-广播动作:%s", body.c_str());
        // 广播响应结果
        return broadcast(json_rsp);
    }
};


using room_ptr = std::shared_ptr<room>;
class room_manager {
private:
    std::mutex _mutex;                               //互斥锁
    uint64_t _room_id;                               //房间ID分配
    user_table *_user;                               //数据库用户管理
    online_manager *_online_user;                    //在线用户管理
    std::unordered_map<uint64_t, room_ptr> _room;    //建立起房间ID与房间信息的映射关系
    std::unordered_map<uint64_t, uint64_t> _room_ids;//先通过用户ID找到所在房间ID，再去查找房间信息

public:
    room_manager(user_table *user, online_manager *online_user)
        : _room_id(1), _user(user), _online_user(online_user) {
        DBG_LOG("房间管理模块初始化完毕");
    }

    ~room_manager() {
        DBG_LOG("房间管理模块即将销毁");
    }

    //为两个用户创建房间，并返回房间的智能指针管理对象
    room_ptr create_room(uint64_t uid1, uint64_t uid2) {
        //两个用户在游戏大厅中进行对战匹配，匹配成功后创建房间
        //1.校验两个用户是否都还在游戏大厅中，只有都在才需要创建房间。
        if (_online_user->is_in_game_room(uid1) == false) {
            DBG_LOG("用户：%lu 不在大厅中，创建房间失败!", uid1);
            return room_ptr();
        }

        if (_online_user->is_in_game_room(uid2) == false) {
            DBG_LOG("用户：%lu 不在大厅中，创建房间失败!", uid2);
            return room_ptr();
        }

        //2.创建房间，将用户信息添加到房间中
        std::unique_lock<std::mutex> lock(_mutex);
        room_ptr rp(new room(_room_id, _user, _online_user));
        rp->add_white_user(uid1);
        rp->add_black_user(uid2);

        //3.将房间信息管理起来
        _room.insert(std::make_pair(_room_id, rp));
        _room_id++;
        _room_ids.insert(std::make_pair(uid1, _room_id));
        _room_ids.insert(std::make_pair(uid2, _room_id));

        //4.返回房间信息
        return rp;
    }

    //加锁操作确保了在查找和返回_room中指定元素的操作是原子的，即在这个操作过程中不会被其他线程打断。这可以避免在找到元素后但还没来得及返回时，其他线程修改了该元素或者从_room中删除了该元素，导致返回了一个无效的引用。
    room_ptr get_room_by_rid(uint64_t rid) {
        std::unique_lock<std::mutex> _mutex;
        auto it = _room.find(rid);
        if (it == _room.end()) {
            return room_ptr();
        }

        return it->second;
    }

    room_ptr get_room_by_uid(uint64_t uid) {
        std::unique_lock<std::mutex> _mutex;
        auto it = _room_ids.find(uid);
        if (it == _room_ids.end()) {
            return room_ptr();
        }

        uint64_t rid = it->second;
        auto rit = _room.find(rid);
        if (rit == _room.end()) {
            return room_ptr();
        }

        return rit->second;
    }

    //通过房间ID销毁房间
    void remove_room(uint64_t rid) {
        //因为房间信息，是通过shared_ptr在_rooms中进行管理，因此只要将shared_ptr从_rooms中移除
        //则shared_ptr计数器==0，外界没有对房间信息进行操作保存的情况下就会释放
        //1. 通过房间ID，获取房间信息
        room_ptr rp = get_room_by_rid(rid);
        if (rp.get() == nullptr) {
            return;
        }

        //2. 通过房间信息，获取房间中所有用户的ID
        uint64_t uid1 = rp->get_white_id();
        uint64_t uid2 = rp->get_black_id();
        //3. 移除房间管理中的用户信息
        std::unique_lock<std::mutex> lock(_mutex);
        _room_ids.erase(uid1);
        _room_ids.erase(uid2);
        //4. 移除房间管理信息
        _room.erase(rid);
    }

    void remove_room_user(uint64_t uid) {
        room_ptr rp = get_room_by_uid(uid);
        if (rp.get() == nullptr) {
            return;
        }

        //处理房间中玩家退出动作
        rp->handle_exit(uid);
        //房间中没有玩家了，则销毁房间
        if (rp->get_player_num() == 0) {
            remove_room(rp->get_room_id());
        }
        return;
    }
};