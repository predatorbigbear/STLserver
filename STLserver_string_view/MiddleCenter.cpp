#include "MiddleCenter.h"



MiddleCenter::MiddleCenter()
{
	m_unlockFun.reset(new std::function<void()>(std::bind(&MiddleCenter::unlock, this)));
}

MiddleCenter::~MiddleCenter()
{
	if (m_initSql)
		freeMysql();
}






void MiddleCenter::setLog(const char * logFileName,const std::shared_ptr<IOcontextPool> &ioPool , bool& success, const int overTime , const int bufferSize, const int bufferNum)
{
	//std::lock_guard<std::mutex>l1{ m_mutex };
	m_mutex.lock();
	try
	{
		if (!logFileName)
			throw std::runtime_error("logFileName is invaild");
		if (!ioPool)
			throw std::runtime_error("ioPool is invaild");
		if (overTime <= 0)
			throw std::runtime_error("overTime is invaild");
		if (bufferSize <= 0)
			throw std::runtime_error("bufferSize is invaild");
		if (bufferNum <= 0)
			throw std::runtime_error("bufferNum is invaild");

		if (!m_hasSetLog)
		{
			m_logPool.reset(new LOGPOOL(logFileName , ioPool, success , overTime, bufferSize, bufferNum));
			if (!success)
				throw std::runtime_error("create LOGPOOL fail");
			m_hasSetLog = true;
		}
		success = true;
		m_mutex.unlock();
	}
	catch (const std::exception &e)
	{
		cout << e.what() << "  ,please restart server\n";
		success = false;
		m_mutex.unlock();
	}
}




void MiddleCenter::setHTTPServer(const std::shared_ptr<IOcontextPool> &ioPool, bool& success, const std::string &tcpAddress,
	const std::string &doc_root,
	const std::vector<std::string>&& fileVec, 
	const int socketNum, const int timeOut,
	const bool isHttp , const char* cert , const char* privateKey )
{
	//std::lock_guard<std::mutex>l1{ m_mutex };
	m_mutex.lock();
	if (!success)
	{
		m_mutex.unlock();
		return;
	}
	try
	{
		if (socketNum <= 0)
			throw std::runtime_error("socketNum is invaild");
		if(doc_root.empty() || *(doc_root.rbegin()) == '/')
			throw std::runtime_error("doc_root is invaild");
		if(!fs::exists(doc_root))
			throw std::runtime_error("doc_root is invaild path");

		//https情况
		if (!isHttp)
		{
			if (!cert || !fs::exists(cert))
				throw std::runtime_error("cert is invaild path");
			if (!privateKey || !fs::exists(privateKey))
				throw std::runtime_error("privateKey is invaild path");
		}


		m_fileVec = fileVec;
		m_fileMap.reset(new std::unordered_map<std::string_view, std::string>{});
		if (!m_fileVec.empty())
		{
			std::unique_ptr<char[]> fileBuf{};
			std::string fileFullName;
			std::string sendStr;
			unsigned int fileSize{};
			unsigned int bufSize{};
			std::string output;
			std::string_view fileView;
			for (auto& fileName : m_fileVec)
			{
				if(fileName.empty())
					throw std::runtime_error("fileName is empty");
				fileFullName = doc_root + "/" + fileName;
				if(!fs::exists(fileFullName))
					throw std::runtime_error(fileFullName + " is invaild path");
				fileSize = fs::file_size(fileFullName);
				if(!fileSize)
					throw std::runtime_error(fileFullName + " size is 0");
				if (fileSize > bufSize)
				{
					bufSize = fileSize;
					fileBuf.reset(new char[fileSize]);
				}
				file.open(fileFullName, std::ios::binary);
				if(!file)
					throw std::runtime_error("open "+ fileFullName +" fail");
				file.read(fileBuf.get(), fileSize);
				file.close();

				sendStr.assign("HTTP/1.1 200 OK\r\n");
				sendStr.append("Connection:keep-alive\r\n");
				sendStr.append("Keep-Alive:timeout=30\r\n");
				sendStr.append("Cache-Control:public,max-age=3600,immutable\r\n");

				if (gzip(fileBuf.get(), fileSize, output))
				{
					fileView = std::string_view(output.c_str(), output.size());
					sendStr.append("Content-Encoding:gzip\r\n");
				}
				else
					fileView = std::string_view(fileBuf.get(), fileSize);

				sendStr.append("Content-Length:");
				sendStr.append(std::to_string(fileView.size()));
				sendStr.append("\r\n\r\n");
				sendStr.append(fileView);

				m_fileMap->insert(std::make_pair(std::string_view(fileName.c_str(), fileName.size()), sendStr));
			}
		}

		if (!m_hasSetListener && m_logPool)
		{
			if (isHttp)
			{
				m_listener.reset(new listener(ioPool, m_multiSqlReadSWPoolMaster,
					m_multiRedisReadPoolMaster, m_multiRedisWritePoolMaster,
					m_multiSqlWriteSWPoolMaster, tcpAddress, doc_root, m_logPool,
					m_fileMap,
					socketNum, timeOut, m_checkSecond, m_timeWheel));
			}
			else
			{
				m_httpsListener.reset(new HTTPSlistener(ioPool, m_multiSqlReadSWPoolMaster,
					m_multiRedisReadPoolMaster, m_multiRedisWritePoolMaster,
					m_multiSqlWriteSWPoolMaster, tcpAddress, doc_root, m_logPool,
					m_fileMap,
					socketNum, timeOut, m_checkSecond, m_timeWheel, cert, privateKey));
			}
			m_hasSetListener = true;
		}
		m_mutex.unlock();
	}
	catch (const std::exception &e)
	{
		cout << e.what() << "  ,please restart server\n";
		success = false;
		m_mutex.unlock();
	}
}





void MiddleCenter::setWebserviceServer(const std::shared_ptr<IOcontextPool> &ioPool, bool& success, const std::string& tcpAddress, 
	const std::string& doc_root, const std::vector<std::string>&& fileVec,
	const std::string& backGround, const std::vector<std::string>&& backGroundFileVec, 
	const int socketNum, const int timeOut,
	const char* cert, const char* privateKey)
{
	m_mutex.lock();
	if (!success)
	{
		m_mutex.unlock();
		return;
	}
	try
	{
		if (socketNum <= 0)
			throw std::runtime_error("socketNum is invaild");
		if (doc_root.empty() || *(doc_root.rbegin()) == '/')
			throw std::runtime_error("doc_root is invaild");
		if (!fs::exists(doc_root))
			throw std::runtime_error("doc_root is invaild path");
		if (backGround.empty() || *(backGround.rbegin()) == '/')
			throw std::runtime_error("backGround is invaild");
		if (!fs::exists(backGround))
			throw std::runtime_error("backGround is invaild path");
		if(fileVec.empty())
			throw std::runtime_error("fileVec is empty");
		if (backGroundFileVec.empty())
			throw std::runtime_error("backGroundFileVec is empty");

		if (!cert || !fs::exists(cert))
			throw std::runtime_error("cert is invaild path");
		if (!privateKey || !fs::exists(privateKey))
			throw std::runtime_error("privateKey is invaild path");


		//网页普通页面html资源初始化
		m_webFileVec.reset(new std::vector<std::string>{});
		if (!fileVec.empty())
		{
			std::unique_ptr<char[]> fileBuf{};
			std::string fileFullName;
			std::string sendStr;
			unsigned int fileSize{};
			unsigned int bufSize{};
			std::string output;
			std::string_view fileView;
			for (auto& fileName : fileVec)
			{
				if (fileName.empty())
					throw std::runtime_error("fileName is empty");
				fileFullName = doc_root + "/" + fileName;
				if (!fs::exists(fileFullName))
					throw std::runtime_error(fileFullName + " is invaild path");
				fileSize = fs::file_size(fileFullName);
				if (!fileSize)
					throw std::runtime_error(fileFullName + " size is 0");
				if (fileSize > bufSize)
				{
					bufSize = fileSize;
					fileBuf.reset(new char[fileSize]);
				}
				file.open(fileFullName, std::ios::binary);
				if (!file)
					throw std::runtime_error("open " + fileFullName + " fail");
				file.read(fileBuf.get(), fileSize);
				file.close();

				sendStr.assign("HTTP/1.1 200 OK\r\n");
				sendStr.append("Connection:keep-alive\r\n");
				sendStr.append("Keep-Alive:timeout=30\r\n");
				sendStr.append("Cache-Control:public,max-age=3600,immutable\r\n");

				if (brotli_compress(fileBuf.get(), fileSize, output))
				{
					fileView = std::string_view(output.c_str(), output.size());
					sendStr.append("Content-Encoding:br\r\n");
				}
				else
					fileView = std::string_view(fileBuf.get(), fileSize);

				sendStr.append("Content-Length:");
				sendStr.append(std::to_string(fileView.size()));
				sendStr.append("\r\n\r\n");
				sendStr.append(fileView);

				m_webFileVec->emplace_back(sendStr);
			}
		}


		//网页后台管理页面html资源初始化
		m_webBGFileVec.reset(new std::vector<std::string>{});
		if (!backGroundFileVec.empty())
		{
			std::unique_ptr<char[]> fileBuf{};
			std::string fileFullName;
			std::string sendStr;
			unsigned int fileSize{};
			unsigned int bufSize{};
			std::string output;
			std::string_view fileView;
			for (auto& fileName : backGroundFileVec)
			{
				if (fileName.empty())
					throw std::runtime_error("fileName is empty");
				fileFullName = backGround + "/" + fileName;
				if (!fs::exists(fileFullName))
					throw std::runtime_error(fileFullName + " is invaild path");
				fileSize = fs::file_size(fileFullName);
				if (!fileSize)
					throw std::runtime_error(fileFullName + " size is 0");
				if (fileSize > bufSize)
				{
					bufSize = fileSize;
					fileBuf.reset(new char[fileSize]);
				}
				file.open(fileFullName, std::ios::binary);
				if (!file)
					throw std::runtime_error("open " + fileFullName + " fail");
				file.read(fileBuf.get(), fileSize);
				file.close();

				sendStr.assign("HTTP/1.1 200 OK\r\n");
				sendStr.append("Connection:keep-alive\r\n");
				sendStr.append("Keep-Alive:timeout=30\r\n");
				sendStr.append("Cache-Control:public,max-age=3600,immutable\r\n");

				if (brotli_compress(fileBuf.get(), fileSize, output))
				{
					fileView = std::string_view(output.c_str(), output.size());
					sendStr.append("Content-Encoding:br\r\n");
				}
				else
					fileView = std::string_view(fileBuf.get(), fileSize);

				sendStr.append("Content-Length:");
				sendStr.append(std::to_string(fileView.size()));
				sendStr.append("\r\n\r\n");
				sendStr.append(fileView);

				m_webBGFileVec->emplace_back(sendStr);
			}
		}


		if (!m_hasSetListener && m_logPool)
		{
			m_randomCode.reset(new RandomCodeGenerator());
			m_webListener.reset(new WEBSERVICELISTENER(ioPool, m_multiSqlReadSWPoolMaster,
				m_multiRedisReadPoolMaster, m_multiRedisReadCopyPoolMaster, m_multiRedisWritePoolMaster,
				m_multiSqlWriteSWPoolMaster, tcpAddress, doc_root, m_logPool,
				m_webFileVec, m_webBGFileVec,
				socketNum, timeOut, m_checkSecond, m_timeWheel, cert, privateKey, m_checkIP, m_randomCode));
			m_hasSetListener = true;
		}
		m_mutex.unlock();
	}
	catch (const std::exception& e)
	{
		cout << e.what() << "  ,please restart server\n";
		success = false;
		m_mutex.unlock();
	}

}



void MiddleCenter::setMultiRedisRead(const std::shared_ptr<IOcontextPool> &ioPool, bool& success, const std::string & redisIP,
	const unsigned int redisPort, const unsigned int bufferNum,
	const unsigned int memorySize, const unsigned int outRangeMaxSize, const unsigned int commandSize)
{
	try
	{
		m_mutex.lock();
		if (!success)
		{
			m_mutex.unlock();
			return;
		}

		m_multiRedisReadPoolMaster.reset(new MULTIREDISREADPOOL(ioPool, m_logPool, m_unlockFun,
			m_timeWheel,
			redisIP, redisPort, bufferNum, memorySize, outRangeMaxSize, commandSize));

		m_multiRedisReadCopyPoolMaster.reset(new MULTIREDISREADCOPYPOOL(ioPool, m_logPool, m_unlockFun,
			m_timeWheel,
			redisIP, redisPort, bufferNum, memorySize, outRangeMaxSize, commandSize));
		success = true;
	}
	catch (const std::exception& e)
	{
		cout << e.what() << "   ,please restart server\n";
		success = false;
		m_mutex.unlock();
	}
}


void MiddleCenter::setMultiRedisWrite(const std::shared_ptr<IOcontextPool> &ioPool, bool& success, const std::string & redisIP,
	const unsigned int redisPort, const unsigned int bufferNum,
	const unsigned int memorySize, const unsigned int outRangeMaxSize, const unsigned int commandSize)
{
	try
	{
		m_mutex.lock();
		if (!success)
		{
			m_mutex.unlock();
			return;
		}
		m_multiRedisWritePoolMaster.reset(new MULTIREDISWRITEPOOL(ioPool, m_unlockFun, redisIP, redisPort, bufferNum, memorySize, outRangeMaxSize, commandSize));
		success = true;
	}
	catch (const std::exception& e)
	{
		cout << e.what() << "   ,please restart server\n";
		success = false;
		m_mutex.unlock();
	}
}



void MiddleCenter::setMultiSqlReadSW(const std::shared_ptr<IOcontextPool> &ioPool, bool& success, const std::string & SQLHOST,
	const std::string & SQLUSER, const std::string & SQLPASSWORD,
	const std::string & SQLDB, const std::string & SQLPORT, const int bufferNum,
	const unsigned int commandMaxSize, const unsigned int bufferSize)
{
	try
	{
		m_mutex.lock();
		if (!success)
		{
			m_mutex.unlock();
			return;
		}
		m_multiSqlReadSWPoolMaster.reset(new MULTISQLREADSWPOOL(ioPool, m_unlockFun, SQLHOST, SQLUSER, SQLPASSWORD, SQLDB, SQLPORT, m_logPool, commandMaxSize, bufferNum, bufferSize));
		success = true;
	}
	catch (const std::exception& e)
	{
		cout << e.what() << "   ,please restart server\n";
		success = false;
		m_mutex.unlock();
	}
}



void MiddleCenter::setMultiSqlWriteSW(const std::shared_ptr<IOcontextPool> &ioPool, bool& success, const std::string & SQLHOST,
	const std::string & SQLUSER, const std::string & SQLPASSWORD,
	const std::string & SQLDB, const std::string & SQLPORT, const unsigned int commandMaxSize, const int bufferNum)
{
	try
	{
		m_mutex.lock();
		if (!success)
		{
			m_mutex.unlock();
			return;
		}
		m_multiSqlWriteSWPoolMaster.reset(new MULTISQLWRITESWPOOL(ioPool, m_unlockFun, SQLHOST, SQLUSER, SQLPASSWORD, SQLDB, SQLPORT, m_logPool, commandMaxSize, bufferNum));
		success = true;
	}
	catch (const std::exception& e)
	{
		cout << e.what() << "   ,please restart server\n";
		success = false;
		m_mutex.unlock();
	}
}


//加载mysql库文件，在调用mysql C api之前调用
void MiddleCenter::initMysql(bool& success)
{
	m_mutex.lock();
	if (!success)
	{
		m_mutex.unlock();
		return;
	}
	if (mysql_library_init(0, NULL, NULL)) //在所有线程对MYSQL C API调用之前调用
	{
		std::cout << "mysql_library_init error\n";
		success = false;
		m_initSql = true;
		m_mutex.unlock();
		return;
	}
	success = true;
	m_mutex.unlock();
}

void MiddleCenter::freeMysql()
{
	mysql_library_end();
	m_initSql = false;
}

void MiddleCenter::unlock()
{
	std::this_thread::sleep_for(std::chrono::seconds(1));
	m_mutex.unlock();
}



void MiddleCenter::setTimeWheel(const std::shared_ptr<IOcontextPool> &ioPool, bool& success,
	const unsigned int checkSecond, const unsigned int wheelNum, const unsigned int everySecondFunctionNumber)
{
	try
	{
		m_checkSecond = checkSecond;
		m_timeWheel.reset(new STLTimeWheel(ioPool->getIoNext(), checkSecond, wheelNum, everySecondFunctionNumber));
		success = true;
	}
	catch (const std::exception& e)
	{
		success = false;
	}
}



void MiddleCenter::setCheckIP(const std::shared_ptr<IOcontextPool> &ioPool, const std::string& host,
	const std::string& port, const std::string& country,
	const std::string& saveFile, bool& success, const unsigned int checkTime)
{
	try
	{
		m_mutex.lock();
		if (!success)
		{
			m_mutex.unlock();
			return;
		}
		if (host.empty())
		{
			success = false;
			m_mutex.unlock();
			return;
		}
		if (port.empty())
		{
			success = false;
			m_mutex.unlock();
			return;
		}
		if (country.empty())
		{
			success = false;
			m_mutex.unlock();
			return;
		}
		if (saveFile.empty())
		{
			success = false;
			m_mutex.unlock();
			return;
		}
		if (checkTime < 3600)
		{
			success = false;
			m_mutex.unlock();
			return;
		}
		if (!ioPool || !m_logPool || !m_timeWheel)
		{
			success = false;
			m_mutex.unlock();
			return;
		}


		m_checkIP.reset(new CHECKIP(ioPool, m_logPool->getLogNext(), m_timeWheel, host, port, country, saveFile, checkTime));
		success = true;
		m_mutex.unlock();
	}
	catch (const std::exception& e)
	{
		cout << e.what() << "   ,please restart server\n";
		success = false;
		m_mutex.unlock();
	}
}



bool MiddleCenter::gzip(const char* source, const int sourLen, std::string& outPut)
{
	if (!source || !sourLen)
		return false;
	outPut.clear();
	int ret{};
	while (true)
	{
		memset(&zs, 0, sizeof(zs));

		// 使用最高压缩级别Z_BEST_COMPRESSION(9)
		if (deflateInit2(&zs, Z_BEST_COMPRESSION, Z_DEFLATED,
			15 | 16, 8, Z_DEFAULT_STRATEGY) != Z_OK)
		{
			return false;
		}

		zs.next_in = (Bytef*)source;
		zs.avail_in = sourLen;

		do
		{
			zs.next_out = reinterpret_cast<Bytef*>(outbuffer);
			zs.avail_out = outbufferLen;

			ret = deflate(&zs, Z_FINISH);

			if (outPut.size() < zs.total_out)
				outPut.append(outbuffer, zs.total_out - outPut.size());
		} while (ret == Z_OK);

		deflateEnd(&zs);

		if (ret != Z_STREAM_END)
			return false;
		break;
	}

	if (outPut.size() > sourLen)
		return false;
	return true;
}

bool MiddleCenter::brotli_compress(const char* source, const int sourLen, std::string& outPut)
{
	if (!source || !sourLen)
		return false;
	outPut.clear();

	// 初始化输出缓冲区（预估最大压缩大小）
	size_t max_compressed_size = BrotliEncoderMaxCompressedSize(sourLen);

	try
	{
		outPut.resize(max_compressed_size);
	}
	catch (...)
	{
		return false;
	}

	// 创建Brotli编码器实例
	BrotliEncoderState* encoder = BrotliEncoderCreateInstance(nullptr, nullptr, nullptr);
	if (!encoder)
		return false;

	// 设置最高压缩等级（11）
	if (BrotliEncoderSetParameter(encoder, BROTLI_PARAM_QUALITY, 11) != BROTLI_TRUE)
		return false;
	// 设置窗口大小为最大（24）
	if(BrotliEncoderSetParameter(encoder, BROTLI_PARAM_LGWIN, 24) != BROTLI_TRUE)
		return false;

	size_t available_in = sourLen;
	const uint8_t* next_in = reinterpret_cast<const uint8_t*>(source);
	size_t available_out = outPut.size();
	uint8_t* next_out = reinterpret_cast<uint8_t*>(outPut.data());

	// 执行压缩
	BROTLI_BOOL result = BrotliEncoderCompressStream(
		encoder,
		BROTLI_OPERATION_FINISH,
		&available_in,
		&next_in,
		&available_out,
		&next_out,
		nullptr
	);

	// 检查压缩结果
	if (result != BROTLI_TRUE) 
	{
		BrotliEncoderDestroyInstance(encoder);
		return false;
	}

	// 获取实际压缩后大小
	size_t compressed_size = outPut.size() - available_out;
	outPut.resize(compressed_size);

	// 清理资源
	BrotliEncoderDestroyInstance(encoder);

	if (outPut.size() > sourLen)
		return false;
	return true;
}
