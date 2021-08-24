#include "readBuffer.h"

ReadBuffer::ReadBuffer(std::shared_ptr<io_context> ioc)
{
	try
	{
		if (!ioc)
			throw std::runtime_error("io_context is null");
		m_sock.reset(new boost::asio::ip::tcp::socket(*ioc));
	}
	catch (const std::exception &e)
	{
		cout << e.what() << "  ,please restart server\n";
	}
}


char * ReadBuffer::getBuffer()
{
	return m_readBuffer.get();
}

size_t & ReadBuffer::getHasRead()
{
	// TODO: 在此处插入 return 语句
	return m_hasReadLen;
}

MYREQVIEW & ReadBuffer::getView()
{
	// TODO: 在此处插入 return 语句
	return reqView;
}


std::shared_ptr<boost::asio::ip::tcp::socket>& ReadBuffer::getSock()
{
	return m_sock;
}

boost::system::error_code & ReadBuffer::getErrCode()
{
	// TODO: 在此处插入 return 语句
	return m_err;
}


void ReadBuffer::setBodyLen(const int len)
{
	m_bodyLen = len;
}

int ReadBuffer::getBodyLen()
{
	return m_bodyLen;
}

void ReadBuffer::setBodyFrontLen(const int len)
{
	m_bodyFrontLen = len;
}

int ReadBuffer::getBodyFrontLen()
{
	return m_bodyFrontLen;
}

const char ** ReadBuffer::bodyPara()
{
	return m_bodyPara.get();
}

size_t ReadBuffer::getBodyParaLen()
{
	return m_bodyParaLen;
}




