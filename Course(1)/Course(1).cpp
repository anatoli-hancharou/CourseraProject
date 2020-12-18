#include<iostream>
#include<string>
#include<memory>
#include<vector>
#include<unordered_map>
#include<sstream>
#include<algorithm>
#include<cmath>
#include<iomanip>

#include"test_runner.h"
#include"json.h"
#include"profile.h"
#include"router.h"

#define Pi 3.1415926535

using namespace std;

using BusStopPtr = shared_ptr<BusStop>;
using BusRoutePtr = unique_ptr<BusRoute>;

double Rads(double degree) {
	return degree * Pi / 180.;
}

double DistanceBetweenStops(const BusStop* lhs, const BusStop* rhs) {
	return acos(sin(Rads(lhs->latitude_)) * sin(Rads(rhs->latitude_)) +
		cos(Rads(lhs->latitude_)) * cos(Rads(rhs->latitude_)) *
		cos(fabs(Rads(lhs->longitude_) - Rads(rhs->longitude_)))) * 6371000;
}

class TransportDataBase {
public:
	using Bus = string;
	
	string GetRouteInfo(const Json::Request* bus_req) const {
		stringstream answer;
		if (auto route = data_.find(bus_req->name); route != data_.end()) {
			auto broute = *route->second;
			answer << "\"route_length\": " << fixed << setprecision(6)
				<< broute.route_length << ",\n"
				<< "\"request_id\": " << bus_req->id << ",\n"
				<< "\"curvature\": " << broute.route_length / broute.distance << ",\n"
				<< "\"stop_count\": ";
			if (broute.is_circle) {
				answer << broute.route.size() << ",\n";
			}
			else {
				if (broute.route.size() != 0) {
					answer << (broute.route.size() * 2 - 1) << ",\n";
				}
				else {
					answer << broute.route.size()<< ",\n";
				}
			}
			answer << "\"unique_stop_count\": " << broute.unique_stops.size();
		}
		else {
			answer << "\"request_id\": " << bus_req->id << ",\n"
				<< "\"error_message\": \"not found\"";
		}
		return answer.str();
	}
	string GetStopInfo( const Json::Request* stop_req) const {
		stringstream answer;
		if (auto stop = stops_.find(stop_req->name); stop == stops_.end()) {
			answer << "\"request_id\": " << stop_req->id << ",\n"
				<< "\"error_message\": \"not found\""; 
		}
		else {
			answer << "\"buses\": [";
			for (auto it = stop->second->buses_through_stop.begin();
				it != stop->second->buses_through_stop.end(); it++) {
				answer << "\n\"" << *it << "\"";
				if (it != prev(stop->second->buses_through_stop.end())) {
					answer << ",";
				}
			}
			answer << "\n],\n" << "\"request_id\": " << stop_req->id;
		}
		return answer.str();
	}

	string FindRouteInfo(Json::Request* req, const Graph::Router<double>& route) const {
		stringstream answer;
		if (auto route_ = route.BuildRoute(stops_.at(req->from)->vertex_id, stops_.at(req->to)->vertex_id); 
			route_ != nullopt) {

			answer << "\"total_time\": " << route_->weight / 60 << ",\n"
				   << "\"items\": [";
			for (size_t i = 0; i < route_->edge_count; ++i) {
				auto edge = graph_->GetEdge(route.GetRouteEdge(route_->id, i));
				auto time = edge.weight / 60.;
				answer << "\n{\n" << "\"time\": " << wait_time << ",\n"
					<< "\"stop_name\": \"" << vertex_ids.at(edge.from)->name << "\",\n"
					<< "\"type\": \"Wait\"\n},"
					<< "\n{\n" << "\"time\": " << (edge.weight / 60.) - wait_time << ",\n"
					<< "\"bus\": \"" << edge.route_name << "\",\n"
					<< "\"span_count\": " << edge.sapn_count << ",\n"
					<< "\"type\": \"Bus\"\n}";
				if (i != route_->edge_count - 1) {
					answer << ',';
				}
			}
			answer << "\n],\n" << "\"request_id\": " << req->id;
		}
		else {
			answer << "\"request_id\": " << req->id << ",\n"
				<< "\"error_message\": \"not found\"";
		}

		return answer.str();
	}

	void GetDataFromJson(const Json::Document& document) {
		auto& requests = document.GetRoot().AsMap();
		{
			const auto& bus_stats = requests.at("routing_settings").AsMap();
			bus_velocity = static_cast<size_t>(bus_stats.at("bus_velocity").AsDouble());
			wait_time = static_cast<size_t>(bus_stats.at("bus_wait_time").AsDouble());
		}
		vector<const Json::Node*> buses;
		int v_id = 0;//
		for (const auto& base_requests : requests.at("base_requests").AsArray()) {
			const auto& stats = base_requests.AsMap();
			if (stats.at("type").AsString() == "Stop") {
				auto bus_stop_ptr = make_shared<BusStop>();
				BusStop& bs = *bus_stop_ptr;
				bs.latitude_ = stats.at("latitude").AsDouble();
				bs.longitude_ = stats.at("longitude").AsDouble();
				bs.name = stats.at("name").AsString();
				bs.vertex_id = v_id++;///
				vertex_ids[v_id - 1] = bus_stop_ptr;
				for (const auto& dists : stats.at("road_distances").AsMap()) {
					bs.dist_to_stops.insert({ dists.first,
						static_cast<int>(dists.second.AsDouble()) });
				}
				stops_.insert({ bs.name, move(bus_stop_ptr) });			
			}
			else {
				buses.push_back(&base_requests);
			}
		}
		graph_ = Graph::DirectedWeightedGraph<double>(v_id);
		for (auto& base_requests : buses) {
			auto& stats = base_requests->AsMap();
			if (stats.at("type").AsString() == "Bus") {
				auto route_ptr = make_unique<BusRoute>();
				BusRoute& br = *route_ptr;
				br.is_circle = stats.at("is_roundtrip").AsBool();
				for (auto& stop : stats.at("stops").AsArray()) {
					br.route.push_back({ stops_.at(stop.AsString()) });
					br.unique_stops.insert(stop.AsString());
					stops_[stop.AsString()]->buses_through_stop.insert(stats.at("name").AsString());
				}
				FindDistance(route_ptr.get(), route_ptr->is_circle);
				FindRouteLength(route_ptr.get(), route_ptr->is_circle);
				data_.insert({ stats.at("name").AsString(), move(route_ptr) });
			}
		}
		BuildGraph();
	}
	const Graph::DirectedWeightedGraph<double>& GetGraph() const  {
		return *graph_;
	}

private:
	void BuildGraph() {
		for (const auto& route : data_) {
			auto& route_p = route.second->route;
			if (route.second->is_circle) {
				for (size_t i = 0; i < route_p.size() - 1; i++) {
					int dist = 0, span_count = 1;
					for (size_t j = i; j < route_p.size() - 2 + i && j + 1 < route_p.size(); j++, ++span_count) {
						dist += LengthBetweenStops(route_p[j]->name, route_p[j + 1]->name);
						graph_->AddEdge({ route_p[i]->vertex_id, route_p[j + 1]->vertex_id,
							(dist / ((int64_t)bus_velocity * 5 / 18.)) + wait_time * 60.,
							route.first, span_count});
					}
				}
			}
			else {
				vector<BusStopPtr> new_stops(route_p.begin(), route_p.end());
				for (auto it = route_p.rbegin() + 1; it != route_p.rend(); it++) {
					new_stops.push_back(*it);
				}
				for (size_t i = 0; i < new_stops.size() - 1; i++) {
					int dist = 0, span_count = 1;
					for (size_t j = i; j < new_stops.size() - 2 + i && j + 1 < new_stops.size(); j++, ++span_count) {
						dist += LengthBetweenStops(new_stops[j]->name, new_stops[j + 1]->name);
						graph_->AddEdge({ new_stops[i]->vertex_id, new_stops[j + 1]->vertex_id,
							(dist / ((int64_t)bus_velocity * 5 / 18.)) + wait_time * 60.,
							route.first, span_count});
					}
				}
			}
		}
	}
	double FindDistance(BusRoute* br, bool is_circle = 0) const {
		if (br->route.size() > 1) {
			for (auto it = br->route.begin(); it != br->route.end() - 1; ++it) {
				br->distance += DistanceBetweenStops(
					it->get(),
					(it + 1)->get());
			}
			if (!is_circle) {
				br->distance *= 2;
			}
		}
		return br->distance;
	}
	int FindRouteLength(BusRoute* br, bool is_circle = 0) const {
		if (br->route.size() > 1) {
			if (is_circle) {
				for (auto it = br->route.begin(); it != br->route.end() - 1; ++it) {
					br->route_length += LengthBetweenStops(
						it->get()->name, (it + 1)->get()->name);
				}
			}
			else {
				for (auto it = br->route.begin(); it != br->route.end() - 1; ++it) {
					br->route_length += LengthBetweenStops(
						it->get()->name, (it + 1)->get()->name);
					br->route_length += LengthBetweenStops(
						(it + 1)->get()->name, it->get()->name);
				}
			}	
		}
		return br->route_length;
	}
	
	int LengthBetweenStops(const string& fst_stop, const string& sec_stop) const {
		int result = 0;
		auto& dis_to_fst = stops_.at(fst_stop)->dist_to_stops;
		auto& dis_to_sec = stops_.at(sec_stop)->dist_to_stops;
		if (dis_to_fst.find(sec_stop) != dis_to_fst.end()) {
			result += dis_to_fst.at(sec_stop);
		}
		else {
			result += dis_to_sec.at(fst_stop);			
		}
		return result;
	}
	optional<Graph::DirectedWeightedGraph<double>> graph_;
	unordered_map<Bus, BusRoutePtr> data_;
	unordered_map<string, BusStopPtr> stops_;
	unordered_map<Graph::VertexId, BusStopPtr> vertex_ids;
	size_t bus_velocity = 0;
	size_t wait_time = 0;
};

void PrintJson(const vector<string>& answers) {
	cout << "[";
	for (auto it = answers.begin(); it != answers.end(); ++it) {
		cout << "\n{\n";
		cout << *it;
		cout << "\n}";
		if (it != (answers.end() - 1)) {
			cout << ',';
		}
	}
	cout << "\n]";
}

void ReadRequests(const TransportDataBase& base, vector<unique_ptr<Json::Request>>& v) {
	vector<string> answers;
	Graph::Router router(base.GetGraph());
	for (auto it = v.begin(); it != v.end(); ++it) {
		if (it->get()->rtype == Json::RequestType::BUS) {
			answers.push_back(base.GetRouteInfo(it->get()));
		}
		else if(it->get()->rtype == Json::RequestType::STOP) {
			answers.push_back(base.GetStopInfo(it->get()));
		}
		else {
			answers.push_back(base.FindRouteInfo(it->get(), router));
		}
		if (it != v.end() - 1) {
		}
	}
	PrintJson(answers);
}

int main() {
	cin.tie(nullptr);
	ios_base::sync_with_stdio(false);
	TransportDataBase td;
	auto doc = Json::Load();
	vector<unique_ptr<Json::Request>> a;
	td.GetDataFromJson(doc);
	auto request = Json::FormRequests(doc);
	ReadRequests(td, request);
	return 0;
}
