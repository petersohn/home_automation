#ifndef COLLECTION_HPP
#define COLLECTION_HPP

#include <algorithm>
#include <iterator>

namespace tools {

template<typename Collection, typename Value>
typename Collection::value_type::second_type const* findValue(
        const Collection& collection,
        const Value& key) {
    using std::begin;
    using std::end;
    auto iterator = std::find_if(begin(collection), end(collection),
            [&key](const typename Collection::value_type& element) {
                return key == element.first;
            });
    if (iterator == end(collection)) {
        return nullptr;
    }
    return &iterator->second;
}

} // namespace tools

#endif // COLLECTION_HPP
