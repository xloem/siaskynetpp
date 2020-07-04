#include <siaskynet_multiportal.hpp>

#include <iostream>

void dump_response(sia::skynet::response & response, bool content);


int main()
{
	sia::skynet_multiportal multiportal;
	auto transfer = multiportal.begin_transfer(sia::skynet_multiportal::upload);
	sia::skynet portal(transfer.portal);
	std::cout << "uploading to portal: " << portal.options.url << std::endl;

	//auto skylink = portal.upload("hello.txt", "hello");
	auto skylink = portal.upload("hello-folder",
		{
			{"hello.txt", "hello"},
			{"world.txt", "world", "text/example.type.sub-type+etc;mode=0640;owner=1000"}
		});
	multiportal.end_transfer(transfer, 12 + 9 + 5 + 9 + 5 + 51);

	std::cout << "hello-folder: " << skylink << std::endl;

	transfer = multiportal.begin_transfer(sia::skynet_multiportal::download);
	portal.options = transfer.portal;
	std::cout << "querying portal: " << portal.options.url << std::endl;

	//auto response = portal.download(skylink + "/hello.txt");
	auto response = portal.query(skylink);

	multiportal.end_transfer(transfer, 12);

	dump_response(response, true);

	transfer = multiportal.begin_transfer(sia::skynet_multiportal::upload);
	portal.options = transfer.portal;

	std::cout << "uploading to portal: " << portal.options.url << std::endl;
	std::string example_filename = "siaskynetpp_example";
	skylink = portal.upload_file(example_filename);

	multiportal.end_transfer(transfer, 3000000);
	
	std::cout << example_filename << ": " << skylink << std::endl;

	transfer = multiportal.begin_transfer(sia::skynet_multiportal::download);

	std::cout << "downloading from portal: " << portal.options.url << std::endl;
	response = portal.download_file(example_filename + ".fromskynet", skylink);

	multiportal.end_transfer(transfer, 3000000);

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
		std::cout << indent << "data: ";
		if (response.data.size()) {
			std::cout << std::string(response.data.begin() + metadata.offset, response.data.begin() + metadata.offset + metadata.len) << std::endl;
		} else {
			auto data = (sia::skynet()).download(response.skylink, {{metadata.offset, metadata.len}}).data;
			std::cout << std::string(data.begin(), data.end()) << std::endl;
		}
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
	if (content && response.data.size()) {
		std::cout << "data: " << std::string(response.data.begin(), response.data.end()) << std::endl;
	}
	std::cout << "metadata: " << std::endl;
	dump_metadata_hierarchy(response, response.metadata, "  ", content);
}
