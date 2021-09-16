#include "IOcontextPool.h"
#include "MiddleCenter.h"
#include "MyReqView.h"



//需要分redis读取     主redis读取     主redis写入        分sql读取      主sql读取     主sql写入


// https://zhuanlan.zhihu.com/p/104213185

int main()
{
	//cleanCode();

	RSA* pRSAPublicKey = RSA_new();


	try
	{
		std::shared_ptr<IOcontextPool> ioPool{new IOcontextPool() };

		ioPool->setThreadNum(8);

		MiddleCenter m1;

		m1.setTimeWheel(ioPool, 1, 120);

		m1.setKeyPlace("/home/testRSA/rsa_test_public_key.pem", "/home/testRSA/rsa_test_private_key.pem");

		m1.setLog("/home/deng/log", ioPool, 60, 40960, 16);

		m1.setMultiRedisRead(ioPool, "127.0.0.1", 6379, false, 1);           //分redis读取

		m1.setMultiRedisWrite(ioPool, "127.0.0.1", 6379, true , 1);          //主redis写入 

		m1.setMultiRedisRead(ioPool, "127.0.0.1", 6379, true, 1);            //主redis读取

		m1.setMultiSqlReadSW(ioPool, "127.0.0.1", "dengdanjun", "13528223610abc,./", "serversql", "3306", false , 100, 1);     //分sql读取

		m1.setMultiSqlWriteSW(ioPool, "127.0.0.1", "dengdanjun", "13528223610abc,./", "serversql", "3306", true , 100, 1);     //主sql写入

		m1.setMultiSqlReadSW(ioPool, "127.0.0.1", "dengdanjun", "13528223610abc,./", "serversql", "3306", true, 100, 1);       //主sql读取

		m1.setHTTPServer(ioPool, "0.0.0.0:8085", "/home/webHttp/httpDir", 1024, 60);

		ioPool->run();
	}
	catch (const boost::system::system_error &err)
	{
		std::cout << err.code() << " " << err.what() << '\n';
		return err.code().value();
	}

	return 0;
}








