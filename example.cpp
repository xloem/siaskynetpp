#include <siaskynet.hpp>

#include <iostream>

void dump_metadata(sia::skynet::response::subfile & metadata, std::string indent = "")
{
	std::cout << indent << "contenttype: " << metadata.contenttype << std::endl;
	std::cout << indent << "filename: " << metadata.filename << std::endl;
	std::cout << indent << "len: " << metadata.len << std::endl;
	auto biggerindent = indent + "  ";
	for (auto & file : metadata.subfiles) {
		std::cout << indent << file.first << std::endl;
		dump_metadata(file.second, biggerindent);
	}
}

int main()
{
	sia::skynet portal;

	//auto skylink = portal.write("hello", "hello.txt");
	auto skylink = portal.write({
		{"hello.txt", "hello"},
		{"world.txt", "world"}
	}, "hello-folder");
	std::cout << skylink << std::endl;

	//auto response = portal.read(skylink + "/hello.txt");
	auto response = portal.read(skylink);

	std::cout << "skylink: " << response.skylink << std::endl;
	std::cout << "filename: " << response.filename << std::endl;
	dump_metadata(response.metadata);
	std::cout << "data: " << std::string(response.data.begin(), response.data.end()) << std::endl;

	return 0;
}
