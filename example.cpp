#include <siaskynet.hpp>

#include <iostream>

void dump_response(sia::skynet::response & response, bool content);

int main()
{
	sia::skynet portal;

	//auto skylink = portal.upload("hello.txt", "hello");
	auto skylink = portal.upload("hello-folder",
		{
			{"hello.txt", "hello"},
			{"world.txt", "world", "text/example.type.sub-type+etc;mode=0640;owner=1000"}
		});

	std::cout << "hello-folder: " << skylink << std::endl;

	//auto response = portal.download(skylink + "/hello.txt");
	auto response = portal.download(skylink);

	dump_response(response, true);

	std::string example_filename = "siaskynetpp_example";
	skylink = portal.upload_file(example_filename);
	
	std::cout << example_filename << ": " << skylink << std::endl;

	response = portal.download_file(example_filename + ".fromskynet", skylink);

	dump_response(response, false);

	return 0;
}

void dump_metadata_hierarchy(sia::skynet::response & response, sia::skynet::response::subfile & metadata, std::string indent, bool content)
{
	std::cout << indent << "contenttype: " << metadata.contenttype << std::endl;
	std::cout << indent << "filename: " << metadata.filename << std::endl;
	std::cout << indent << "len: " << metadata.len << std::endl;
	std::cout << indent << "offset: " << metadata.offset << std::endl;
	if (content) {
		std::cout << indent << "data: " << std::string(response.data.begin() + metadata.offset, response.data.begin() + metadata.offset + metadata.len) << std::endl;
	}
	auto biggerindent = indent + "  ";
	for (auto & file : metadata.subfiles) {
		std::cout << indent << file.first << std::endl;
		dump_metadata_hierarchy(response, file.second, biggerindent, content);
	}
}

void dump_response(sia::skynet::response & response, bool content)
{
	std::cout << "skylink: " << response.skylink << std::endl;
	std::cout << "filename: " << response.filename << std::endl;
	if (content) {
		std::cout << "data: " << std::string(response.data.begin(), response.data.end()) << std::endl;
	}
	std::cout << "metadata: " << std::endl;
	dump_metadata_hierarchy(response, response.metadata, "  ", content);
}
