#pragma once

#include "publicHeader.h"
#include "templateSafeList.h"
#include "httpService.h"


struct HTTPSERVICELIST :protected TEMPLATESAFELIST<HTTPSERVICE>
{
	HTTPSERVICELIST(shared_ptr<IOcontextPool> ioPool, shared_ptr<std::function<void()>>start ,const int beginSize = 200, const int addSize = 50 );

















private:
	shared_ptr<std::function<void()>>m_start;

	shared_ptr<io_context> m_ioc;

};
