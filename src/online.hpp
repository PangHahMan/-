#pragma once
#include "util.hpp"
#include <mutex>
#include <unordered_map>
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

class online_manager {
private:
    std::mutex _mutex;
    //用于建立游戏大厅用户的用户ID与通信连接的关系
    std::unordered_map<uint64_t, server_t::connection_ptr> _game_hall;
    //用于建立游戏房间用户的用户ID与通信连接的关系
    std::unordered_map<uint64_t, server_t::connection_ptr> _game_room;

public:
    online_manager() {}
    //websocket连接建立的时候才会加入游戏大厅&游戏房间在线用户管理
    void enter_game_hall(uint64_t uid, server_t::connection_ptr &conn) {
        std::unique_lock<std::mutex> _mutex;
        _game_hall.insert(std::make_pair(uid, conn));
        //_game_hall[uid] = conn
    }
    void enter_game_room(uint64_t uid, server_t::connection_ptr &conn) {
        std::unique_lock<std::mutex> _mutex;
        _game_room.insert(std::make_pair(uid, conn));
    }
    //websocket连接断开的时候，才会移除游戏大厅&游戏房间在线用户管理
    bool exit_game_hall(uint64_t uid) {
        std::unique_lock<std::mutex> _mutex;
        _game_hall.erase(uid);
    }
    bool exit_game_room(uint64_t uid) {
        std::unique_lock<std::mutex> _mutex;
        _game_room.erase(uid);
    }
    //判断当前指定用户是否在游戏大厅/游戏房间
    bool is_in_game_hall(uint64_t uid) {
        std::unique_lock<std::mutex> _mutex;
        auto it = _game_hall.find(uid);
        if (it == _game_hall.end()) {
            return false;
        }
        return true;
    }

    bool is_in_game_room(uint64_t uid) {
        std::unique_lock<std::mutex> _mutex;
        auto it = _game_room.find(uid);
        if (it == _game_room.end()) {
            return false;
        }
        return true;
    }

    //通过用户ID在游戏大厅/游戏房间用户管理中获取对应的通信连接
    server_t::connection_ptr get_conn_from_hall(uint64_t uid) {
        std::unique_lock<std::mutex> _mutex;
        auto it = _game_hall.find(uid);
        if (it == _game_hall.end()) {
            return server_t::connection_ptr();
        }
        return it->second;
    }

    server_t::connection_ptr get_conn_from_room(uint64_t uid) {
        std::unique_lock<std::mutex> _mutex;
        auto it = _game_room.find(uid);
        if (it == _game_room.end()) {
            return server_t::connection_ptr();
        }
        return it->second;
    }
};