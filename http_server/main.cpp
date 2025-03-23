#include <chrono>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <string>

#include "http_connection.h"
#include <Windows.h>

#include <fstream>

void httpServer(tcp::acceptor& acceptor, tcp::socket& socket);

int main(int argc, char* argv[])
{
	SetConsoleCP(CP_UTF8);
	SetConsoleOutputCP(CP_UTF8);

	try
	{
		unsigned short port;

		std::ifstream file("..\\ini.txt");
		if (file.is_open()) {
			while (!file.eof()) {
				std::string input;
				file >> input;
				if (input == "port_search:") {
					file >> port;
					break;
				}
			}
		} else {
			throw std::exception("File ini.txt missing");
		}
		file.close();

		auto const address = net::ip::make_address("0.0.0.0");

		net::io_context ioc{1};

		tcp::acceptor acceptor{ioc, { address, port }};
		tcp::socket socket{ioc};
		httpServer(acceptor, socket);

		std::cout << "Open browser and connect to http://localhost:8080 to see the web server operating" << std::endl;

		ioc.run();
	}
	catch (std::exception const& e)
	{
		std::cerr << "Error: " << e.what() << std::endl;
		return EXIT_FAILURE;
	}
}

void httpServer(tcp::acceptor& acceptor, tcp::socket& socket) {
	acceptor.async_accept(socket, [&](beast::error_code ec) {
		if (!ec)
			std::make_shared<HttpConnection>(std::move(socket))->start();
		httpServer(acceptor, socket);
	});
}
