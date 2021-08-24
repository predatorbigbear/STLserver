#pragma once



#include "publicHeader.h"



struct IOcontextPool
{
	IOcontextPool();

	void setThreadNum(const size_t threadNum = 0);

	std::shared_ptr<io_context> getIoNext();

	void run();

	void stop();

private:
	std::shared_ptr<std::vector<std::shared_ptr<io_context>>>m_ioc{};
	boost::thread_group m_tg;
	std::vector<io_context::work> m_work;
	std::mutex m_ioMutex;

	std::shared_ptr<io_context> m_io_temp{};

	bool m_hasRun{ false };
	bool m_hasSetThreadNum{ false };

	size_t m_ioNum{};
};