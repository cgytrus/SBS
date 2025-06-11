﻿#include "Shader.hpp"

Result<> Shader::compile(std::string vertexSource, std::string fragmentSource) {
    vertexSource = string::trim(vertexSource);
    fragmentSource = string::trim(fragmentSource);

    auto getShaderLog = [](const GLuint id) -> std::string {
        GLint length, written;
        glGetShaderiv(id, GL_INFO_LOG_LENGTH, &length);
        if (length <= 0)
            return "";
        const auto stuff = new char[length + 1];
        glGetShaderInfoLog(id, length, &written, stuff);
        std::string result(stuff);
        delete[] stuff;
        return result;
    };
    GLint res;

    vertex = glCreateShader(GL_VERTEX_SHADER);
    const char* vertexSources[] = {
#ifdef GEODE_IS_WINDOWS
        "#version 120\n",
#endif
#ifdef GEODE_IS_MOBILE
        "precision highp float;\n",
#endif
        vertexSource.c_str()
    };
    glShaderSource(vertex, sizeof(vertexSources) / sizeof(char*), vertexSources, nullptr);
    glCompileShader(vertex);
    auto vertexLog = string::trim(getShaderLog(vertex));

    glGetShaderiv(vertex, GL_COMPILE_STATUS, &res);
    if (!res) {
        glDeleteShader(vertex);
        vertex = 0;
        return Err("vertex shader compilation failed:\n{}", vertexLog);
    }

    if (vertexLog.empty()) {
        log::debug("vertex shader compilation successful");
    }
    else {
        log::debug("vertex shader compilation successful:\n{}", vertexLog);
    }

    fragment = glCreateShader(GL_FRAGMENT_SHADER);
    const char* fragmentSources[] = {
#ifdef GEODE_IS_WINDOWS
        "#version 120\n",
#endif
#ifdef GEODE_IS_MOBILE
        "precision highp float;\n",
#endif
        fragmentSource.c_str()
    };
    glShaderSource(fragment, sizeof(vertexSources) / sizeof(char*), fragmentSources, nullptr);
    glCompileShader(fragment);
    auto fragmentLog = string::trim(getShaderLog(fragment));

    glGetShaderiv(fragment, GL_COMPILE_STATUS, &res);
    if (!res) {
        glDeleteShader(vertex);
        glDeleteShader(fragment);
        vertex = 0;
        fragment = 0;
        return Err("fragment shader compilation failed:\n{}", fragmentLog);
    }

    if (fragmentLog.empty()) {
        log::debug("fragment shader compilation successful");
    }
    else {
        log::debug("fragment shader compilation successful:\n{}", fragmentLog);
    }

    program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);

    return Ok();
}

Result<> Shader::link() {
    if (!vertex)
        return Err("vertex shader not compiled");
    if (!fragment)
        return Err("fragment shader not compiled");

    auto getProgramLog = [](const GLuint id) -> std::string {
        GLint length, written;
        glGetProgramiv(id, GL_INFO_LOG_LENGTH, &length);
        if (length <= 0)
            return "";
        const auto stuff = new char[length + 1];
        glGetProgramInfoLog(id, length, &written, stuff);
        std::string result(stuff);
        delete[] stuff;
        return result;
    };
    GLint res;

    glLinkProgram(program);
    auto programLog = string::trim(getProgramLog(program));

    glDeleteShader(vertex);
    glDeleteShader(fragment);
    vertex = 0;
    fragment = 0;

    glGetProgramiv(program, GL_LINK_STATUS, &res);
    if (!res) {
        glDeleteProgram(program);
        program = 0;
        return Err("shader link failed:\n{}", programLog);
    }

    if (programLog.empty()) {
        log::debug("shader link successful");
    }
    else {
        log::debug("shader link successful:\n{}", programLog);
    }

    return Ok();
}

void Shader::cleanup() {
    if (program)
        glDeleteProgram(program);
    program = 0;
}
