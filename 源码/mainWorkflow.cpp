/*

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include "workflow/HttpMessage.h"
#include "workflow/HttpUtil.h"
#include "workflow/WFServer.h"
#include "workflow/WFHttpServer.h"
#include "workflow/WFFacilities.h"


static WFFacilities::WaitGroup wait_group(1);

void sig_handler(int signo)
{
    wait_group.done();
}


int main()
{

    WFHttpServer server([](WFHttpTask* task) {
        task->get_resp()->append_output_body("<html>Hello World!</html>");
    });

    if (server.start(8085) == 0) { // start server on port 8888
        wait_group.wait();
        // press "Enter" to end.
        server.stop();
    }

    return 0;
}

*/