

#include "IOcontextPool.h"
#include "MiddleCenter.h"
#include "MyReqView.h"



//需要分redis读取     主redis读取     主redis写入        分sql读取      主sql读取     主sql写入


// https://zhuanlan.zhihu.com/p/104213185


int main()
{

	try
	{
		std::shared_ptr<IOcontextPool> ioPool{new IOcontextPool() };

		bool success;

		
		//设置io service数量   一般来说直接设置核数即可
		ioPool->setThreadNum(success, 1);
		if (!success)
			return -1;

		MiddleCenter m1;

		//设置时间轮  每次间隔1s检查   120个轮盘
		m1.setTimeWheel(ioPool, success, 1, 120);
		if (!success)
			return -2;

		//设置日志  60s检查一次  buffer空间40960   16个日志文件    需要有该目录存在，比如/home/deng   log是日志前缀名称
		m1.setLog("/home/deng/log", ioPool, success, 60);
		if (!success)
			return -3;

		
		m1.setMultiRedisWrite(ioPool, success, "127.0.0.1", 6379, 1);          //主redis写入
		if (!success)
			return -4;

		//redis操作模块  可进行读写操作    一般来说直接设置核数即可
		m1.setMultiRedisRead(ioPool, success, "127.0.0.1", 6379, 1);            //主redis读取
		if (!success)
			return -5;

		m1.initMysql(success);
		if (!success)
			return -6;

		//设置mysql  连接地址   用户名  密码  连接数据库名称  端口
		m1.setMultiSqlWriteSW(ioPool, success,  "127.0.0.1", "root", "884378abc", "serversql", "3306", true , 1);     //主sql写入
		if (!success)
		{
			m1.freeMysql();
			return -7;
		}

		//设置mysql  连接地址   用户名  密码  连接数据库名称  端口
		m1.setMultiSqlReadSW(ioPool, success, "127.0.0.1", "root", "884378abc", "serversql", "3306");       //主sql读取
		if (!success)
		{
			m1.freeMysql();
			return -8;
		}

		//设置mysql  连接地址   用户名  密码  连接数据库名称  端口
		m1.setMultiSqlRead(ioPool, success, "127.0.0.1", "root", "884378abc", "serversql", 3306);
		if (!success)
		{
			m1.freeMysql();
			return -8;
		}


		//绑定8085端口  http默认网页文件夹   1024处理对象   60s内超时        网页端文件存储目录需要存在
		//{}填入想要缓存起来的文件，内部会调用gzip预先进行压缩
		//{}是一个vector，可以为空
		
		//启动HTTP示例,测试接口4性能开启setHTTPServer，注释setCheckIP  setVerifyCode    setWebserviceServer即可
		//m1.setHTTPServer(ioPool, success, "0.0.0.0:8085", "/home/webHttp/httpDir", {"webfile"}, 1024, 30);
		
		//启动HTTPS可以仿照以下示例
		//m1.setHTTPServer(ioPool, success, "0.0.0.0:8085", "/home/webHttp/httpDir", { "webfile" }, 1024, 30, 
		//	false, "/home/certs/web.crt", "/home/certs/web.key");


		
		m1.setCheckIP(ioPool, "ftp.apnic.net", "80", "CN", "/home/download/ipRecord.txt", success);
		if (!success)
		{
			m1.freeMysql();
			return -9;
		}
		

		
		m1.setVerifyCode(ioPool, success);
		if (!success)
		{
			m1.freeMysql();
			return -10;
		}

		
		m1.setWebserviceServer(ioPool, success, "0.0.0.0:8085", "/home/kongweb", { "0","1","2","3","4" },
			"/home/backWeb", { "0","1","2","3" }, 50, 60, "/home/certs/web.crt", "/home/certs/web.key");
			

		if (!success)
		{
			m1.freeMysql();
			return -11;
		}
		

		ioPool->run();
	}
	catch (const boost::system::system_error &err)
	{
		std::cout << err.code() << " " << err.what() << '\n';
		return err.code().value();
	}
	catch (const std::exception& err)
	{
		std::cout << err.what() << '\n';
		return -12;
	}

	return 0;
}







