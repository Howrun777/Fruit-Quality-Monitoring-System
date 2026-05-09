#include "PasswordHasher.h"
#include <sodium.h>
#include <iostream>

std::string PasswordHasher::hash(const std::string& password) {
    char hashed_password[crypto_pwhash_STRBYTES];
    if (crypto_pwhash_str(hashed_password, password.c_str(), password.length(),
                          crypto_pwhash_OPSLIMIT_INTERACTIVE,
                          crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0) {
        throw std::runtime_error("Out of memory during password hashing");
    }
    return std::string(hashed_password);
}

bool PasswordHasher::verify(const std::string& hash, const std::string& password) {
    return crypto_pwhash_str_verify(hash.c_str(), password.c_str(), password.length()) == 0;
}