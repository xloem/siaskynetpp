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
		return read(tail, span, offset);
	}

	std::pair<double,double> span(std::string span)
	{
		auto content_span = tail["content"]["spans"][span];
		double end = content_span["end"];
		double start = content_span["start"];
		for (auto & lookup : tail["lookup"]) {
			double point = lookup["spans"][span]["start"];
			if (point < start) { start = point; }
		}
		return {start,end};
	}

	double length(std::string span)
	{
		auto range = this->span(span);
		return range.second - range.first;
	}

private:
	std::vector<uint8_t> read(nlohmann::json & node, std::string span, double offset)
	{
		auto content_span = node["content"]["spans"][span];
		double content_start = content_span["start"];
		double content_end = content_span["end"];
		if (offset >= content_start && offset < content_end) {
			auto data = get(node["content"]["identifiers"]);
			return {data.begin() + content_start - offset, data.end()};
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
				return read(cache[identifier], span, offset);
			}
		}
		throw std::runtime_error(span + " " + std::to_string(offset) + " out of range");
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

int main(int argc, char **argv)
{
	skystream stream("skylink", argv[1]);
	std::cerr << "Bytes: " << (unsigned long long)stream.length("bytes") << std::endl;
	std::cerr << "Time: " << stream.length("time") << std::endl;
	std::cerr << "Index: " << (unsigned long long)stream.length("index") << std::endl;

	std::string span = "bytes";
	auto range = stream.span(span);
	double offset = range.first;
	while (offset < range.second) {
		auto data = stream.read(span, offset);
		size_t suboffset = 0;
		while (suboffset < data.size()) {
			ssize_t size = write(1, data.data() + suboffset, data.size() - suboffset);
			if (size < 0) {
				perror("write");
				return size;
			}
			suboffset += size;
		}
		offset += data.size();
	}
	return 0;
}
