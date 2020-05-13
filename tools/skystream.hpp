#pragma once

class skystream
{
public:
	skystream(std::string way, std::string link)
	{
		std::vector<uint8_t> data;
		tail.metadata = get_json({{way,link}});
		tail.identifiers = cryptography.digests({&data});
		tail.identifiers[way] = link;
	}
	skystream(nlohmann::json identifiers)
	: tail{identifiers, get_json(identifiers)}
	{ }

	std::vector<uint8_t> read(std::string span, double offset)
	{
		auto metadata = this->get_node(tail, span, offset).metadata;
		double content_start = metadata["content"]["spans"][span]["start"];
		if (span != "bytes" && offset != content_start) {
			throw std::runtime_error(span + " " + std::to_string(offset) + " is within block span");
		}
		auto data = get(metadata["content"]["identifiers"]);
		return {data.begin() + content_start - offset, data.end()};
	}

	void write(std::vector<uint8_t> & data, std::pair<double,double> time, std::string span, double offset)
	{
		auto head_node = this->get_node(this->tail, span, offset);
		unsigned long long start_bytes = head_node.metadata["content"]["spans"]["bytes"]["start"];
		double start_head = head_node.metadata["content"]["spans"][span]["start"];
		auto full_size = data.size() + offset - start_head;
		unsigned long long index = head_node.metadata["content"]["spans"]["index"]["start"];
		if (span != "bytes" && offset != start_head) {
			throw std::runtime_error(span + " " + std::to_string(offset) + " is within block span");
		}
		nlohmann::json spans = { // these are the spans of the new write
			{"time", {{"start", time.first},{"end", time.second}}},
			{"bytes", {{"start", start_bytes},{"end", start_bytes + full_size}}},
			{"index", {{"start", index}, {"end", index + 1}}}
		};
		node * tail_node;
		try {
		       	tail_node = &get_node(tail, "bytes", start_bytes + full_size);
		} catch (std::out_of_range const &) {
			tail_node = &tail;
		}

		nlohmann::json lookup_nodes;
		size_t depth = 0;
		nlohmann::json new_lookup_node;
		lookup_nodes.clear();
		try {
			auto preceding = this->get_node(head_node, "index", index - 1);
			new_lookup_node = preceding.metadata["content"];
			new_lookup_node["identifiers"] = preceding.identifiers;
			lookup_nodes = preceding.metadata["lookup"];
		} catch (std::out_of_range const &) { }

		if (new_lookup_node) {
			while (lookup_nodes.size() && lookup_nodes.back()["depth"] == depth) {
				auto & back = lookup_nodes.back();
				auto & back_spans = back["spans"];
				for (auto & span : new_lookup_node["spans"].items()) {
					auto start = back_spans[span.key()]["start"];
					span.value()["start"] = start;
				}
				auto end = lookup_nodes.end();
				-- end;
				lookup_nodes.erase(end);

				++ depth;
			}
			new_lookup_node["depth"] = depth;
			lookup_nodes.emplace_back(new_lookup_node);
		}

		// 4: write the data out (also see 2B)
		auto content_identifiers = cryptography.digests({&data});
		nlohmann::json metadata_json = {
			{"sia-skynet-stream", "1.0.10_debugging"},
			{"content", {
				{"spans", spans},
				{"identifiers", content_identifiers},
			}},
			{"lookup", lookup_nodes}
		};
		std::string metadata_string = metadata_json.dump();

		sia::skynet::upload_data metadata_upload("metadata.json", std::vector<uint8_t>{metadata_string.begin(), metadata_string.end()}, "application/json");
		sia::skynet::upload_data content("content", data, "application/octet-stream");

		auto metadata_identifiers = cryptography.digests({&metadata_upload.data});

		std::string skylink;
		while (true) {
			try {
				skylink = portal.upload(metadata_identifiers["sha3_12"], {metadata_upload, content});
				break;
			} catch(std::runtime_error const & e) {
				std::cerr << e.what() << std::endl;
				continue;
			}
		}
		metadata_identifiers["skylink"] = skylink + "/" + metadata_upload.filename;


		// 4B: note we need to provide a node for our metadata, after upload

		
		// 5: update our tail link
		// end: we can make a new tail metadata node that indexes everything afterward.  it can even have tree nodes if desired.

	}

	std::map<std::string,std::pair<double,double>> block_spans(std::string span, double offset)
	{
		auto metadata = this->get_node(tail, span, offset).metadata;
		std::map<std::string,std::pair<double,double>> result;
		for (auto & content_span : metadata["content"]["spans"].items()) {
			auto span = content_span.key();
			result[span].second = content_span.value()["end"];
			result[span].first = content_span.value()["start"];
		}
		return result;
	}

	std::pair<double,double> block_span(std::string span, double offset)
	{
		return block_spans(span, offset)[span];
	}

	std::map<std::string,std::pair<double,double>> spans()
	{
		std::map<std::string,std::pair<double,double>> result;
		for (auto & content_span : tail.metadata["content"]["spans"].items()) {
			auto span = content_span.key();
			result[span].second = content_span.value()["end"];
			result[span].first = content_span.value()["start"];
		}
		for (auto & lookup : tail.metadata["lookup"]) {
			for (auto & lookup_span : lookup["spans"].items()) {
				auto span = lookup_span.key();
				double point = lookup_span.value()["start"];
				if (point < result[span].first) {
					result[span].first = point;
				}
			}
		}
		return result;
	}

	std::pair<double,double> span(std::string span)
	{
		return spans()[span];
	}

	std::map<std::string,double> lengths()
	{
		std::map<std::string,double> result;
		for (auto & span : spans()) {
			result[span.first] = span.second.second - span.second.first;
		}
		return result;
	}

	double length(std::string span)
	{
		return lengths()[span];
	}

private:
	struct node
	{
		nlohmann::json identifiers;
		nlohmann::json metadata;
	};

	node & get_node(node & start, std::string span, double offset)
	{
		auto content_span = start.metadata["content"]["spans"][span];
		if (offset >= content_span["start"] && offset < content_span["end"]) {
			return start;
		}
		for (auto & lookup : start.metadata["lookup"]) {
			auto lookup_span = lookup["spans"][span];
			double start = lookup_span["start"];
			double end = lookup_span["end"];
			if (offset >= start && offset < end) {
				auto identifiers = lookup["identifiers"];
				std::string identifier = identifiers.begin().value();
				if (!cache.count(identifier)) {
					cache[identifier] = node{identifiers, get_json(identifiers)};
				}
				return get_node(cache[identifier], span, offset);
			}
		}
		throw std::out_of_range(span + " " + std::to_string(offset) + " out of range");
	}

	nlohmann::json get_json(nlohmann::json identifiers, std::vector<uint8_t> * data = nullptr)
	{
		auto data_result = get(identifiers);
		if (data) { *data = data_result; }
		auto result = nlohmann::json::parse(data_result);
		// TODO improve (refactor?), hardcodes storage system and is slow due to 2 requests for each chunk
		std::string skylink = identifiers["skylink"];
		skylink.resize(52); skylink += "/content";
		result["content"]["identifiers"]["skylink"] = skylink;
		return result;
	}

	std::vector<uint8_t> get(nlohmann::json identifiers)
	{
		auto skylink = identifiers["skylink"];
		std::vector<uint8_t> result;
		while (true) {
			try {
				result = portal.download(skylink).data;
				break;
			} catch(std::runtime_error const & e) {
				std::cerr << e.what() << std::endl;
				continue;
			}
		}
		auto digests = cryptography.digests({&result});
		for (auto & digest : digests.items()) {
			if (identifiers.contains(digest.key())) {
				if (digest.value() != identifiers[digest.key()]) {
					throw std::runtime_error(digest.key() + " digest mismatch.  identifiers=" + identifiers.dump() + " digests=" + digests.dump());
				}
			}
		}
		return result;
	}

	sia::skynet portal;
	crypto cryptography;
	node tail;
	std::unordered_map<std::string, node> cache;
};
