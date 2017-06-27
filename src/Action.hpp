#ifndef ACTION_HPP
#define ACTION_HPP

#include <Arduino.h>

#include <memory>
#include <vector>

class Action {
public:
    virtual void fire(const std::vector<String>& value) = 0;
};

class Actions {
public:
    Actions(const std::vector<std::unique_ptr<Action>>& actions)
            : actions(actions) {}

    void fire(const std::vector<String>& value) {
        for (const auto& action : actions) {
            action->fire(value);
        }
    }
private:
    const std::vector<std::unique_ptr<Action>>& actions;
};

#endif // ACTION_HPP
