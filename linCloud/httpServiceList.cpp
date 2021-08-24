#include "httpServiceList.h"

HTTPSERVICELIST::HTTPSERVICELIST(shared_ptr<IOcontextPool> ioPool, shared_ptr<std::function<void()>>start ,const int beginSize, const int addSize)
{
	if (!ioPool)
		throw std::runtime_error("ioPool is null");
	TEMPLATESAFELIST<HTTPSERVICE>(beginSize, addSize);
	m_start = start;
}


