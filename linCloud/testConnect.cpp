/*#include "publicHeader.h"





int main()
{
	try
	{
		io_context ioc;

		io_context::work work{ ioc };

		vector<shared_ptr<boost::asio::ip::tcp::socket>>vec;

		for (int i = 0; i != 10; ++i)
		{
			boost::asio::ip::tcp::endpoint end_point(boost::asio::ip::address::from_string("42.194.134.131"), 8085);

			shared_ptr<boost::asio::ip::tcp::socket>sock{std::make_shared<boost::asio::ip::tcp::socket>(ioc)};

			sock->connect(end_point);

			vec.emplace_back(sock);
			
		}

		ioc.run();
	}
	catch (const boost::system::error_code &err)
	{
		cout << err.value() << " : " << err.message() << '\n';
	}

	return 0;
}
*/