#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>

#include <unistd.h>

#include <nlohmann/json.hpp>

#include <siaskynet.hpp>

#include <openssl/conf.h>
#include <openssl/evp.h>
#include <openssl/err.h>

class crypto
{
public:
	crypto()
	{
		ERR_load_crypto_strings();
		OpenSSL_add_all_algorithms();
		mdctx = EVP_MD_CTX_create();
	}
	~crypto()
	{
		EVP_MD_CTX_destroy(mdctx);
		EVP_cleanup();
		CRYPTO_cleanup_all_ex_data();
		ERR_free_strings();
	}
	std::string sha3_512(std::initializer_list<std::vector<uint8_t> const *> data)
	{
		bytes.resize(EVP_MAX_MD_SIZE);

		EVP_DigestInit_ex(mdctx, EVP_sha3_512(), NULL);

		for (auto & chunk : data) {
			EVP_DigestUpdate(mdctx, chunk->data(), chunk->size());
		}

		unsigned int size;
		EVP_DigestFinal_ex(mdctx, bytes.data(), &size);
		bytes.resize(size);

		digest.resize(size * 2);

		static char hex[] = {'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'};

		for (int i = 0; i < size; ++ i) {
			int j = i << 1;
			digest[j] = hex[bytes[i] >> 4];
			digest[j+1] = hex[bytes[i] & 0xf];
		}
		return digest;
	}

private:
	std::vector<uint8_t> bytes;
	std::string digest;
	EVP_MD_CTX * mdctx;
};
double time()
{
	return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count() / double(1000000);
}

int main(int argc, char **argv)
{
	crypto cryptography;
	sia::skynet portal;

	auto start_time = time();
	size_t index = 0;
	size_t offset = 0;
	ssize_t size;

	sia::skynet::upload_data content("content", "", "application/octet-stream");
	sia::skynet::upload_data content_json_upload("content.json", "", "application/json");
	sia::skynet::upload_data lookup_json_upload("lookup.json", "", "application/json");

	content.data.reserve(1024 * 1024);
	content.data.resize(stream_upload.data.capacity());

	nlohmann::json stream_json{
		{"sia-skynet-stream", "1.0.3"},
	};

	std::vector<nlohmann:json> lookup_nodes; // roots and spans to use to index entire stream

	while ((size = read(0, stream_upload.data.data(), stream_upload.data.size()))) {
		auto end_time = time();
		if (size < 0) {
			perror("read");
			return size;
		}
		auto content_sha3_512 = cryptography.sha3_512({&stream_upload.data});
		content.filename = content_sha3_512;
		content.data.resize(size);
		auto content_skylink = portal.upload(content);

		nlohmann::json node = {
			{"spans", {
				{"time", {{"start", start_time}, {"end", end_time}, {"length", end_time - start_time}}},
				{"bytes", {{"start", offset}, {"end", offset + size}, {"length", size}}},
				{"index", {{"start", index},{"end", index}, {"length", 0}}}
			}},
			{"lookup-depth", 0},
			{"content", { // change to lookup for roots
				{"sha3-512", content_sha3_512}
				{"skylink", content_skylink}
			}
		};
		start_time = end_time;

		std::string skylink;

		if (lookup_nodes.back()["depth"] == 0) {
			// we will be using a lookup.json
		} else {
			lookup_nodes.emplace_back(content_json);
			skylink = portal.upload(content.filename, {content, content_json});
		}
		std::string metadata_string = metadata.dump();
		metadata_upload.data = std::vector<uint8_t>(metadata_string.begin(), metadata_string.end());
		std::cout << metadata_string << std::endl;

		std::string skylink = portal.upload(stream_upload.filename, {metadata_upload, stream_upload});
		lookup_nodes.back()["content"]["skylink"] = skylink + "/" + stream_upload.filename;
		lookup_nodes.back()["metadata"] = {
			{"sha3-512", cryptography.sha3_512({&metadata_upload.data})},
			{"skylink", skylink + "/" + metadata_upload.filename}
		};

		stream_upload.data.resize(stream_upload.data.capacity());

		++ index;
		offset += size;

		if (lookup_nodes.size() > 1) {
			auto node_1 = lookup_nodes[lookup_nodes.size()-2];
			auto node_2 = lookup_nodes.back();
			if (node_1["lookup-depth"] == node_2["lookup-depth"]) {
				lookup_nodes.pop();
				lookup_nodes.pop();
				auto start_time = node_1["time"]["start"];
				auto end_time = node_2["time"]["end"];
				auto start_bytes = node_1["bytes"]["start"];
				auto end_bytes = node_2["bytes"]["end"];
				auto start_index = node_1["index"]["start"];
				auto end_index = node_2["index"]["end"];

				auto content_sha3_512_1 = node_1["content"]["sha3-512"];
				auto content_sha3_512_2 = node_2["content"]["sha3-512"];
				auto string_1 = node_1.dump();
				auto string_2 = node_2.dump();
				// from this node we want to be able to find the two others
				// and hash their hashes

				lookup_nodes.emplace_back({
					{"time", {{"start", start_time},{"end", end_time},{"length",end_time - start_time}}},
					{"bytes", {{"start", start_bytes},{"end", end_bytes},{"length",end_bytes - start_bytes}}},
					{"index", {{"start", start_index},{"end", end_index}}},
					{"content", {
						{"sha3-512", cryptography.sha3_512({{content_sha3_512_1.begin(), content_sha3_512_1.end()}, {content_sha3_512_2.begin(), content_sha3_512_2.end()}}}
					}},
					{"lookup-depth", node_1["lookup-depth"] + 1},
					{"lookup", {
						node_1,
						node_2
					}}
					// how to find them?
					// 	leaf nodes have a metadata.skylink field
					// 	in what cases would we recursively add?  ... that would be if we referenced a field we wrote to, or one that contains it
					// 	so if we put their metadata in ours, it grows without boudn unless we remove something
					// 	but we could put their metadata alongside ours
					// where does the link to this come from?
					// let's say we includ euthem in a merkle-roots field
					// to find this root, we need to know the skylink it will be attached to.
					// that means we want to attach skylinks to roots when they are calculated.  note: we just calcualted the skylink for our new leaf.  node_2 has a skylink on it, in its metadata field.
					// each root is on a leaf.  possibly repeatfgedly!
					// 	each root only need be broadcast once.  this root will be broadcast with the next one, so the skylink to find it is different from the one we have.
					//
					//not quickly seeing a way to get an entry in the vector from a reference to its content.  maybe store an index.
					//
					//okay, so we are storing roots that we have already broadcast, because we want to rehash them.
					//but there is a special root: the one that hasn't been broadcast yet.
					//there is only 1.  each time we broadcast, we identify the skylink for the last root.
				});
			}
		}
	}
	std::cout << metadata["history"][0]["metadata"].dump(2) << std::endl;
	return 0;
}

// okay, now on the first post, 'metadata' is non-present
// it is then stored as alookup node with 'metadata' equal to the hash of
// the first post metadata
// when we merge them, the new one has 'metadata' equal to the hash of
// the child documents.
// not ideal i think
// we likely want to hash the same things
//
// what we value is the hash of the stuff actually posted
// so likely we want to keep documents the same, not mutate them
//
// the metadata on something is kinda special
//
// so each post has a metadata doc with its hash in it
// when we refer to it, we refer to its metadata doc and that hash
// this reference, the lookup-node, is very similar to the metadata doc
//
// we may have done it okay
//
// well, when you make your second post, it is immediately merged with the first
// the hash that is posted, its content is only seen in the same document that
// contains it
//
// we don't need to post that hash: it is apparent from the embedded documents
//
// 0 [metadata: hash of content]
// 1 [we don't want to merge off the last lookup-node because it is the metadata]
//
// let's separate metadata from lookup nodes
//
// wrt metadata hashes we want to hash the exact metadata file.  this jsut means
// we include the lookup output etc.
//
// sometimes our last post will be a new root.  sometimes it will merge with
// an old root.
// when it merges with an old root, we'd still like to include information on it.
// that likely means including it, too.
//
// when we get to a doc we want to go other places:
// 1-> the deepest root [i.e. list of roots]
// 2-> right here
// 3-> a child of us
// so we want it easy to look up our children, if we are a root.
// 	when a deepest root is posted it is the only root, it has merged everything
// 	so that should be easy.
// 		so links to roots are individual documents, not lists?
// 			should be
// 		excess roots only happen on non-roots?
// 			the roots list is only for seeking backwards in the stream
// 			it provides random access using the tip as a start.
// 			it's not useful when coming from elsewhere.
// 			meanwhile sometimes we make a root, and it is all we have
// 			that means we will get referenced from elsewhere.
// 			okay so when we make a root we can call it 'root'?
// 			let's just put it at index 0 of lookup-nodes?
// 				when we make a root our whole metadata hash
// 				is going to get used as a reference
//
// 		1
// 			2
// 		3 <- made root
// 				4
// 		5 <- has list of 2 roots, 2 and 5, could be longer
// 			6
// 		7 <- made root
//
//
// 1:
// 	content
// 	metadata
// 3:
// 	content
// 	metadata
// 	lookup-node: 2
// 	lookup-nodes: 2
// 2 is now defined and found by the hash of 3
//
// 1:
// 	content
// 	content.json
// 3:
// 	content
// 	content.json
// 	lookup.json: references 1/content.json and 3/content.json
//
// 	then how do we navigate from 3? we reference it by its lookup, not
// 	its metadata.
// 5:
// 	content
// 	content.json
// 	lookup.json: reference 3/lookup.json and 5/content.json
//
//	content.json will have information on span
//	lookup.json should too, but it will be bigger.
//
//	now i think we could merge content.json and lookup.json
//	but that could make finding 1 a little more confusing from 3
//
//	we work in links to documents/files/json.
//	after 1, the identifier is the link to content.json
//	after 5, the identifier is the link to lookup.json
//
//	we could unify lookup.json and content.json if we
//	let a json file reference either data or a way to look it up.
//	it would need to specify the nature of the link.
//	other issue: how to find content when in a lookup.
//		so if you want to unify content and lookup,
//		you'll want a way to reference the content metadata
//		from the lookup metadata.  maybe include it instead of
//		having a link.  but that will make using it harder.
//	okay, what's arising is getting rid of the content.json and only
//	using the lookup.json.
//		we need a way to reference the leaf when it is time
//		to make a root.
//
//			note when we make a root it actually makes lots of roots.
//			one makes another makes another makes another.
//
// 1
// 	2
// 3
// 		4
// 5
//	6
// 7 <- makes 6 & 4
//
// 		maybe referencing inside documents would help a lot
// 		document: something.json
//		index: 
//			so when we make a root, it is made of its leaves
//			specifically the hash of its hashes
//			we can use that as an index
//
//		so we have uris of documents and also places within them
//
//
//
//	<i seem to be 'losing' this attempt to make productive behavior on goal.
//	 keep engaging workarounds but workaroudn results in further
//	 change-of-mind etc.>
//	 	<not really sure what our competing values are if we are
//	 	 in conflict>
//	 	 	[i think karl was trying to take an opportunity to
//	 	 	 add a very valuable component to world software
//	 	 	 ecosystem.  we're in second side-failure atm.
//	 	 	 small changes were introduced, but very small
//	 	 	 compare to work done and availability, roughly]]
//
//	[we're trying to combine offers-to-store-unlimited-data
//	with ease-of-use roughly.]
//
//		[karl i know you want to contineu trying to do this
//		 because it seems so helpful, but there is a proces goin
//		 on that is halting that at the moment.]
//		 [it would make sense to do a different task]
//
//		 	[sad, invested social failure in this hope.]
//		 	[would have liked to see azia yesterday, go to my
//		 	 meeting today, etc.]
