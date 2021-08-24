/*#include<boost/asio.hpp>
#include<vector>
#include<memory>
#include<iostream>
#include<chrono>
#include<thread>
#include<random>
#include<string>
#include<atomic>
#include<thread>
#include<iostream>

using std::atomic;
using std::string;
using std::vector;
using std::shared_ptr;
using std::cout;


string str{ "*2\r\n$3\r\nget\r\n$3\r\ndan\r\n*2\r\n$3\r\nget\r\n$3\r\ndan\r\n*2\r\n$3\r\nget\r\n$3\r\ndan\r\n" };
//string str{ "POST /lessonTime/query HTTP/1.1\r\nHost: 192.168.88.15:8085\r\n\r\n" };
//string str{ "GET / HTTP/1.1\r\nHost: 192.168.88.15:8085\r\n\r\n" };
//string str{ "POST /zhihuiClass/cxListData HTTP/1.1\r\nHost: 192.168.88.15:8085\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: 65\r\n\r\naction=ONline&SCHOOLNAME=ffffffff|aa:bb:cc:dd:ee:ff&classPostion=\r\n" };
atomic<int>i{ -1 };
char arr[1024]{};
auto t1{ std::chrono::high_resolution_clock::now() };



struct testSocket
{
	std::shared_ptr<boost::asio::ip::tcp::socket> sock{};
	char arr[1024]{};
};




void startWrite(std::shared_ptr<testSocket> testSock);
void startRead(std::shared_ptr<testSocket> testSock);




void startWrite(std::shared_ptr<testSocket> testSock)
{
	if (testSock && testSock->sock && ++i < 10000)
	{
		boost::asio::async_write(*(testSock->sock), boost::asio::buffer(str), [testSock](const boost::system::error_code &err, const std::size_t size)
		{
			if (!err)
			{
				startRead(testSock);
			}
			else
			{
				std::cout << err.value() << "  " << err.message() << '\n';
			}
		});
	}
	else
	{
		std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - t1).count() << "\n";
	}
}




void startRead(std::shared_ptr<testSocket> testSock)
{
	if (testSock && testSock->sock)
	{
		testSock->sock->async_read_some(boost::asio::buffer(testSock->arr, 1024), [testSock](const boost::system::error_code &err, std::size_t size)
		{
			if (!err)
			{
				string str{ testSock->arr,size };

				cout << str << '\n';

			}
			else
			{
				std::cout << err.value() << "  " << err.message() << '\n';
			}
		});
	}
}



int main()
{

	try
	{
		boost::asio::io_context ioc;

		boost::asio::io_context::work work{ ioc };

		boost::asio::ip::tcp::endpoint end_point(boost::asio::ip::address::from_string("127.0.0.1"), 6379);

		int num{};
		boost::system::error_code err{};

		for (int i = 0; i != 1; ++i)
		{
			shared_ptr<testSocket>temp{ new testSocket() };
			temp->sock.reset(new boost::asio::ip::tcp::socket(ioc));

			do
			{
				err = {};
				temp->sock->connect(end_point, err);
			} while (!err);


			startWrite(temp);
		}


		ioc.run();
	}
	catch (const boost::system::error_code &err)
	{
		std::cout << err.value() << " : " << err.message() << '\n';
	}


	return 0;
}


*/