#ifndef YWJ_OCSSD_COMMON_H
#define YWJ_OCSSD_COMMON_H
namespace rocksdb {
namespace ocssd {

#define BOOL2STR(b) ((b) ? "true" : "false")
void StrAppendTime(std::string& str);
std::string& StrAppendInt(std::string& str, int val);

} // namespace ocssd
} // namespace rocksdb

#endif
