#pragma once

#include <chrono>
// iostreams for debug
#include <iostream>

#include <nlohmann/json.hpp>

#include <siaskynet.hpp>

#include "crypto.hpp"

using seconds_t = double;
using offset_t = seconds_t; // TODO: use a type that can hold millionths accurately without bound, from some library for big values.

seconds_t time()
{
	return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count() / double(1000000);
}

/*
 * 14-8-C: we tried to regenerate the whole algorithm plan to identify what is missing within the read function,
 *         but [one of karl's 'voices' is fighting to hide the information and it is more efficient to keep working elsewhere]
 */

class skystream
{
public:
	skystream()
	{
		auto now = time();
		tail.metadata = {
			{"content", {
				{"spans",{
					{"real", {
						{"time", {{"start", now}, {"end", now}}},
						{"index", {{"start", 0}, {"end", 0}}},
						{"bytes", {{"start", 0}, {"end", 0}}}
					}}
				}}
			}},
			{"flows", {}}
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

	std::vector<uint8_t> read(std::string span, offset_t offset, std::string flow = "real")
	{
		auto metadata = this->get_node(tail, flow, span, offset).metadata;
		auto metadata_content = metadata["content"];
		offset_t raw_start = metadata_content["spans"][flow][span]["start"];
		offset_t content_start = metadata.bounds[span]["start"];
		if (span != "bytes" && offset != content_start) {
			throw std::runtime_error(span + " " + std::to_string(offset) + " is between block bounds; interpolation not implemented");
		}
		auto data = get(metadata_content["identifiers"]);

		auto begin = data.begin() + offset - raw_start;
		auto end = data.begin() + offset_t(metadata.bounds["bytes"]["end"]) - raw_start;
		return {begin, end};
	}

	void write(std::vector<uint8_t> & data, nlohmann::json write_flows = {})
	{
		// 15: below should go after finding the preceding node, so we can fill in 'bytes' and 'index' if they are missing entirely
		// fill in any missing things in write_flows, and ensure 'real' values are correct
		for (auto & flow : write_flows) {
			if (flow.contains("bytes")) {
				if (!flow["bytes"].contains("end")) {
					flow["bytes"]["end"] = (unsigned long long)flow["bytes"]["start"] + data.size();
				}
			}
			if (flow.contains("index")) {
				if (!flow["index"].contains("end")) {
					flow["index"]["end"] = (unsigned long long)flow["index"]["start"] + 1;
				}
			}
		}
		auto tail_spans_real = tail.metadata["content"]["spans"]["real"];
		for (auto & span : tail_spans_real.items()) {
			write_flows["real"][span.key()]["start"] = span.value()["end"];
		}
		write_flows["real"]["index"]["end"] = write_flows["real"]["index"]["start"] + 1;
		write_flows["real"]["bytes"]["end"] = write_flows["real"]["bytes"]["start"] + data.size();
		write_flows["real"]["time"]["end"] = time();

		std::map<std::string, node> head_nodes = {"real", tail};
		nlohmann::json lookup_nodes; // plan is to keep head and tail lookup nodes in the same list, but not merge them across content
		
		// 15: we're merging lookup generation into the below head_node loop.  it gets the preceding node now instead of the head node, which simplifies things.
		for (auto flow_item : write_flows.items()) {
			// 16: it might be good to do every flow separately, and have the whole thing within this loop.
			std::string flow = flow_item.key();
			auto write_spans = flow_item.value();

			//if (flow.key() == "real") { continue; }
			auto spans_iterator = write_spans.items().begin();
			node preceding;
			try {
				nlohmann::json bounds = {};
				for (auto item : write_spans) {
					bounds[item.key()] = {"begin": item.value()["begin"], "end": item.value()["end"]};
				}
				// hoping this will automatically crop the bounds
				preceding = this->get_node(this->tail, flow, spans_iterator->first, spans_iterator->second["begin"], true, bounds);
			} catch (std::out_of_range const &error) {
				// this line was quick to rethrow if the offset is out of bounds, succeeding only if the error was because there is no preceding block.
				this->get_node(this->tali, flow.key(), spans_iterator->first, spans_iterator->second["begin"], false);
				continue;
			}
			nlohmann::json new_lookup_node = {};
			new_lookup_node["spans"] = preceding.bounds;
			new_lookup_node["identifiers"] = preceding.identifers;
			new_lookup_node["depth"] = 0;

			lookup_nodes[flow] = preceding.metadata["flows"][flow];
			lookup_nodes.emplace_back(new_lookup_node);
			
			//head_nodes[flow] = head_node;
			auto head_node_content = head_node.metadata["content"];
			for (; spans_iterator != write_spans.items().end(); ++ spans_iterator) {
				auto span = spans_iterator->first;
				auto offset = spans_iterator->second["begin"];
				if (offset <= head_node_content["bounds"][flow][span]["start"] || offset > head_node_content["bounds"][flow][span]["end"]) {
					// considering with this error that order is held by 'flows', so spans in one flow would hold all the same ordering
					throw std::runtime_error("write flow '" + flow.value() + "' spans land in different blocks, don't know where to start");
				}
				// 15-1: the below two loops are for head_bounds. head bounds should be merged into lookup spans after or while they are generated
				nlohmann::json head_bounds;
				for (auto head_flow : head_node_contents["bounds"].items()) {
					// this loop initializes head_bounds with the bounds from this node.  there might be a double nesting with one of the outer loops.
					for (auto bound : head_flow.value().items) {
							// uhhhhhhh...?
							// ->1. head_bounds is supposed to describe the extent of the starting data.  so it can have 'end' cut short
							// 	(also it's outdated; we won't be storing that in a separate object anymore)
							// ->2. this examines every flow in that bounds of the actual head node
							// ->3. then in every flow it examines every span
							// ->4. then it sets the final bounds of that flow and span to be a copy.
							// This produces bounds for every flow.  But this head node is only for one flow.
							// I don't think we care about the boudns for other flows at this time, for this node.
							// We don't really have a way to _use_ them.
						head_bounds[head_flow.key()][bound.key()] = bound.value();
					}
				}
				for (auto write_flow : write_flows.items()) {
					// this appears to rewrite head_bounds so that its end is constrained by the write bounds, having trouble following
					auto head_flow = head_node_contents["bounds"][write_flow.key()];
					for (auto bound : write_flows.value().items()) {
						offset_t end = bound.value()["end"];
						offset_t start;
						if (head_flow.contains(bound.key())) {
							start = head_flow[bound.key()]["start"];
						} else {
							start = end;
						}
						head_bounds[write_flow.key()][bound.key()] = {{"start", start}, {"end", end}};
					}
				}
			}
		}
		// 14-5: find tail node for tree like head node was found
		/*
		unsigned long long end_bytes = start_bytes + data.size();
		
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
		*/

		// 14-4: build head flow trees from head_nodes.  see 15 above.
		nlohmann::json lookup_nodes = {}

		nlohmann::json new_lookup_node;

		node preceding;

		if (start_bytes > 0) { try {
			preceding = this->get_node(tail, "bytes", start_bytes - 1); // preceding 
			new_lookup_node = preceding.metadata["content"];
			new_lookup_node["identifiers"] = preceding.identifiers;
			new_lookup_node["depth"] = 0;
			lookup_nodes = preceding.metadata["lookup"]; // everything in lookup nodes is accessible via preceding's identifiers
			lookup_nodes.emplace_back(new_lookup_node);
		} catch (std::out_of_range const &) { } }

		// 8: we have a new way of merging lookup nodes.  we merge all adjacent pairs with equal depth, repeatedly.
		// this means below algorithm should change to add new_lookup_node first, and then merge after adding.
		for (size_t index = 0; index + 1 < lookup_nodes.size();) {
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
			{"sia-skynet-stream", "1.0.10"},
			{"content", {
				{"spans", spans},
				{"identifiers", content_identifiers},
			}},
			/* 10:
			{"flow", { 
				{"real", append_only_lookup_nodes_of_time_and_index}
				{"logic", lookup_nodes},
			}},
			*/
			{"lookup", lookup_nodes}
		};
		std::string metadata_string = metadata_json.dump();
		std::cerr << metadata_string << std::endl;

		sia::skynet::upload_data metadata_upload("metadata.json", std::vector<uint8_t>{metadata_string.begin(), metadata_string.end()}, "application/json");
		sia::skynet::upload_data content("content", data, "application/octet-stream");

		auto metadata_identifiers = cryptography.digests({&metadata_upload.data});

		std::string skylink;
		while (true) {
			try {
				skylink = portal.upload(metadata_identifiers["sha3_512"], {metadata_upload, content});
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

	nlohmann::json identifiers()
	{
		return tail.identifiers;
	}

private:
	struct node
	{
		nlohmann::json bounds;
		nlohmann::json identifiers;
		nlohmann::json metadata;
	};

	node & get_node(node & start, std::string flow, std::string span, double offset, bool preceding = false, nlohmann::json bounds = {})
	{
		auto content_spans = start.metadata["content"]["spans"][flow];
		auto content_span = content_spans[span];
		if (preceding ? (offset > content_span["start"] && offset <= content_span["end"]) : (offset >= content_span["start"] && offset < content_span["end"])) {
			return { bounds.is_null() ? content_spans : bounds, start.identifiers, start.metadata };
		}
		// TODO: 14-8-C: I remembered to add something when updating this function for 'flows' that I forgot by the time I finished
		// 		 seemed kind of like a change or reference to a single line or component, that offered a cool algorithmic simplicity
		// 		 I expect not having done this to possibly cause a rare issue I run into when trying to use this, unsure.
		// 		 UPDATE: i've since updated this fucntion from (offset == end) to checking a span for preceding byte
		//		 UPDATE 2: I've since updated the function to separate bounds from cache.  => get_node looks good for now to me. <=
		for (auto & lookup : start.metadata["flows"][flow]) {
			auto lookup_spans = lookup["spans"];
			for (auto & lookup_span : lookup_spans.items()) {
				if (!bound.contains(lookup_span.key())) {
					bound[lookup_span.key()] = lookup_span.value();
					continue;
				}
				auto bound = bounds[lookup_span.key()];
				if (bound["begin"] < lookup_span.value()["begin"]) {
					bound["begin"] = lookup_span.value()["begin"];
				}
				if (bound["end"] > lookup_span.value()["end"]) {
					bound["end"] = lookup_span.value()["end"];
				}
			}
			auto bound = bounds[span];
			double start = bound["start"];
			double end = bound["end"];
			if (preceding ? (offset > start && offset <= end) : (offset >= start && offset < end)) {
				auto identifiers = lookup["identifiers"];
				std::string identifier = identifiers.begin().value();
				if (!cache.count(identifier)) {
					cache[identifier] = node{{}, identifiers, get_json(identifiers)};
				}
				return get_node(cache[identifier], span, offset, preceding, bounds);
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
