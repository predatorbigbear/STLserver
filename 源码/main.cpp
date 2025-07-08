#include "IOcontextPool.h"
#include "MiddleCenter.h"
#include "MyReqView.h"



//��Ҫ��redis��ȡ     ��redis��ȡ     ��redisд��        ��sql��ȡ      ��sql��ȡ     ��sqlд��


// https://zhuanlan.zhihu.com/p/104213185

int main()
{

	try
	{
		std::shared_ptr<IOcontextPool> ioPool{new IOcontextPool() };

		bool success;
		//����io service���� cpu����*2
		ioPool->setThreadNum(success, 1);
		if (!success)
			return -1;

		MiddleCenter m1;

		//����ʱ����  ÿ�μ��1s���   120������
		m1.setTimeWheel(ioPool, success, 1, 120);
		if (!success)
			return -2;

		//������־  60s���һ��  buffer�ռ�40960   16����־�ļ�
		m1.setLog("/home/deng/log", ioPool, success, 60);
		if (!success)
			return -3;

		
		m1.setMultiRedisWrite(ioPool, success, "127.0.0.1", 6379, 1);          //��redisд��
		if (!success)
			return -4;

		m1.setMultiRedisRead(ioPool, success, "127.0.0.1", 6379, 1);            //��redis��ȡ
		if (!success)
			return -5;

		m1.initMysql(success);
		if (!success)
			return -6;

		m1.setMultiSqlWriteSW(ioPool, success,  "127.0.0.1", "root", "884378abc", "serversql", "3306", true , 1);     //��sqlд��
		if (!success)
		{
			m1.freeMysql();
			return -7;
		}

		m1.setMultiSqlReadSW(ioPool, success, "127.0.0.1", "root", "884378abc", "serversql", "3306", true, 1);       //��sql��ȡ
		if (!success)
		{
			m1.freeMysql();
			return -8;
		}

		//��8085�˿�  httpĬ����ҳ�ļ���   1024�������   60s�ڳ�ʱ
		m1.setHTTPServer(ioPool, success, "0.0.0.0:8085", "/home/webHttp/httpDir", {}, 1024, 30);
		if (!success)
		{
			m1.freeMysql();
			return -9;
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
		return -10;
	}

	return 0;
}








