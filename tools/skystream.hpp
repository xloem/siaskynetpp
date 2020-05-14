#pragma once

#include <chrono>

#include <nlohmann/json.hpp>

#include <siaskynet.hpp>

#include "crypto.hpp"

using seconds_t = double;

seconds_t time()
{
	return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count() / double(1000000);
}

class skystream
{
public:
	skystream()
	{
		auto now = time();
		tail.metadata = {
			{"content", {
				{"spans",{
					//{"real", {
						{"time", {"start", time()}, {"end", time()}},
						{"index", {"start", 0}, {"end", 0}},
						{"bytes", {"start", 0}, {"end", 0}}
					//}}
				}}
			}}//,
			//{"flows", {}}
		};
	}
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

	std::vector<uint8_t> read(std::string span, double offset, std::string flow = "real")
	{
		auto metadata = this->get_node(tail, span, offset).metadata;
		auto metadata_content = metadata["content"];
		double content_start = metadata_content["spans"][span]["start"];
		if (span != "bytes" && offset != content_start) {
			throw std::runtime_error(span + " " + std::to_string(offset) + " is within block span");
		}
		auto data = get(metadata_content["identifiers"]);

		auto begin = data.begin() + offset - content_start;
		auto end = data.begin() + metadata_content["bounds"]["bytes"]["end"] - content_start;
		return {begin, end};
	}

	void write(std::vector<uint8_t> & data, std::string span, double offset)
	{
		seconds_t end_time = time();
		seconds_t start_time = tail.metadata["content"]["spans"]["time"]["end"];
		
		node head_node;
		nlohmann::json head_bounds;
		unsigned long long start_bytes;
		//unsigned long long full_size = data.size(); // let's try to implement by reusing surrounding data
		unsigned long long index = tail.metadata["content"]["spans"]["index"]["end"];
		if (offset == tail.metadata["content"]["spans"][span]["end"]) {
			// append case, no head node to replace
			start_bytes = tail.metadata["content"]["spans"]["bytes"]["end"];
			//full_size = data.size();
		} else {
			nlohmann::json head_node_bounds;
			head_node = this->get_node(this->tail, span, offset);
			auto head_node_content = head_node.metadata["content"];
			double start_head = head_node_content["bounds"][span]["start"];
			start_bytes = head_node_content["bounds"]["bytes"]["start"]; 
			if (offset != start_head) {
				if (span != "bytes") {
					throw std::runtime_error(span + " " + std::to_string(offset) + " is within block span");
				} else {
					start_bytes = offset;
					for (auto bound : head_node_content["bounds"].items()) {
						if (bound.key() == "bytes") {
							head_bounds["bytes"] = {{"start", bound.value()["start"]},{"end", start_bytes}};
						} else {
							head_bounds[bound.key()] = {{"start", bound.value()["start"]},{"end", bound.value()["end"]}};
						}
					}
				}
			}
			//full_size = data.size() + offset - start_head; // full_size is the number of bytes including the beginning bits of head_node
		}
		unsigned long long end_bytes = start_bytes + data.size(); /*full_size*/
		nlohmann::json spans = { // these are the spans of the new write
			{"time", {{"start", start_time},{"end", end_time}}},
			{"bytes", {{"start", start_bytes},{"end", end_bytes}}},
			{"index", {{"start", index}, {"end", index + 1}}}
		};
		node * tail_node;
		nlohmann::json tail_bounds;
		try {
		       	tail_node = &get_node(tail, "bytes", end_bytes);
			auto tail_node_content = tail_node->metadata["content"];
			if (end_bytes != tail_node_content["bounds"]["bytes"]["start"]) {
				for (auto bound : tail_node_content["bounds"].items()) {
						if (bound.key() == "bytes") {
							tail_bounds["bytes"] = {{"start", end_bytes},{"end", bound.value()["end"]}};
						} else {
							tail_bounds[bound.key()] = {{"start", bound.value()["start"]},{"end", bound.value()["end"]}};
						}
				}
			}
		} catch (std::out_of_range const &) {
			tail_node = &tail;
		}

		nlohmann::json lookup_nodes;
		size_t depth = 0;
		nlohmann::json new_lookup_node;
		node preceding;
		lookup_nodes.clear();
		try {
			preceding = this->get_node(tail, "bytes", start_bytes - 1); // preceding 
			new_lookup_node = preceding.metadata["content"];
			new_lookup_node["identifiers"] = preceding.identifiers;
			new_lookup_node["depth"] = 0;
			lookup_nodes = preceding.metadata["lookup"]; // everything in lookup nodes is accessibel via preceding's identifiers
			lookup_nodes.emplace_back(new_lookup_node);
		} catch (std::out_of_range const &) { }

		// 8: we have a new way of merging lookup nodes.  we merge all adjacent pairs with equal depth, repeatedly.
		// this means below algorithm should change to add new_lookup_node first, and then merge after adding.
		for (size_t index = 0; index < lookup_nodes.size() - 1;) {
			auto & current_node = lookup_nodes[index];
			auto & next_node = lookup_nodes[index + 1];
			if (current_node["depth"] == next_node["depth"]) {
				auto next_spans = next_node["spans"];
				for (auto & span : current_node["spans"].items()) {
					auto & current_span = span.value();
					auto & next_span = next_spans[span.key()];
					// 10: we're changing the format to use "flows" of "real" and "logic" as below
					// 		[this change is at the edge of checks for likely-to-finish-task.  this is known.
					// 		 so, no more generalization until something is working and usable.]
					// 		[ETA for completion has doubled.]
					// 			[we have other tasks we want to do.]
					// 				[considering undoing proposed change.]
					// 					[okay, demand-to-make-more-general: you don't seem to have the capacity to support
					// 					 that.  i am forcing you to not have it be more general.]
					// 		[we will implement a test before changing further.  thank you all for the relation.]
					// {
					//	'sia-skynet-stream': "1.1.0_debugging",
					// 	content: {
					// 		identifiers: {important-stuff},
					// 		spans: {
					// 			{real: {time:}},
					// 			{logic: {bytes:,index:}}
					// 		}
					//	},
					//	flows: { // 'lookup' gets translated to 'flows'
					//		real: [
					//			{
					//				identifiers: {important-stuff},
					//	 			spans: {
					//	 				{real: {time:}},
					//	 				{logic: {bytes:,index:}}
					//	 			}
					//			}, ...
					//		], // we're considering updating this to not store multiple copies of the lookup data for each order.
					//		   // but we're noting with logic-space changes, the trees may refer to different nodes.  maybe leave for later.
					//		   	// for streams, reuse is helpful.
					//		logic: [
					//			{
					//				identifiers: {important-stuff},
					//	 			spans: {
					//	 				{real: {time:}},
					//	 				{logic: {bytes:,index:}}
					//	 			}
					//			}, ...
					//		]
					//	}
					//}     

					// NOTE: we need to update identifiers of lookup_nodes to point to something that contains both
						    		
					auto & current_end = current_span["end"];
					auto & next_end = next_span["end"];
					assert (current_end == next_span["start"]);
					if (current_end < next_end) { current_end = next_end; }
				}
				current_node["identifiers"] = preceding.identifiers;
				current_node["depth"] = (unsigned long long)current_node["depth"] + 1;
				lookup_nodes.erase(index + 1);
			} else {
				++ index;
			}
		}
		// 9: this is old implementation, below loop.  9: above loop is wip new implementation
		/*
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
		*/


		// end: we can make a new tail metadata node that indexes everything afterward.  it can even have tree nodes if desired.
		// 5: remaining before testing: build lookup nodes using three more sources in 1-2-3 order
		//  1. if !head_bounds.is_null(), then add a lookup reference for head
			// note: we can't merge this lookup node with previous because it is the only one with a link to its content.
		if (!head_bounds.is_null()) {
			lookup_nodes.emplace_back(nlohmann::json{
				{"identifiers", head_node.identifiers},
				{"spans", head_bounds},
				{"depth", 0} // now .... will this get merged if we append to tail after this?
						// when appending we assuming depth reduces forward, which is no longer true.
						// we probably want to reduce depth within as well as forward.
			});
		}

		//  2. if !tail_bounds.is_null(), then add a lookup reference for tail
		//  3. reference node hierarchies until real tail to complete reference to rest of doc

		auto content_identifiers = cryptography.digests({&data});
		nlohmann::json metadata_json = {
			{"sia-skynet-stream", "1.0.10_debugging"},
			{"content", {
				{"spans", spans},
				{"identifiers", content_identifiers},
			}},
			/* 10:
			{"flow", { 
				{"logical", lookup_nodes},
				{"creation", append_only_lookup_nodes_of_time_and_index}
			}},
			*/
			{"lookup", lookup_nodes}
		};
		std::string metadata_string = metadata_json.dump();

		sia::skynet::upload_data metadata_upload("metadata.json", std::vector<uint8_t>{metadata_string.begin(), metadata_string.end()}, "application/json");
		sia::skynet::upload_data content("content", data, "application/octet-stream");

		// CHANGE 3C: let's try to reuse all surrounding data using the new 'bounds' attribute
		// 3C: TODO: we want to insert into content from head_node if we are doing a midway-write (full_size above).  we could also split the write into two.

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

		// if we want to supporto threading we'll likely need a lock around this whole function (not just the change to tail)
		tail.identifiers = metadata_identifiers;
		tail.metadata = metadata_json;
	}

	std::map<std::string,std::pair<double,double>> block_spans(std::string span, double offset)
	{
		auto metadata = this->get_node(tail, span, offset).metadata;
		std::map<std::string,std::pair<double,double>> result;
		for (auto & content_span : metadata["content"]["bounds"].items()) {
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

	node & get_node(node & start, std::string span, double offset, nlohmann::json bounds = {})
	{
		auto content_spans = start.metadata["content"]["spans"];
		auto content_span = content_spans[span];
		if (offset >= content_span["start"] && offset < content_span["end"]) {
			start.metadata["content"]["bounds"] = bounds.is_null() ? content_spans : bounds;
			return start;
		}
		for (auto & lookup : start.metadata["lookup"]) {
			auto lookup_spans = lookup["spans"];
			for (auto & bound : bounds.items()) {
				if (!lookup_spans.contains(bound.key())) { continue; }
				auto bound_span = lookup_spans[bound.key()];
				if (bound.value()["begin"] > bound_span["begin"]) {
					bound_span["begin"] = bound.value()["begin"];
				}
				if (bound.value()["end"] < bound_span["end"]) {
					bound_span["end"] = bound.value()["end"];
				}
			}
			auto lookup_span = lookup["spans"][span];
			double start = lookup_span["start"];
			double end = lookup_span["end"];
			if (offset >= start && offset < end) {
				auto identifiers = lookup["identifiers"];
				std::string identifier = identifiers.begin().value();
				if (!cache.count(identifier)) {
					cache[identifier] = node{identifiers, get_json(identifiers)};
				}
				return get_node(cache[identifier], span, offset, lookup_spans);
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

	nlohmann::json lookup_nodes(node & source, nlohmann::json & bounds)
	{
		// to do this right, consider that source's content may be in the middle of its lookups.  so you want to put it in the right spot.
	}

	sia::skynet portal;
	crypto cryptography;
	node tail;
	std::unordered_map<std::string, node> cache;
};

/*
so: mid-stream writing.
we'll want to write a lookup list that lets us find all the stuff after the stream.
let's imagine a use-case scenariio where we are always mid-stream writing.
we'll want to reuse tree roots to find data.

	we have an option of reusing tree roots that have the wrong history.
	this would expand the meaning of 'span' to mean that things outside that span, that _could_ be found from the reference, are just plain wrong.
	this would save us a lot of space and time wrt metadata, so let's do it.

		that means that all the tail links that are after our range can be reused.
 
			then we have a range of stuff that is after our write, but before the last accurate chunk.
				the content documents of most of them are accurate, so we can build 1 or more new chunks to wrap those
	okay, so considering spans as truncations makes most of it easy.
	then, we have one document where only part of the data is accurate.
	what we were planning to do at the start was to extend our data and reupload it.
		let's consider not reuploading unmodified data a bit.

		since the system could handle hyperlarge chunks, it would be good reuse existing data.
		we would shrink the span.
		we would have to modify fucntions to handle reading the spans properly.
			this means tracing the flow of read into subspans.


			okay we're working on reuse of mid-data
				->problem: if we adjust thse spans in the content field, the document will not read right.  we'd need to add an offset, right?
					When reading we _have_ an offset.  we just need to get the read length right.  It doesn't seem that hard.
						we'll also retrieving spans
							[NOTE: WE ARE NOT INSERTING.  ONLY REPLACING.  SO OFFSETS STAY ACCURATE.]
							let's store this as a TODO for now.  reading more important than writing.
								okay considering this further.  we could have a content-type specificaly for this situation.
								it references content from another document, by an offsset.
								it would be a modification to the {"content", {}} structure.
								Maybe it could be a bare field, "byte-offset", "byte-length"
									this would mean changing the read fucntion, but not by very much

										we could make bare metadata documents
										or we could put the content reference in the lookup table
											maybe we could do span-limiting from the lookup table only if depth=0
												yeah i think that would work [small chance it won't]
												it means read() needs to process spans from the deepest lookup
												table, not the metadata associated with the document.
			rewriting get_node to respect bounds.
note: when get-node is called first, it doesn't need to respect any bounds.  there is no larger context.
		[but note that from write() it is called on its own return value to find preceding node from head_node, a middle-node]
			[possible edge cases might be fixable by mutating the cache when we adjust bounds]
 */
