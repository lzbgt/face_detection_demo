#include <exception>
#include <iostream>
#include <mutex>
#include <nlohmann/json.hpp>
#include <queue>
#include <thread>
#include <zmq.hpp>
#include <zmq_addon.hpp>

#include "utils/safe_queue.hpp"

using json = nlohmann::json;
class EventDetection {
    SafeQueue<int> q;
    bool isRunning = false;

   public:
    static EventDetection& getInstance() {
        static EventDetection inst;
        return inst;
    }
    void notify(int cnt) {
        q.push(cnt);
    }
    void run() {
        if (!isRunning) {
            _run();
        }
    }

   protected:
    EventDetection() {}

   private:
    void _run() {
        isRunning = true;
        auto th = std::thread([this]() {
            std::cout << "init ctx: " << std::endl;
            zmq::context_t ctx(1);
            std::cout << "create socket ctx: " << std::endl;
            zmq::socket_t sock(ctx, zmq::socket_type::pub);
            std::string addr = "tcp://*:3015";
            std::cout << "binding: " << addr << std::endl;
            try {
                sock.bind(addr.c_str());
            } catch (std::exception& e) {
                std::cout << "exception: " << e.what() << std::endl;
                exit(1);
            }
            while (true) {
                auto cnt = this->q.take();
                json data;
                data["cnt"] = cnt;
                if (cnt > 0) {
                    data["event"] = "face_on";
                } else {
                    data["event"] = "face_off";
                }

                const std::string_view m = data.dump();
                std::cout << "sent: " << m << std::endl;
                sock.send(zmq::buffer(m), zmq::send_flags::dontwait);
            }
        });

        th.detach();
    }
};