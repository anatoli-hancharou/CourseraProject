#include "json.h"

using namespace std;

namespace Json {

    Document::Document(Node root) : root(move(root)) {
    }

    const Node& Document::GetRoot() const {
        return root;
    }

    Node LoadNode(istream& input);

    Node LoadArray(istream& input) {
        vector<Node> result;

        for (char c; input >> c && c != ']'; ) {
            if (c != ',') {
                input.putback(c);
            }
            result.push_back(LoadNode(input));
        }

        return Node(move(result));
    }

    Node LoadDouble(istream& input) {
        double d_result;
        input >> d_result;
        return Node(d_result);
    }

    Node LoadString(istream& input) {
        string line;
        getline(input, line, '"');
        return Node(move(line));
    }

    Node LoadBool(istream& input) {
        string s_circle;
        getline(input, s_circle, 'e');
        return s_circle.find('t') != string::npos ? Node(true) : Node(false);
    }

    Node LoadDict(istream& input) {
        map<string, Node> result;

        for (char c; input >> c && c != '}'; ) {
            if (c == ',') {
                input >> c;
            }

            string key = LoadString(input).AsString();
            input >> c;
            result.insert({move(key), LoadNode(input)});
        }

        return Node(move(result));
    }

    Node LoadNode(istream& input) {
        char c;
        input >> c;

        if (c == '[') {
            return LoadArray(input);
        }
        else if (c == '{') {
            return LoadDict(input);
        }
        else if (c == '"') {
            return LoadString(input);
        }
        else if (c == 't' or c == 'f') {
            input.putback(c);
            return LoadBool(input);
        }
        else {
            input.putback(c);
            return LoadDouble(input);
        }
    }

    Document Load(istream& input ) {
         return Document{ LoadNode(input) };
    }

    vector<unique_ptr<Request>> FormRequests(const Document& document){
        vector<unique_ptr<Request>> v_out;
        const auto& root = document.GetRoot();
        auto& requests = root.AsMap();
        for (auto& stat_requests : requests.at("stat_requests").AsArray()) {
            auto req_ptr = make_unique<Request>();
            Request& req = *req_ptr;
            const auto& stats = stat_requests.AsMap();
            if (stats.at("type").AsString() == "Bus") {
                req = { .rtype = RequestType::BUS,
                    .name = stats.at("name").AsString(),
                    .id = static_cast<int64_t>(stats.at("id").AsDouble()) };
            }
            else if (stats.at("type").AsString() == "Stop") {
                req = { .rtype = RequestType::STOP,
                    .name = stats.at("name").AsString(),
                    .id = static_cast<int64_t>(stats.at("id").AsDouble()) };
            }
            else {
                req = { .rtype = RequestType::ROUTE,
                  .from = stats.at("from").AsString(),
                  .to = stats.at("to").AsString(),
                  .id = static_cast<int64_t>(stats.at("id").AsDouble()) };
            }
            v_out.push_back(move(req_ptr));
        }
        return move(v_out);
    }
}