#pragma once
#include "logger.hpp"
#include "util.hpp"
#include <unordered_map>
typedef enum {
    UNLOGIN,
    LOGIN
} sesson_status;

class sesson {
private:
    uint64_t _ssid;
    uint64_t _uid;
    sesson_status _status;
    server_t::timer_ptr _tp;

public:
    sesson(uint64_t ssid) {
        DBG_LOG("sesson %p 创建了", this);
    }

    ~sesson() {
        DBG_LOG("sesson %p 销毁了", this);
    }

    void set_user(uint64_t uid) {
        _uid = uid;
    }

    uint64_t get_user() {
        return _uid;
    }

    bool is_login() {
        if (_status == LOGIN) {
            return true;
        }
        return false;
    }

    void set_timer(const server_t::timer_ptr &tp) {
        _tp = tp;
    }

    server_t::timer_ptr &get_timer() {
        return _tp;
    }
};

using sesson_ptr = std::shared_ptr<sesson>;

class sesson_manager {
private:
    uint64_t _sesson_id;
    std::mutex _mutex;
    std::unordered_map<uint64_t, sesson_ptr> _sesson;
    server_t _server;

public:
    sesson_ptr create_sesson(uint64_t uid,sesson_status status) {
        
    }

    sesson_ptr get_sesson(uint64_t sesson_id) {
        
    }

    void set_session_expire_time(uint64_t sesson_id,int ms) {
        
    }

    void remove_session(uint64_t sesson_id);
};