#include "CipherEnv.hpp"

CipherEnv::CipherEnv(const char* passwd, const char* method,const char* key)
{
    crypto = crypto_init(passwd, key, method);
}

CipherEnv::~CipherEnv() = default;
