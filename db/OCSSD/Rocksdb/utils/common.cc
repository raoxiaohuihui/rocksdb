#include "common.h"

#include <ctime>
#include <cstdio>
#include <string>

namespace rocksdb {
namespace ocssd {

void StrAppendTime(std::string& str)
{
	char buf[32];
	time_t timep;
	struct tm *p;
	int ret;
	time(&timep);
	p = gmtime(&timep);
	str.clear();
	ret = snprintf(buf, 32, "%d/%d/%d", (1900 + p->tm_year), (1 + p->tm_mon), p->tm_mday);
	str.append(buf);
	ret = snprintf(buf, 32, " %d:%d:%d", p->tm_hour, p->tm_min, p->tm_sec);
	str.append(buf);
}

std::string& StrAppendInt(std::string& str, int val)
{
	char buf[32];
	int ret;
	ret = snprintf(buf, 32, "%d", val);
	if (ret < 32) {
		str.append(buf);
	}
	return str;
}

} // namespace ocssd
} // namespace rocksdb
