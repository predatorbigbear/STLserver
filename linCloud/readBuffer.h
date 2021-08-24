#pragma once


#include "publicHeader.h"
#include "MyReqView.h"


struct ReadBuffer
{
	ReadBuffer(std::shared_ptr<io_context> ioc);

	char* getBuffer();

	size_t& getHasRead();

	MYREQVIEW& getView();

	std::shared_ptr<boost::asio::ip::tcp::socket>& getSock();

	boost::system::error_code& getErrCode();

	void setBodyLen(const int len);

	int getBodyLen();

	void setBodyFrontLen(const int len);

	int getBodyFrontLen();

	const char **bodyPara();

	size_t getBodyParaLen();

private:
	std::shared_ptr<boost::asio::ip::tcp::socket> m_sock{};
	std::unique_ptr<char[]> m_readBuffer{ new char[1024] };
	size_t m_bodyLen{};
	size_t m_bodyFrontLen{};
	size_t m_hasReadLen{};
	size_t m_bodyParaLen{100};
	std::unique_ptr<const char*[]>m_bodyPara{ new const char*[m_bodyParaLen] };

	
	boost::system::error_code m_err{};

	MYREQVIEW reqView;
};


