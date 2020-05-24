#include <siaskynet.hpp>

#include <iostream>

using namespace std;

int usage(char ** argv, std::string issue = {})
{
	if (issue.size()) { cout << issue << std::endl; }
	cout << "Usage: " << argv[0] << " [--timeout milliseconds] [--portal url] local_path|sia://skylink|--portals" << std::endl;
	return issue.size() ? -1 : 0;
}

int main(int argc, char ** argv)
{
	if (argc < 2) {
		return usage(argv);
	}
	sia::skynet portal;

	string positional;
	long long milliseconds = 0;
	for (char ** argp = argv + 1; argp < argv + argc; ++ argp) {
		std::string arg = *argp;
		if ("--timeout" == arg) {
			milliseconds = atoll(*++argp);
		} else if ("--portal" == arg) {
			portal.options.url = *++argp;
		} else if ("--help" == arg) {
			return usage(argv);
		} else if ("--portals" == arg) {
			for (auto & portal : sia::skynet::portals()) {
				std::cout << portal.url << std::endl;
			}
			return 0;
		} else {
			if (positional.size()) { return usage(argv, "Unrecognised: " + arg); }
			positional = arg;
		}
	}

	if (positional.compare(0, 6, "sia://") == 0) {
		portal.download_directory(positional);
	} else {
		std::cout << portal.upload_directory(positional) << std::endl;
	}

	return 0;
}
