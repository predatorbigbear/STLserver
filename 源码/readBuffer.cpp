#include "readBuffer.h"

ReadBuffer::ReadBuffer(std::shared_ptr<boost::asio::io_context> ioc, const unsigned int bufNum)
{
	try
	{
		if (!ioc)
			throw std::runtime_error("io_context is null");
		if(!bufNum)
			throw std::runtime_error("bufNum is 0");
		m_sock.reset(new boost::asio::ip::tcp::socket(*ioc));
		m_readBuffer.reset(new char[bufNum]);
	}
	catch (const std::exception &e)
	{
		std::cout << e.what() << "  ,please restart server\n";
	}
}


char * ReadBuffer::getBuffer()
{
	return m_readBuffer.get();
}

size_t & ReadBuffer::getHasRead()
{
	// TODO: �ڴ˴����� return ���
	return m_hasReadLen;
}

MYREQVIEW & ReadBuffer::getView()
{
	// TODO: �ڴ˴����� return ���
	return reqView;
}


std::shared_ptr<boost::asio::ip::tcp::socket>& ReadBuffer::getSock()
{
	return m_sock;
}

boost::system::error_code & ReadBuffer::getErrCode()
{
	// TODO: �ڴ˴����� return ���
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

void ReadBuffer::setFileName(const char * ch, const size_t len)
{
	std::string_view temp{ ch,len };
	m_fileName.swap(temp);
}

std::string_view ReadBuffer::fileName()
{
	return m_fileName;
}




