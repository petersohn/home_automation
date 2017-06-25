#ifndef ACTION_HPP
#define ACTION_HPP

#include <Arduino.h>

#include <memory>
#include <vector>

class Action {
public:
    virtual void fire(const String& value) = 0;
};

class Actions {
public:
    Actions(const std::vector<std::unique_ptr<Action>>& actions)
            : actions(actions) {}

    void fire(const String& value) {
        for (const auto& action : actions) {
            action->fire(value);
        }
    }
private:
    const std::vector<std::unique_ptr<Action>>& actions;
};

#endif // ACTION_HPP
