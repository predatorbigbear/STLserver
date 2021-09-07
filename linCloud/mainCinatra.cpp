/*#include "cinatra.hpp"
#include<string_view>
using namespace cinatra;

int main() {
	int max_thread_num = std::thread::hardware_concurrency();
	http_server server(max_thread_num);
	server.listen("0.0.0.0", "8085");
	server.set_http_handler<GET, POST>("/2", [](request& req, response& res) {
		std::string_view httpbody{ req.body() };
		res.set_status_and_content(status_type::ok, std::string(&*httpbody.cbegin(), httpbody.size()));
	});

	server.run();
	return 0;
}
*/