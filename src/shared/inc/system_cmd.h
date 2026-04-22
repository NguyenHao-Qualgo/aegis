#pragma once

#include <cstdio>
#include <optional>
#include <string>
#include <vector>

int shell_exec(const std::string& cmd);
int shell_exec(const std::string& cmd, std::string& output);