#pragma once
#include<string>
#include<vector>
#include<memory>
#include<unordered_map>
#include<unordered_set>
#include<set>
#include<fstream>
#include"graph.h"

struct BusStop {
	std::string name;
	std::set<std::string> buses_through_stop;
	std::unordered_map<std::string, int> dist_to_stops;
	double latitude_ = 0;
	double longitude_ = 0;
	Graph::VertexId vertex_id = 0;
};

struct BusRoute {
	std::vector<std::shared_ptr<BusStop>> route;
	std::unordered_set<std::string> unique_stops;
	bool is_circle = 0;
	double distance = 0;
	int route_length = 0;
};

