#ifndef CIPHERENV_HPP
#define CIPHERENV_HPP
extern "C"
{
#include "crypto.h"
};

#include <memory>

class CipherEnv
{
public:
    crypto_t* crypto = nullptr;
    CipherEnv(const char* passwd, const char* method,const char* key= nullptr);
    ~CipherEnv();
};

#endif // CIPHERENV_HPP
