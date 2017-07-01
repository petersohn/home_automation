#ifndef ACTION_HPP
#define ACTION_HPP

#include <memory>
#include <vector>

#include <Arduino.h>

class Action {
public:
    virtual void fire(const std::vector<String>& value) = 0;
    virtual ~Action() {}
};

class Actions {
public:
    Actions(const std::vector<std::unique_ptr<Action>>& actions)
            : actions(actions) {}

    void fire(const std::vector<String>& value) {
        if (value.empty()) {
            return;
        }
        for (const auto& action : actions) {
            action->fire(value);
        }
    }
private:
    const std::vector<std::unique_ptr<Action>>& actions;
};

#endif // ACTION_HPP