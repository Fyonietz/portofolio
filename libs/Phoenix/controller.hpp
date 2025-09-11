#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include "../json.hpp"

template<typename T>
struct Model {
    using Setter = std::function<void(T&, const nlohmann::json&)>;

    std::unordered_map<std::string, Setter> bindings;

    // Bind a JSON key to a member pointer (basic types only)
    template<typename MemberType>
    Model& bind(const std::string& key, MemberType T::* member) {
        bindings[key] = [member](T& obj, const nlohmann::json& value) {
            if constexpr (std::is_same_v<MemberType, int>) {
                obj.*member = value.get<int>();
            } else if constexpr (std::is_same_v<MemberType, std::string>) {
                obj.*member = value.get<std::string>();
            } else {
                // Add more types as needed
                static_assert(sizeof(MemberType) == 0, "Unsupported type in Model");
            }
        };
        return *this;
    }

    // Parse a JSON array into a vector of objects
    std::vector<T> parse(const nlohmann::json& json_array) const {
        std::vector<T> result;

        for (const auto& item : json_array) {
            T obj{};
            for (const auto& [key, setter] : bindings) {
                if (item.contains(key)) {
                    setter(obj, item[key]);
                }
            }
            result.push_back(obj);
        }

        return result;
    }
    T parse_one(const nlohmann::json& json_obj) const {
    T obj{};
    for (const auto& [key, setter] : bindings) {
        if (json_obj.contains(key)) {
            setter(obj, json_obj[key]);
        }
    }
    return obj;
}
};

