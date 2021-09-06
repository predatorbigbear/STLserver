#include<boost/asio.hpp>
#include<vector>
#include<memory>
#include<iostream>
#include<chrono>
#include<thread>
#include<random>
#include<string>
#include<atomic>
#include<thread>

using std::atomic;
using std::string;
using std::vector;
using std::shared_ptr;


static std::string hello{ "hello_world" };


std::shared_ptr<boost::asio::io_context> ioc;


std::shared_ptr<boost::asio::ip::tcp::endpoint> end_point;



//  12580     12573

//  67518
//   29335  29336      27872      282635    280986
//117
// 
//string str{ "POST /18 HTTP/1.1\r\nHost:101.32.203.226:8085\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: 11\r\n\r\nhello_world" };
//string str{ "POST /5 HTTP/1.1\r\nHost: 42.194.134.131:8085\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: 29\r\n\r\nselectWord=select * from test" };
//string str{ "POST /2 HTTP/1.1\r\nHost: 101.32.203.226:8085\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: 11\r\n\r\nhello_world" };
//string str{ "POST /7 HTTP/1.1\r\nHost: 192.168.9.128:8085\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: 17\r\n\r\nredisWord=GET foo" };
//string str{ "POST /10 HTTP/1.1\r\nHost: 101.32.203.200:8085\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: 0\r\n\r\n" };
//string str{ "POST /10 HTTP/1.1\r\nHost: 192.168.9.128:8085\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: 0\r\n\r\n" };
string str{ "POST /2 HTTP/1.1\r\nHost: 101.32.203.200:8085\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: 120\r\n\r\nkey=hello_worldkey=hello_worldkey=hello_worldkey=hello_worldkey=hello_worldkey=hello_worldkey=hello_worldkey=hello_world" };
//string str{ "POST /6 HTTP/1.1\r\nHost: 101.32.203.200:8085\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: 15\r\n\r\nkey=hello_world" };
//string str{ "POST /3 HTTP/1.1\r\nHost: 101.32.203.200:8085\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: 0\r\n\r\n" };
//string str{ "POST /8 HTTP/1.1\r\nHost: 101.32.203.200:8085\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: 29\r\n\r\nusername=deng&password=884378" };
//string str{ "POST /10 HTTP/1.1\r\nHost: 101.32.203.200:8085\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: 341\r\n\r\npublicKey=-----BEGIN%20PUBLIC%20KEY-----%0AMIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQCketFMAocL50svYiVxUC7KS5Dm%0AUXIa38KOZWvfCuNYOt9Go9gh1FZFYSo5fPVgTZwaFU4bVNT7hBbCXjgLSY7dkTT5%0ApRFCVaXVvuY3%2FlQsM%2BDX0yeXvvKu%2BZtIZQAGqFdXPfBOdrXn93%2BBXzUt2hOIB5yB%0AHJSwwc51fUZZxX4rWwIDAQAB%0A-----END%20PUBLIC%20KEY-----%0A&signature=123&ciphertext=12345" };
//string str{ "POST /4 HTTP/1.1\r\nHost: 192.168.80.128:8085\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: 0\r\n\r\n" };
//string str{ "POST /10 HTTP/1.1\r\n\r\n" };
atomic<int>i{ -1 };
atomic<int>j{ -1 };
char arr[1024]{};
auto t1{ std::chrono::high_resolution_clock::now() };



struct testSocket
{
	std::shared_ptr<boost::asio::ip::tcp::socket> sock{};
	char arr[1024]{};
};




void startWrite(std::shared_ptr<testSocket> testSock);
void startRead(std::shared_ptr<testSocket> testSock);
void resetSocket(std::shared_ptr<testSocket> testSock);



void resetSocket(std::shared_ptr<testSocket> testSock)
{
	boost::system::error_code err{};

	testSock->sock.reset(new boost::asio::ip::tcp::socket(*ioc));

	do
	{
		err = {};
		testSock->sock->connect(*end_point, err);
	} while (!err);

	startWrite(testSock);
}



void startWrite(std::shared_ptr<testSocket> testSock)
{
	if (testSock && testSock->sock && ++i < 90000000)
	{
		boost::asio::async_write(*(testSock->sock), boost::asio::buffer(str), [testSock](const boost::system::error_code &err, const std::size_t size)
		{
			if (!err)
			{
				startRead(testSock);
			}
			else
			{
				std::cout << err.value() << ':' << err.message() << '\n';
				//resetSocket(testSock);
			}
		});
	}
	else
	{
		std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - t1).count() << "\n";
	}
}







//1.39
void startRead(std::shared_ptr<testSocket> testSock)
{
	if (testSock && testSock->sock)
	{
		testSock->sock->async_read_some(boost::asio::buffer(testSock->arr, 1024), [testSock](const boost::system::error_code &err, std::size_t size)
		{
			if (!err)
			{
				startWrite(testSock);
			}
			else
			{
				std::cout << err.value() << ':' << err.message() << '\n';
				//resetSocket(testSock);
			}
		});
	}
}



int main()
{

	try
	{

		ioc.reset(new boost::asio::io_context());

		boost::asio::io_context::work work{ *ioc };



		

		end_point.reset( new boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string("101.32.203.200"), 8085));

		int num{};
		boost::system::error_code err{};

		vector<shared_ptr<testSocket>>vec;

		for(int i=0;i!=128;++i)
		{
			shared_ptr<testSocket>temp{ new testSocket() };
			temp->sock.reset(new boost::asio::ip::tcp::socket(*ioc));

			do
			{
				err = {};
				temp->sock->connect(*end_point, err);
			} while (!err);

			startWrite(temp);
		}


		ioc->run();
	}
	catch (const boost::system::error_code &err)
	{
		std::cout << err.value() << " : " << err.message() << '\n';
	}


	return 0;
}








//C  15   50%  35283
//N  15   55%  35803 
// string str{ "POST /2 HTTP/1.1\r\nHost: 101.32.203.226:8085\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: 15\r\n\r\nkey=hello_world" };


//C        34%  19307
//N  120   35%  19687 
// string str{ "POST /2 HTTP/1.1\r\nHost: 101.32.203.226:8085\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: 120\r\n\r\nkey=hello_worldkey=hello_worldkey=hello_worldkey=hello_worldkey=hello_worldkey=hello_worldkey=hello_worldkey=hello_world" };





//C        15%    7380
//N  480   15%    7300
//string str{ "POST /2 HTTP/1.1\r\nHost: 101.32.203.226:8085\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: 512\r\n\r\nkey=hello_worldkey=hello_worldkey=hello_worldkey=hello_worldkey=hello_worldkey=hello_worldkey=hello_worldkey=hello_worldkey=hello_worldkey=hello_worldkey=hello_worldkey=hello_worldkey=hello_worldkey=hello_worldkey=hello_worldkey=hello_worldkey=hello_worldkey=hello_worldkey=hello_worldkey=hello_worldkey=hello_worldkey=hello_worldkey=hello_worldkey=hello_worldkey=hello_worldkey=hello_worldkey=hello_worldkey=hello_worldkey=hello_worldkey=hello_worldkey=hello_worldkey=hello_world" };




// 67421


// 73694


// 75192


// 48247