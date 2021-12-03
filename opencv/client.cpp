
#include <exception>
#include <iostream>
#include <nlohmann/json.hpp>
#include <thread>
#include <vector>
#include <zmq.hpp>
#include <zmq_addon.hpp>

int main() {
    zmq::context_t ctx(1);
    zmq::socket_t subscriber(ctx, zmq::socket_type::sub);
    std::cout << "connect to zmq" << std::endl;
    try {
        subscriber.connect("tcp://127.0.0.1:3015");
        std::cout << "set sockopt" << std::endl;

        subscriber.set(zmq::sockopt::subscribe, "");

    } catch (std::exception &e) {
        std::cout << "exception: " << e.what() << std::endl;
        exit(1);
    }

    while (1) {
        // Receive all parts of the message
        std::vector<zmq::message_t>
            recv_msgs;
        std::cout << "receiving msg: " << std::endl;
        zmq::recv_result_t result =
            zmq::recv_multipart(subscriber, std::back_inserter(recv_msgs));
        assert(result && "recv failed");
        assert(*result == 1);

        std::cout << "Thread2: [" << recv_msgs[0].to_string() << "] " << std::endl;
    }
    return 0;
}