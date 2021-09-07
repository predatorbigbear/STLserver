/*#include <stdio.h>
#include "workflow/WFHttpServer.h"

int main()
{
	const void *body{};
	size_t len{};
	

	WFHttpServer server([&body,&len](WFHttpTask *task) {
		if (task->get_req()->get_parsed_body(&body, &len))
			task->get_resp()->append_output_body(std::string((char*)body, len));
	});

	if (server.start(8085) == 0) 
	{ // start server on port 8888
		while(getchar())
		{

		}
		server.stop();
	}

	return 0;
}
*/