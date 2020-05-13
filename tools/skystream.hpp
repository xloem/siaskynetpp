#pragma once

class skystream
{
public:
	skystream(std::string way, std::string link)
	: tail(get_json({{way,link}}))
	{
	}
	skystream(nlohmann::json metadata)
	: tail(metadata)
	{ }

	std::vector<uint8_t> read(std::string span, double offset)
	{
		auto metadata = this->metadata(tail, span, offset);
		double content_start = metadata["content"]["spans"][span]["start"];
		if (span != "bytes" && offset != content_start) {
			throw std::runtime_error(span + " " + std::to_string(offset) + " is within block span");
		}
		auto data = get(metadata["content"]["identifiers"]);
		return {data.begin() + content_start - offset, data.end()};
	}

	void write(std::vector<uint8_t> & data, std::pair<double,double> time, std::string span, double offset)
	{
		auto head = this->metadata(this->tail, span, offset);
		unsigned long long start_bytes = head["content"]["spans"]["bytes"]["start"];
		double start_head = head["content"]["spans"][span]["start"];
		auto full_size = data.size() + offset - start_head;
		unsigned long long index = head["content"]["spans"]["index"]["start"];
		if (span != "bytes" && offset != start_head) {
			throw std::runtime_error(span + " " + std::to_string(offset) + " is within block span");
		}
		nlohmann::json spans = {
			{"time", {{"start", time.first},{"end", time.second}}},
			{"bytes", {{"start", start_bytes},{"end", start_bytes + full_size}}},
			{"index", {{"start", index}, {"end", index + 1}}}
		};
		nlohmann::json * tail;
		try {
		       	tail = &this->metadata(this->tail, "bytes", start_bytes + full_size);
		} catch (std::out_of_range const & error) {
			tail = &this->tail;
		}

		// TODO
	}

	std::map<std::string,std::pair<double,double>> block_spans(std::string span, double offset)
	{
		auto metadata = this->metadata(tail, span, offset);
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
		for (auto & content_span : tail["content"]["spans"].items()) {
			auto span = content_span.key();
			result[span].second = content_span.value()["end"];
			result[span].first = content_span.value()["start"];
		}
		for (auto & lookup : tail["lookup"]) {
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
	nlohmann::json & metadata(nlohmann::json & node, std::string span, double offset)
	{
		auto content_span = node["content"]["spans"][span];
		if (offset >= content_span["start"] && offset < content_span["end"]) {
			return node;
		}
		for (auto & lookup : node["lookup"]) {
			auto lookup_span = lookup["spans"][span];
			double start = lookup_span["start"];
			double end = lookup_span["end"];
			if (offset >= start && offset < end) {
				auto identifiers = lookup["identifiers"];
				std::string identifier = identifiers.begin().value();
				if (!cache.count(identifier)) {
					cache[identifier] = get_json(identifiers);
				}
				return metadata(cache[identifier], span, offset);
			}
		}
		throw std::out_of_range(span + " " + std::to_string(offset) + " out of range");
	}

	nlohmann::json get_json(nlohmann::json identifiers)
	{
		auto result = nlohmann::json::parse(get(identifiers));
		// TODO improve (refactor?), hardcodes storage system and slow due to 2 requests for each chunk
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
	nlohmann::json tail;
	std::unordered_map<std::string, nlohmann::json> cache;
};
