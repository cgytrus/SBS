#pragma once

#include <string>
#include <Geode/Result.hpp>

using namespace geode::prelude;

struct Shader {
    GLuint vertex = 0;
    GLuint fragment = 0;
    GLuint program = 0;

    Result<> compile(std::string vertexSource, std::string fragmentSource);
    Result<> link();
    void cleanup();

    ~Shader() {
        this->cleanup();
    }
};
