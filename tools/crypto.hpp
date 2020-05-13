#pragma once

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
#ifndef OPENSSL_NO_BLAKE2
			{"blake2b512", digest(data, EVP_blake2b512())},
#endif
			{"sha3_512", digest(data, EVP_sha3_512())},
			{"sha512_256", digest(data, EVP_sha512_256())}
		};
	}

private:
	EVP_MD_CTX * mdctx;
};
