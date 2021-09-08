#include "MiddleCenter.h"



MiddleCenter::MiddleCenter()
{
	try
	{
		m_unlockFun.reset(new std::function<void()>(std::bind(&MiddleCenter::unlock, this)));
	}
	catch (const std::exception &e)
	{
		cout << e.what() << "  ,please restart server\n";
		throw;
	}
}



// use PEM_read_RSA_PUBKEY instead of PEM_read_RSAPublicKey
// https://stackoverflow.com/questions/7818117/why-i-cant-read-openssl-generated-rsa-pub-key-with-pem-read-rsapublickey
// https://stackoverflow.com/questions/18039401/how-can-i-transform-between-the-two-styles-of-public-key-format-one-begin-rsa/29707204#29707204
void MiddleCenter::setKeyPlace(const char * publicKey, const char * privateKey)
{
	try
	{
		if (!publicKey)
			throw std::runtime_error("publicKey is nullptr");
		if (!privateKey)
			throw std::runtime_error("privateKey is nullptr");

		BIO *bio{};
		char *bufferEnd{};
		std::ifstream file;
		FILE *fp{};

		bool success{ false };
		
		do
		{
			file.open(publicKey, std::ios::binary);
			if (!file)
				break;

			m_publicKeyLen = fs::file_size(publicKey);
			if (!m_publicKeyLen)
				break;

			m_publicKeyBuffer = new char[m_publicKeyLen];
			file.read(m_publicKeyBuffer, m_publicKeyLen);
			file.close();

			bufferEnd = m_publicKeyBuffer + m_publicKeyLen;

			m_publicKeyBegin = m_publicKeyBuffer, m_publicKeyEnd = bufferEnd;

			if (!(bio = BIO_new_mem_buf(m_publicKeyBegin, m_publicKeyLen)))       //从字符串读取RSA公钥
				break;
			m_rsaPublic = PEM_read_bio_RSA_PUBKEY(bio, nullptr, nullptr, nullptr);   //从bio结构中得到rsa结构
			if (!m_rsaPublic)
			{
				BIO_free_all(bio);
				break;
			}
			BIO_free_all(bio);
			bio = nullptr;

			success = true;
		} while (false);
		if(!success)
			throw std::runtime_error("can not read publicKey");


		success = false;

		do
		{
			file.open(privateKey, std::ios::binary);
			if (!file)
				break;

			m_privateKeyLen = fs::file_size(privateKey);
			if (!m_privateKeyLen)
				break;

			m_privateKeyBuffer = new char[m_privateKeyLen];
			file.read(m_privateKeyBuffer, m_privateKeyLen);
			file.close();

			bufferEnd = m_privateKeyBuffer + m_privateKeyLen;

			m_privateKeyBegin = m_privateKeyBuffer, m_privateKeyEnd = bufferEnd;

			if (!(bio = BIO_new_mem_buf(m_privateKeyBegin, m_privateKeyLen)))       //从字符串读取RSA私钥
				break;
			m_rsaPrivate = PEM_read_bio_RSAPrivateKey(bio, nullptr, nullptr, nullptr);   //从bio结构中得到rsa结构
			if (!m_rsaPrivate)
			{
				BIO_free_all(bio);
				break;
			}
			BIO_free_all(bio);
			
			success = true;
		} while (false);
		if (!success)
			throw std::runtime_error("can not read privateKey");


		cout << "Success read publicKey and privateKey\n";
	}
	catch (const std::exception &e)
	{
		cout << e.what() << "  ,please restart server\n";
		throw;
	} 
}


void MiddleCenter::setLog(const char * logFileName, std::shared_ptr<IOcontextPool> ioPool , const int overTime , const int bufferSize, const int bufferNum)
{
	//std::lock_guard<std::mutex>l1{ m_mutex };
	m_mutex.lock();
	try
	{
		if (!logFileName)
			throw std::runtime_error("logFileName is invaild");
		if (!ioPool)
			throw std::runtime_error("ioPool is invaild");
		if (overTime<=0)
			throw std::runtime_error("overTime is invaild");
		if (bufferSize<=0)
			throw std::runtime_error("bufferSize is invaild");
		if (bufferNum<=0)
			throw std::runtime_error("bufferNum is invaild");

		if (!m_hasSetLog)
		{
			m_logPool.reset(new LOGPOOL(logFileName , ioPool, overTime, bufferSize, bufferNum));
			m_hasSetLog = true;
		}
		m_mutex.unlock();
	}
	catch (const std::exception &e)
	{
		cout << e.what() << "  ,please restart server\n";
		throw;
	}
}



void MiddleCenter::setHTTPServer(std::shared_ptr<IOcontextPool> ioPool, const std::string &tcpAddress, const std::string &doc_root, const int socketNum, const int timeOut)
{
	//std::lock_guard<std::mutex>l1{ m_mutex };
	m_mutex.lock();
	try
	{
		if (socketNum <= 0)
			throw std::runtime_error("socketNum is invaild");
		if(doc_root.empty() || *doc_root.rbegin()=='/')
			throw std::runtime_error("doc_root is invaild");
		if(!fs::exists(doc_root))
			throw std::runtime_error("doc_root is invaild path");
		if (!m_hasSetListener && m_logPool)
		{
			m_listener.reset(new listener(ioPool, m_multiSqlReadSWPoolSlave,m_multiSqlReadSWPoolMaster,
				m_multiRedisReadPoolSlave, m_multiRedisReadPoolMaster ,m_multiRedisWritePoolMaster,
				m_multiSqlWriteSWPoolMaster,tcpAddress, doc_root, m_logPool, socketNum, timeOut,
				m_publicKeyBegin, m_publicKeyEnd, m_publicKeyLen, m_privateKeyBegin, m_privateKeyEnd, m_privateKeyLen,
				m_rsaPublic, m_rsaPrivate
				));
			m_hasSetListener = true;
		}
		m_mutex.unlock();
	}
	catch (const std::exception &e)
	{
		cout << e.what() << "  ,please restart server\n";
		throw;
	}
}



void MiddleCenter::setMultiRedisRead(std::shared_ptr<IOcontextPool> ioPool, const std::string & redisIP, const unsigned int redisPort, const bool isMaster, const unsigned int bufferNum,
	const unsigned int memorySize, const unsigned int outRangeMaxSize, const unsigned int commandSize)
{
	m_mutex.lock();
	if (!isMaster)
		m_multiRedisReadPoolSlave.reset(new MULTIREDISREADPOOL(ioPool, m_unlockFun, redisIP, redisPort, bufferNum, memorySize, outRangeMaxSize, commandSize));
	else
		m_multiRedisReadPoolMaster.reset(new MULTIREDISREADPOOL(ioPool, m_unlockFun, redisIP, redisPort, bufferNum, memorySize, outRangeMaxSize, commandSize));
}


void MiddleCenter::setMultiRedisWrite(std::shared_ptr<IOcontextPool> ioPool, const std::string & redisIP, const unsigned int redisPort, const bool isMaster, const unsigned int bufferNum,
	const unsigned int memorySize, const unsigned int outRangeMaxSize, const unsigned int commandSize)
{
	m_mutex.lock();
	m_multiRedisWritePoolMaster.reset(new MULTIREDISWRITEPOOL(ioPool, m_unlockFun, redisIP, redisPort, bufferNum, memorySize, outRangeMaxSize, commandSize));
}



void MiddleCenter::setMultiSqlReadSW(std::shared_ptr<IOcontextPool> ioPool, const std::string & SQLHOST, const std::string & SQLUSER, const std::string & SQLPASSWORD,
	const std::string & SQLDB, const std::string & SQLPORT, const bool isMaster, const unsigned int commandMaxSize, const int bufferNum)
{
	m_mutex.lock();
	if (!isMaster)
		m_multiSqlReadSWPoolSlave.reset(new MULTISQLREADSWPOOL(ioPool, m_unlockFun, SQLHOST, SQLUSER, SQLPASSWORD, SQLDB, SQLPORT, m_logPool, commandMaxSize, bufferNum));
	else
		m_multiSqlReadSWPoolMaster.reset(new MULTISQLREADSWPOOL(ioPool, m_unlockFun, SQLHOST, SQLUSER, SQLPASSWORD, SQLDB, SQLPORT, m_logPool, commandMaxSize, bufferNum));
}



void MiddleCenter::setMultiSqlWriteSW(std::shared_ptr<IOcontextPool> ioPool, const std::string & SQLHOST, const std::string & SQLUSER, const std::string & SQLPASSWORD,
	const std::string & SQLDB, const std::string & SQLPORT, const bool isMaster, const unsigned int commandMaxSize, const int bufferNum)
{
	m_mutex.lock();
	m_multiSqlWriteSWPoolMaster.reset(new MULTISQLWRITESWPOOL(ioPool, m_unlockFun, SQLHOST, SQLUSER, SQLPASSWORD, SQLDB, SQLPORT, m_logPool, commandMaxSize, bufferNum));
}



void MiddleCenter::unlock()
{
	std::this_thread::sleep_for(std::chrono::seconds(1));
	m_mutex.unlock();
}
