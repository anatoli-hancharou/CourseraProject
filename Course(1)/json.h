#pragma once
#include"bus.h"
#include <istream>
#include<iostream>
#include <map>
#include <string>
#include <variant>
#include<memory>
#include <vector>
#include<unordered_map>
#include<sstream>
#include<optional>
#include"profile.h"

namespace Json {
    enum class RequestType {
        BUS,
        STOP,
        ROUTE
    };

    struct Request {
        RequestType rtype ;
       /* std::optional<std::string> name;
        std::optional<std::string> from;
        std::optional<std::string> to;*/
        std::string name;
        std::string from;
        std::string to;
        int64_t id = 0;
    };

    class Node : public std::variant<std::vector<Node>,
        std::map<std::string, Node>,
        double,
        bool,
        std::string> {
    public:
        using variant::variant;

        const auto& AsArray() const {
            return std::get<std::vector<Node>>(*this);
        }
        const auto& AsMap() const {
            return std::get<std::map<std::string, Node>>(*this);
        }
        double AsDouble() const {
            return std::get<double>(*this);
        }
        const auto& AsString() const {
            return std::get<std::string>(*this);
        }
        const auto& AsBool() const {
            return std::get<bool>(*this);
        }
    };

    class Document {
    public:
        explicit Document(Node root);
        Document() = default;

        const Node& GetRoot() const;

        
    private:
        Node root;
    };

    std::vector<std::unique_ptr<Request>> FormRequests(const Document& document);

    Document Load(std::istream& input = std::cin);

}