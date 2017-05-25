#ifndef YWJ_OCSSD_OC_EXCEPTION_HPP
#define YWJ_OCSSD_OC_EXCEPTION_HPP

#include <string>
#include <cerror>
#include <cstring>

namespace rocksdb {
namespace ocssd {

class oc_excpetion : public std::exception {
public:
	explicit oc_excpetion(const char *msg) throw()
		: msg_(msg)
	{
	}
	virtual const char* what() const throw()
	{
		return msg_;
	}
protected:
	const char *msg_;
};

class dev_init_exception : public oc_excpetion {
public:
	explicit dev_init_exception(std::string err_hint) throw()
		: oc_excpetion((err_hint + strerror(errno)).c_str())
	{
	}
};

class blk_cleaner_exception : public oc_excpetion {
public:
	explicit blk_cleaner_exception(std::string err_hint) throw()
		: oc_excpetion((err_hint + strerror(errno)).c_str())
	{
	}
};



}
}

#endif
