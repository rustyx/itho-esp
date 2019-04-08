#pragma once
#include <string>

bool console_read(const char* prompt, std::string& res, const char* defaultValue = "", bool multiline = false);
