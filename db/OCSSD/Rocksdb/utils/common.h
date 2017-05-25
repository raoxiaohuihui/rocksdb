#ifndef YWJ_OCSSD_COMMON_H
#define YWJ_OCSSD_COMMON_H
namespace rocksdb {
namespace ocssd {

#define BOOL2STR(b) ((b) ? "true" : "false")
void TimeStr(std::string& str);

}
}

#endif
