#pragma once
#include <string>

struct DBStruct {
	std::string db_host;
	std::string db_port;
	std::string db_name;
	std::string db_user;
	std::string db_password;

	std::string DbstructConnection() {
		// "host=localhost " "port=5432 " "dbname=my_database " "user=my_database_user " "password=my_password_123"
		std::string host = "host=";
		std::string port = " port=";
		std::string dbname = " dbname=";
		std::string user = " user=";
		std::string password = " password=";

		std::string all = host + db_host + port + db_port + dbname + db_name + user + db_user + password + db_password;
		return all;
	}
};