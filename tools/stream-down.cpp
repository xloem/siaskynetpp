#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>

#include <unistd.h>

#include <nlohmann/json.hpp>

#include <siaskynet.hpp>

#include "crypto.hpp"
#include "skystream.hpp"

int main(int argc, char **argv)
{
	if (argc != 2) {
		std::cerr << "Usage: " << argv[0] << " sia://<skylinkcontents>" << std::endl;
		return -1;
	}
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
