#include "http_connection.h"

#include <sstream>
#include <iomanip>
#include <locale>
#include <codecvt>
#include <iostream>
#include <fstream>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

std::string url_decode(const std::string& encoded) {
	std::string res;
	std::istringstream iss(encoded);
	char ch;

	while (iss.get(ch)) {
		if (ch == '%') {
			int hex;
			iss >> std::hex >> hex;
			res += static_cast<char>(hex);
		} else {
			res += ch;
		}
	}

	return res;
}

std::string convert_to_utf8(const std::string& str) {
	std::string url_decoded = url_decode(str);
	return url_decoded;
}

HttpConnection::HttpConnection(tcp::socket socket) : socket_(std::move(socket)) {}

void HttpConnection::start() {
	readRequest();
	checkDeadline();
}

void HttpConnection::readRequest() {
	auto self = shared_from_this();

	http::async_read(socket_, buffer_, request_, [self](beast::error_code ec, std::size_t bytes_transferred) {
		boost::ignore_unused(bytes_transferred);
		if (!ec)
			self->processRequest();
	});
}

void HttpConnection::processRequest() {
	response_.version(request_.version());
	response_.keep_alive(false);

	switch (request_.method()) {
	case http::verb::get:
		response_.result(http::status::ok);
		response_.set(http::field::server, "Beast");
		createResponseGet();
		break;
	case http::verb::post:
		response_.result(http::status::ok);
		response_.set(http::field::server, "Beast");
		createResponsePost();
		break;

	default:
		response_.result(http::status::bad_request);
		response_.set(http::field::content_type, "text/plain");
		beast::ostream(response_.body()) << "Invalid request-method '" << std::string(request_.method_string()) << "'";
		break;
	}

	writeResponse();
}

void HttpConnection::createResponseGet() {
	if (request_.target() == "/") {
		response_.set(http::field::content_type, "text/html");
		beast::ostream(response_.body()) << "<html>\n"
										 << "<head><meta charset=\"UTF-8\"><title>Search Engine</title></head>\n"
										 << "<body>\n"
										 << "<h1>Search Engine</h1>\n"
										 << "<p>Welcome!<p>\n"
										 << "<form action=\"/\" method=\"post\">\n"
										 << "    <label for=\"search\">Search:</label><br>\n"
										 << "    <input type=\"text\" id=\"search\" name=\"search\"><br>\n"
										 << "    <input type=\"submit\" value=\"Search\">\n"
										 << "</form>\n"
										 << "</body>\n"
										 << "</html>\n";
	} else {
		response_.result(http::status::not_found);
		response_.set(http::field::content_type, "text/plain");
		beast::ostream(response_.body()) << "File not found\r\n";
	}
}

void HttpConnection::createResponsePost() {
	if (request_.target() == "/") {
		std::string s = buffers_to_string(request_.body().data());

		// Create system default locale
		boost::locale::generator gen;
		std::locale loc = gen("");
		std::locale::global(loc);
		std::wcout.imbue(loc);

		std::string s_low = boost::locale::to_lower(s);

		std::cout << "POST data: " << s_low << std::endl;

		std::vector<std::string> input_low;
		std::string divide_input = "";
		bool b_search = true;
		for (int i = 0; i < s_low.size(); i++) {
			if (b_search) {
				for (i; i < s_low.size(); i++) {
					if (s_low[i] == '=') {
						i++;
						b_search = false;
						break;
					}
				}
			}

			if (s_low[i] != '+') {
				divide_input = divide_input + s_low[i];
			} else if (s_low[i] == '+') {
				input_low.push_back(divide_input);
				divide_input.clear();
			}
		}
		input_low.push_back(divide_input);

		size_t pos = s_low.find('=');
		if (pos == std::string::npos) {
			response_.result(http::status::not_found);
			response_.set(http::field::content_type, "text/plain");
			beast::ostream(response_.body()) << "File not found\r\n";
			return;
		}

		std::string key = s_low.substr(0, pos);
		std::string value = s_low.substr(pos + 1);

		std::string utf8value = convert_to_utf8(value);

		if (key != "search") {
			response_.result(http::status::not_found);
			response_.set(http::field::content_type, "text/plain");
			beast::ostream(response_.body()) << "File not found\r\n";
			return;
		}

		// Complete: Fetch your own search results here - Получите результаты собственного поиска здесь
		DBStruct dbstruct;

		std::ifstream file("..\\ini.txt");
		if (file.is_open()) {
			readFile(file, dbstruct);
		}

		const std::string connectStr = dbstruct.DbstructConnection();
		pqxx::connection con(connectStr);
		if (con.is_open()) {
			std::cout << "Opened database successfully: " << con.dbname() << std::endl;
		}
		pqxx::work pq_work(con);

		std::map<std::string, float> linkAmount;

		for (auto word : input_low) {
			pqxx::result r_link = pq_work.exec("SELECT DISTINCT d.name, w.amount  FROM documents_words dw "
											   "JOIN words w ON dw.words = w.id "
											   "JOIN documents d ON dw.documents = d.id "
											   "WHERE w.name = '" + word + "' "
											   "ORDER BY w.amount DESC;");
			std::string this_link = "";
			int this_amount = 0;
			bool b_link = true;
			for (auto row : r_link) {
				for (auto field : row) {
					if (b_link) {
						this_link = field.c_str();
						b_link = false;
					} else {
						this_amount = std::stoi(field.c_str());
						linkAmount[this_link] = linkAmount[this_link] + this_amount;
						b_link = true;
					}
				}
			}
		}

		std::vector<std::string> searchResult;
		std::vector<int> amountResult;

		for (const auto& [this_link, this_amount] : linkAmount) {
			searchResult.push_back(this_link);
			amountResult.push_back(this_amount);
		}

		if (input_low.size() > 1) {
			bool not_sorted = true;
			while (not_sorted) {
				not_sorted = false;
				for (int i = 0; i < amountResult.size() - 1; i++) {
					if (amountResult[i] < amountResult[i + 1]) {
						std::string str_swap = searchResult[i];
						int int_swap = amountResult[i];

						searchResult[i] = searchResult[i + 1];
						amountResult[i] = amountResult[i + 1];

						searchResult[i + 1] = str_swap;
						amountResult[i + 1] = int_swap;

						not_sorted = true;
					}
				}
			}
		}


		/*std::vector<std::string> searchResult = {
			"https://en.wikipedia.org/wiki/Main_Page",
			"https://en.wikipedia.org/wiki/Wikipedia",
		};*/

		response_.set(http::field::content_type, "text/html");
		beast::ostream(response_.body()) << "<html>\n"
										 << "<head><meta charset=\"UTF-8\"><title>Search Engine</title></head>\n"
										 << "<body>\n"
										 << "<h1>Search Engine</h1>\n"
										 << "<p>Response:<p>\n"
										 << "<ul>\n";

		for (const auto& url : searchResult) {

			beast::ostream(response_.body()) << "<li><a href=\"" << url << "\">" << url << "</a></li>";
		}

		beast::ostream(response_.body()) << "</ul>\n"
										 << "</body>\n"
										 << "</html>\n";
	} else {
		response_.result(http::status::not_found);
		response_.set(http::field::content_type, "text/plain");
		beast::ostream(response_.body()) << "File not found\r\n";
	}
}

void HttpConnection::writeResponse() {
	auto self = shared_from_this();

	response_.content_length(response_.body().size());

	http::async_write(socket_, response_, [self](beast::error_code ec, std::size_t) {
		self->socket_.shutdown(tcp::socket::shutdown_send, ec);
		self->deadline_.cancel();
	});
}

void HttpConnection::checkDeadline() {
	auto self = shared_from_this();

	deadline_.async_wait([self](beast::error_code ec) {
		if (!ec) {
			self->socket_.close(ec);
		}
	});
}

void HttpConnection::readFile(std::ifstream& file, DBStruct& dbstruct) {
	while (!file.eof()) {
		std::string input;
		file >> input;
		if (input == "host:") {
			file >> dbstruct.db_host;
		} else if (input == "port:") {
			file >> dbstruct.db_port;
		} else if (input == "dbname:") {
			file >> dbstruct.db_name;
		} else if (input == "user:") {
			file >> dbstruct.db_user;
		} else if (input == "password:") {
			file >> dbstruct.db_password;
		}
	}
}
