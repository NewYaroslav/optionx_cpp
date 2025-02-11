#include "optionx_cpp/parts/utils/pubsub.hpp"
#include <iostream>
#include <memory>

/// \class MessageA
/// \brief Event type for message A.
class MessageA : public optionx::utils::Event {
public:
    int data;
    explicit MessageA(int val) : data(val) {}
};

/// \class MessageB
/// \brief Event type for message B.
class MessageB : public optionx::utils::Event {
public:
    std::string text;
    explicit MessageB(const std::string& str) : text(str) {}
};

/// \class MessageC
/// \brief Event type for message C.
class MessageC : public optionx::utils::Event {
public:
    double value;
    explicit MessageC(double val) : value(val) {}
};

/// \class MessageD
/// \brief Event type for message D.
class MessageD : public optionx::utils::Event {
public:
    int a, b;
    explicit MessageD(int a, int b) : a(a), b(b) {}
};

/// \class Module1
/// \brief Sends MessageA and receives MessageB.
class Module1 : public optionx::utils::EventMediator {
public:

    Module1(optionx::utils::EventHub& hub) : EventMediator(hub) {
        subscribe<MessageB>([](std::shared_ptr<MessageB> msg) {
            std::cout << "Module1 received MessageB with text: " << msg->text << std::endl;
        });
        subscribe<MessageC>(this);
    }

    void send_message_a(int data) {
        auto msg = std::make_shared<MessageA>(data);
        notify(msg);
    }

    void on_event(const std::shared_ptr<optionx::utils::Event>& event) override {
        if (auto msg = std::dynamic_pointer_cast<MessageA>(event)) {
            std::cout << "Module1 received MessageA with data: " << msg->data << std::endl;
        } else
        if (auto msg = std::dynamic_pointer_cast<MessageB>(event)) {
            std::cout << "Module1 received MessageB with text: " << msg->text << std::endl;
        } else
        if (auto msg = std::dynamic_pointer_cast<MessageC>(event)) {
            std::cout << "Module1 received MessageC with value: " << msg->value << std::endl;
        } else
        if (auto msg = std::dynamic_pointer_cast<MessageD>(event)) {
            std::cout << "Module1 received MessageD with a: " << msg->a << "; b: " << msg->b << std::endl;
        }
    };

    void on_event(const optionx::utils::Event* const event) override {
        if (auto msg = dynamic_cast<const MessageA*>(event)) {
            std::cout << "Module1 received MessageA with data: " << msg->data << std::endl;
        } else
        if (auto msg = dynamic_cast<const MessageB*>(event)) {
            std::cout << "Module1 received MessageB with text: " << msg->text << std::endl;
        } else
        if (auto msg = dynamic_cast<const MessageC*>(event)) {
            std::cout << "Module1 received MessageC with value: " << msg->value << std::endl;
        } else
        if (auto msg = dynamic_cast<const MessageD*>(event)) {
            std::cout << "Module1 received MessageD with a: " << msg->a << "; b: " << msg->b << std::endl;
        }
    };
};

/// \class Module2
/// \brief Receives MessageA and sends MessageB.
class Module2 : public optionx::utils::EventMediator {
public:
    Module2(optionx::utils::EventHub& hub) : optionx::utils::EventMediator(hub) {
        subscribe<MessageA>([this](std::shared_ptr<MessageA> msg) {
            std::cout << "Module2 received MessageA with data: " << msg->data << std::endl;
            send_message_b("Hello from Module2");
        });
        subscribe<MessageD>(this);
    }

    void send_message_b(const std::string& text) {
        auto msg = std::make_shared<MessageB>(text);
        notify(msg);
    }

    void on_event(const std::shared_ptr<optionx::utils::Event>& event) override {
        if (auto msg = std::dynamic_pointer_cast<MessageA>(event)) {
            std::cout << "Module2 received MessageA with data: " << msg->data << std::endl;
        } else
        if (auto msg = std::dynamic_pointer_cast<MessageB>(event)) {
            std::cout << "Module2 received MessageB with text: " << msg->text << std::endl;
        } else
        if (auto msg = std::dynamic_pointer_cast<MessageC>(event)) {
            std::cout << "Module2 received MessageC with value: " << msg->value << std::endl;
        } else
        if (auto msg = std::dynamic_pointer_cast<MessageD>(event)) {
            std::cout << "Module2 received MessageD with a: " << msg->a << "; b: " << msg->b << std::endl;
        }
    };

    void on_event(const optionx::utils::Event* const event) override {
        if (auto msg = dynamic_cast<const MessageA*>(event)) {
            std::cout << "Module2 received MessageA with data: " << msg->data << std::endl;
        } else
        if (auto msg = dynamic_cast<const MessageB*>(event)) {
            std::cout << "Module2 received MessageB with text: " << msg->text << std::endl;
        } else
        if (auto msg = dynamic_cast<const MessageC*>(event)) {
            std::cout << "Module2 received MessageC with value: " << msg->value << std::endl;
        } else
        if (auto msg = dynamic_cast<const MessageD*>(event)) {
            std::cout << "Module2 received MessageD with a: " << msg->a << "; b: " << msg->b << std::endl;
        }
    };
};

/// \class Module3
/// \brief Receives MessageC by reference.
class Module3 : public optionx::utils::EventMediator {
public:
    Module3(optionx::utils::EventHub& hub) : optionx::utils::EventMediator(hub) {
        subscribe<MessageC>(this);
    }

    void send_message_c(double value) {
        MessageC msg(value);
        notify(msg);
    }

    void send_message_d(int a, int b) {
        auto msg = std::make_shared<MessageD>(a, b);
        notify(msg);
    }

    void on_event(const std::shared_ptr<optionx::utils::Event>& event) override {
        if (auto msg = std::dynamic_pointer_cast<MessageA>(event)) {
            std::cout << "Module3 received MessageA with data: " << msg->data << std::endl;
        } else
        if (auto msg = std::dynamic_pointer_cast<MessageB>(event)) {
            std::cout << "Module3 received MessageB with text: " << msg->text << std::endl;
        } else
        if (auto msg = std::dynamic_pointer_cast<MessageC>(event)) {
            std::cout << "Module3 received MessageC with value: " << msg->value << std::endl;
        } else
        if (auto msg = std::dynamic_pointer_cast<MessageD>(event)) {
            std::cout << "Module3 received MessageD with a: " << msg->a << "; b: " << msg->b << std::endl;
        }
    };

    void on_event(const optionx::utils::Event* const event) override {
        if (auto msg = dynamic_cast<const MessageA*>(event)) {
            std::cout << "Module3 received MessageA with data: " << msg->data << std::endl;
        } else
        if (auto msg = dynamic_cast<const MessageB*>(event)) {
            std::cout << "Module3 received MessageB with text: " << msg->text << std::endl;
        } else
        if (auto msg = dynamic_cast<const MessageC*>(event)) {
            std::cout << "Module3 received MessageC with value: " << msg->value << std::endl;
        } else
        if (auto msg = dynamic_cast<const MessageD*>(event)) {
            std::cout << "Module3 received MessageD with a: " << msg->a << "; b: " << msg->b << std::endl;
        }
    };
};

int main() {
    optionx::utils::EventHub hub;

    Module1 module1(hub);
    Module2 module2(hub);
    Module3 module3(hub);

    // Test sending and receiving events
    module1.send_message_a(42);
    module3.send_message_c(3.14);
    module2.send_message_b("Hello!");
    module3.send_message_d(5, 7);

    return 0;
}
