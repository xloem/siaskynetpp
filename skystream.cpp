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
	auto now = time();
	crypto cryptography;
	sia::skynet portal;
	sia::skynet::upload_data metadata_upload("metadata.json", "", "application/json");
	sia::skynet::upload_data stream_upload("stream", "", "application/octet-stream");
	size_t index = 0;
	size_t offset = 0;
	nlohmann::json metadata{
		{"sia-skynet-stream", "1.0.1"},
		{"history", {
			{{"time", now}}
		}}
	};
	stream_upload.data.reserve(1024 * 1024);
	stream_upload.data.resize(stream_upload.data.capacity());
	ssize_t size;
	while ((size = read(0, stream_upload.data.data(), stream_upload.data.size()))) {
		now = time();
		if (size < 0) {
			perror("read");
			return size;
		}
		stream_upload.data.resize(size);

		metadata["history"][0] = {
			{"time", now},
			{"index", index},
			{"offset", offset},
			{"length", size},
			{"content", {
				{"sha3-512", cryptography.sha3_512({&stream_upload.data})}
			}}
		};
		
		std::string metadata_string = metadata.dump();
		metadata_upload.data = std::vector<uint8_t>(metadata_string.begin(), metadata_string.end());
		stream_upload.filename = "stream-" + std::to_string(index);
		std::cout << metadata_string << std::endl;

		std::string skylink = portal.upload(stream_upload.filename, {metadata_upload, stream_upload});
		stream_upload.data.resize(stream_upload.data.capacity());

		++ index;
		offset += size;

		//metadata["history"][0]["skylink"] = skylink;
		metadata["history"][0]["content"]["skylink"] = skylink + "/" + stream_upload.filename;
		metadata["history"][0]["metadata"] = {
			{"sha3-512", cryptography.sha3_512({&metadata_upload.data})},
			{"skylink", skylink + "/" + metadata_upload.filename}
		};
		metadata["history"][2] = metadata["history"][1];
		metadata["history"][1] = metadata["history"][0];
	}
	std::cout << metadata["history"][0]["metadata"].dump(2) << std::endl;
	return 0;
}
