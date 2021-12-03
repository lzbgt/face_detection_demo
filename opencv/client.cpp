
#include <exception>
#include <iostream>
#include <nlohmann/json.hpp>
#include <thread>
#include <vector>
#include <zmq.hpp>
#include <zmq_addon.hpp>

int main() {
    zmq::context_t ctx(1);
    zmq::socket_t subscriber(ctx, zmq::socket_type::dealer);
    try {
        int opt = 5;
        // subscriber.set(zmq::sockopt::tcp_keepalive, opt);
        subscriber.setsockopt(ZMQ_IDENTITY, "client-demo", 12);    // selfId
        subscriber.setsockopt(ZMQ_ROUTING_ID, "client-demo", 12);  // routeId
        std::cout << "connect to zmq" << std::endl;
        subscriber.connect("tcp://127.0.0.1:3015");
    } catch (std::exception &e) {
        std::cout << "exception: " << e.what() << std::endl;
        exit(1);
    }

    auto v = {zmq::buffer("hello")};

    zmq::send_multipart(subscriber, v);

    while (1) {
        // Receive all parts of the message
        std::vector<zmq::message_t>
            recv_msgs;
        std::cout << "receiving msg: ";
        zmq::recv_result_t result =
            zmq::recv_multipart(subscriber, std::back_inserter(recv_msgs));
        for (auto &s : recv_msgs) {
            auto pos = &s - &recv_msgs[0];
            std::cout << "pos: " << pos << " " << s.to_string();
        }
        std::cout << std::endl;
    }
    return 0;
}