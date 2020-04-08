#include <siaskynet.hpp>

#include <iostream>

void dump_metadata_hierarchy(sia::skynet::response::subfile & metadata, std::string indent = "");

int main()
{
	sia::skynet portal;

	//auto skylink = portal.upload("hello.txt", "hello");
	auto skylink = portal.upload("hello-folder",
		{
			{"hello.txt", "hello"},
			{"world.txt", "world"}
		});

	std::cout << skylink << std::endl;

	//auto response = portal.download(skylink + "/hello.txt");
	auto response = portal.download(skylink);

	std::cout << "skylink: " << response.skylink << std::endl;
	std::cout << "filename: " << response.filename << std::endl;
	std::cout << "data: " << std::string(response.data.begin(), response.data.end()) << std::endl;
	std::cout << "metadata: " << std::endl;
	dump_metadata_hierarchy(response.metadata, "  ");

	return 0;
}

void dump_metadata_hierarchy(sia::skynet::response::subfile & metadata, std::string indent)
{
	std::cout << indent << "contenttype: " << metadata.contenttype << std::endl;
	std::cout << indent << "filename: " << metadata.filename << std::endl;
	std::cout << indent << "len: " << metadata.len << std::endl;
	auto biggerindent = indent + "  ";
	for (auto & file : metadata.subfiles) {
		std::cout << indent << file.first << std::endl;
		dump_metadata_hierarchy(file.second, biggerindent);
	}
}
