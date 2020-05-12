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
	std::string digest(std::initializer_list<std::vector<uint8_t> const *> data, decltype(EVP_sha3_512()) algorithm)
	{
		static thread_local std::string result;
		static thread_local std::vector<uint8_t> bytes;
		bytes.resize(EVP_MAX_MD_SIZE);

		EVP_DigestInit_ex(mdctx, algorithm, NULL);

		for (auto & chunk : data) {
			EVP_DigestUpdate(mdctx, chunk->data(), chunk->size());
		}

		unsigned int size;
		EVP_DigestFinal_ex(mdctx, bytes.data(), &size);
		bytes.resize(size);

		result.resize(size * 2);

		static char hex[] = {'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'};

		for (int i = 0; i < size; ++ i) {
			int j = i << 1;
			result[j] = hex[bytes[i] >> 4];
			result[j+1] = hex[bytes[i] & 0xf];
		}
		return result;
	}
	nlohmann::json digests(std::initializer_list<std::vector<uint8_t> const *> data)
	{
		return {
			{"sha3_512", digest(data, EVP_sha3_512())},
			{"sha512_256", digest(data, EVP_sha512_256())}
		};
	}

private:
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
	sia::skynet::upload_data metadata_upload("metadata.json", "", "application/json");

	content.data.reserve(1024 * 1024 * 16);
	content.data.resize(content.data.capacity());

	nlohmann::json metadata_json{
		{"sia-skynet-stream", "1.0.7"}
	};

	std::vector<nlohmann::json> lookup_nodes; // roots and spans to use to seek stream history

	while ((size = read(0, content.data.data(), content.data.size()))) {
		auto end_time = time();
		if (size < 0) {
			perror("read");
			return size;
		}
		auto content_identifiers = cryptography.digests({&content.data});
		//content_identifiers["skylink"] = content.filename;
		//content.filename = content_sha3_512;
		content.data.resize(size);

		metadata_json["content"] = {
			{"spans", {
				{"time", {{"start", start_time}, {"end", end_time}, {"length", end_time - start_time}}},
				{"bytes", {{"start", offset}, {"end", offset + size}, {"length", size}}},
				{"index", {{"start", index},{"end", index}, {"length", 0}}}
			}},
			{"identifiers", content_identifiers}
		};
		metadata_json["lookup"] = lookup_nodes;
		std::string metadata_string = metadata_json.dump();
		std::cout << metadata_string << std::endl;
		metadata_upload.data = std::vector<uint8_t>(metadata_string.begin(), metadata_string.end());
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

		size_t depth = 0;
		auto new_lookup_node = metadata_json["content"];
		new_lookup_node["identifiers"] = metadata_identifiers;

		while (lookup_nodes.size() && lookup_nodes.back()["depth"] == depth) {
			auto & back = lookup_nodes.back();
			auto & back_spans = back["spans"];
			for (auto & span : new_lookup_node["spans"].items()) {
				auto start = back_spans[span.key()]["start"];
				span.value()["start"] = start;
				span.value()["length"] = (double)span.value()["end"] - (double)start;
			}
			lookup_nodes.pop_back();
			++ depth;
		}
		new_lookup_node["depth"] = depth;
		lookup_nodes.emplace_back(new_lookup_node);

		++ index;
		offset += size;

		start_time = end_time;
		/*
		if (content.data.size() == content.data.capacity()) {
			content.data.reserve(content.data.capacity()*2);
		}
		*/
		content.data.resize(content.data.capacity());
	}
	std::cout << lookup_nodes.back()["content"]["identifiers"].dump(2) << std::endl;
	return 0;
}
