/*#include "IOcontextPool.h"
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

		//设置io service数量 cpu核数*2
		ioPool->setThreadNum(4);

		MiddleCenter m1;

		//设置时间轮  每次间隔1s检查   120个轮盘
		m1.setTimeWheel(ioPool, 1, 120);

		//测试用的RSA公私钥（自己放两个就行）
		m1.setKeyPlace("/home/testRSA/rsa_test_public_key.pem", "/home/testRSA/rsa_test_private_key.pem");

		//设置日志  60s检查一次  buffer空间40960   16个日志文件
		m1.setLog("/home/deng/log", ioPool, 60, 40960, 16);


		m1.setMultiRedisRead(ioPool, "127.0.0.1", 6379, false, 1);           //分redis读取

		m1.setMultiRedisWrite(ioPool, "127.0.0.1", 6379, true , 1);          //主redis写入 

		m1.setMultiRedisRead(ioPool, "127.0.0.1", 6379, true, 1);            //主redis读取

		m1.setMultiSqlReadSW(ioPool, "127.0.0.1", "dengdanjun", "13528223610abc,./", "serversql", "3306", false , 100, 1);     //分sql读取

		m1.setMultiSqlWriteSW(ioPool, "127.0.0.1", "dengdanjun", "13528223610abc,./", "serversql", "3306", true , 100, 1);     //主sql写入

		m1.setMultiSqlReadSW(ioPool, "127.0.0.1", "dengdanjun", "13528223610abc,./", "serversql", "3306", true, 100, 1);       //主sql读取

		//绑定8085端口  http默认网页文件夹   1024处理对象   60s内超时
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
*/







