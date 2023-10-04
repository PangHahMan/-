#pragma once
#include "logger.hpp"
#include "util.hpp"
#include <mutex>
#include <unordered_map>

// 定义会话状态枚举
typedef enum {
    UNLOGIN,// 表示未登录状态
    LOGIN   // 表示已登录状态
} sesson_status;

class session {
private:
    uint64_t _ssid;         // 会话ID
    uint64_t _uid;          // 用户ID
    sesson_status _status;  // 会话状态
    server_t::timer_ptr _tp;// 定时器对象指针
public:
    // 构造函数
    session(uint64_t ssid)
        : _ssid(ssid) {
        DBG_LOG("session %p 创建了", this);
    }

    // 析构函数
    ~session() {
        DBG_LOG("session %p 销毁了", this);
    }

    // 获取会话ID
    uint64_t get_ssid() {
        return _ssid;
    }

    // 设置用户ID
    void set_user(uint64_t uid) {
        _uid = uid;
    }

    // 获取用户ID
    uint64_t get_user() {
        return _uid;
    }

    // 设置会话状态
    void set_status(sesson_status status) {
        _status = status;
    }

    // 判断是否登录
    bool is_login() {
        if (_status == LOGIN) {
            return true;
        }
        return false;
    }

    // 设置定时器对象
    void set_timer(const server_t::timer_ptr &tp) {
        _tp = tp;
    }

    // 获取定时器对象
    server_t::timer_ptr &get_timer() {
        return _tp;
    }
};

#define SESSION_TIMEOUT 30000                // 定义会话超时时间
#define SESSION_FOREVER -1                   // 定义永久会话
using session_ptr = std::shared_ptr<session>;// 定义会话智能指针

class session_manager {
private:
    uint64_t _session_id;                              // 会话ID计数器
    std::mutex _mutex;                                 // 互斥锁
    std::unordered_map<uint64_t, session_ptr> _session;// 存储会话的哈希表
    server_t *_server;                                 // 服务器对象

public:
    // 构造函数
    session_manager(server_t *server)
        : _session_id(1), _server(server) {
        DBG_LOG("session_manager初始化完毕");
    }

    // 析构函数
    ~session_manager() {
        DBG_LOG("session_manager销毁成功");
    }

    // 创建新会话
    session_ptr create_sesson(uint64_t uid, sesson_status status) {
        std::unique_lock<std::mutex> _mutex;// 独占锁，保护共享资源
        session_ptr ssp(new session(_session_id));
        ssp->set_status(status);
        ssp->set_user(uid);
        _session.insert(std::make_pair(_session_id, ssp));

        _session_id++;
        return ssp;
    }

    // 获取会话
    session_ptr get_sesson(uint64_t sesson_id) {
        std::unique_lock<std::mutex> _mutex;// 独占锁，保护共享资源
        auto it = _session.find(sesson_id); // 查找会话
        if (it == _session.end()) {
            return session_ptr();// 未找到，返回空智能指针
        }

        return it->second;// 返回找到的会话
    }

    // 移除会话
    void remove_session(uint64_t sesson_id) {
        std::unique_lock<std::mutex> _mutex;// 独占锁，保护共享资源
        _session.erase(sesson_id);          // 移除会话
    }

    // 添加会话
    void append_session(const session_ptr &ssp) {
        std::unique_lock<std::mutex> lock(_mutex);            // 独占锁，保护共享资源
        _session.insert(std::make_pair(ssp->get_ssid(), ssp));// 添加会话
    }

    void set_session_expire_time(uint64_t session_id, int ms) {
        //依赖于websocketpp的定时器来完成session生命周期的管理
        //在http通信的时候(登录，注册)session应该具备生命周期，指定时间无通信后删除
        //在客户端建立websocket长连接之后，sesson应该是永久存在的
        //登录之后，创建session，session需要在指定时间无通信后删除
        //但是进入游戏大厅，或者游戏房间，这个session就应该长久存在
        //等到退出游戏大厅，或者游戏房间，这个session应该被重新设置为临时，在长时间无通信后删除
        session_ptr ssp = get_sesson(session_id);//通过会话ID，获取对应的sesson类
        if (ssp.get() == nullptr) {
            return;
        }

        server_t::timer_ptr tp = ssp->get_timer();//获取定时器
        if (tp.get() == nullptr && ms == SESSION_FOREVER) {
            //意味着session在创建时被设置为永久存在，所以无需任何更改，所以函数直接返回。
            //1.在sesson永久存在的情况下，设置永久存在
            return;
        } else if (tp.get() == nullptr && ms != SESSION_FOREVER) {
            //2.在sesson永久存在的情况下，设置指定时间之后被删除的定时任务
            //意味着要将永久存在的session更改为临时session。因此，创建一个新的定时器，当指定的时间ms到达时移除session。
            server_t::timer_ptr tmp_tp = _server->set_timer(ms, std::bind(&session_manager::remove_session, this, session_id));
            ssp->set_timer(tmp_tp);
        } else if (tp.get() != nullptr && ms == SESSION_FOREVER) {
            //3.在sesson设置了定时删除的情况下，将sesson设置为永久存在
            //删除定时任务--- stready_timer删除定时任务会导致任务直接执行
            //意味着要将临时的session更改为永久的session。因此，取消定时器并将其设置为空，因为永久的session不需要定时器。
            tp->cancel();                         //这个取消定时任务不是立即取消的
            ssp->set_timer(server_t::timer_ptr());//将session关联的定时器设置为空
                                                  //因此重新给session管理器中，添加一个session信息, 且添加的时候需要使用定时器，而不是立即添加
            _server->set_timer(0, std::bind(&session_manager::append_session, this, ssp));
        } else if (tp.get() != nullptr && ms != SESSION_FOREVER) {
            //4.在sesson设置了定时删除的情况下，将sesson重置删除时间
            //意味着要更改session的过期时间。因此，取消原来的定时器，并创建一个新的定时器，当新的时间ms到达时移除session。
            tp->cancel();
            ssp->set_timer(server_t::timer_ptr());
            _server->set_timer(0, std::bind(&session_manager::append_session, this, ssp));

            //重新给session添加定时销毁任务
            server_t::timer_ptr tmp_tp = _server->set_timer(ms, std::bind(&session_manager::remove_session, this, ssp->get_ssid()));

            //重新设置session关联的定时器
            ssp->set_timer(tmp_tp);
        }
    }
};