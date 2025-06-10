#include <Geode/Geode.hpp>

using namespace geode::prelude;

struct Shader {
    GLuint vertex = 0;
    GLuint fragment = 0;
    GLuint program = 0;

    Result<> compile(
        std::string vertexSource,
        std::string fragmentSource
    ) {
        vertexSource = string::trim(vertexSource);
        fragmentSource = string::trim(fragmentSource);

        auto getShaderLog = [](GLuint id) -> std::string {
            GLint length, written;
            glGetShaderiv(id, GL_INFO_LOG_LENGTH, &length);
            if (length <= 0)
                return "";
            auto stuff = new char[length + 1];
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
        if(!res) {
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
            "#version 150\n",
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
        if(!res) {
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

    Result<> link() {
        if (!vertex)
            return Err("vertex shader not compiled");
        if (!fragment)
            return Err("fragment shader not compiled");

        auto getProgramLog = [](GLuint id) -> std::string {
            GLint length, written;
            glGetProgramiv(id, GL_INFO_LOG_LENGTH, &length);
            if (length <= 0)
                return "";
            auto stuff = new char[length + 1];
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
        if(!res) {
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

    void cleanup() {
        if (program)
            glDeleteProgram(program);
        program = 0;
    }
};

bool s_glewInitialized = false;
CCSize s_lastFrameSize = {};

static union {
    struct {
        GLuint left;
        GLuint right;
    } s_fbo;
    GLuint s_fbos[2];
};
static union {
    struct {
        GLuint left;
        GLuint right;
    } s_color;
    GLuint s_colors[2];
};
static union {
    struct {
        GLuint left;
        GLuint right;
    } s_stencil;
    GLuint s_stencils[2];
};

Shader s_shader;
GLint s_leftLoc = 0;
GLint s_rightLoc = 0;
const char* s_vertShader = R"(
attribute vec4 aPosition;
varying vec2 Position;
void main() {
    gl_Position = aPosition;
    Position = aPosition.xy;
}
)";
const char* s_fragShader = R"(
varying vec2 Position;
uniform sampler2D left;
uniform sampler2D right;
void main() {
    vec2 texCoords = vec2(mod(Position.x + 1.0, 1.0), (Position.y + 1.0) * 0.5);
    gl_FragColor = texture(Position.x < 0.0 ? left : right, texCoords);
    //gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0);
}
)";
GLuint s_vao = 0;
GLuint s_vbo = 0;

bool s_isRight;

void cleanup() {
    if (s_vbo)
        glDeleteBuffers(1, &s_vbo);
    s_vbo = 0;
    if (s_vao)
        glDeleteVertexArrays(1, &s_vao);
    s_vao = 0;
    s_shader.cleanup();
    s_leftLoc = 0;
    s_rightLoc = 0;
    if (s_stencils[0] || s_stencils[1])
        glDeleteRenderbuffers(2, s_stencils);
    s_stencils[0] = 0;
    s_stencils[1] = 0;
    if (s_colors[0] || s_colors[1])
        glDeleteTextures(2, s_colors);
    s_colors[0] = 0;
    s_colors[1] = 0;
    if (s_fbos[0] || s_fbos[1])
        glDeleteFramebuffers(2, s_fbos);
    s_fbos[0] = 0;
    s_fbos[1] = 0;
}

GLint s_savedFboDraw, s_savedFboRead, s_savedTexture, s_savedRbo;
void startMod() {
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &s_savedFboDraw);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &s_savedFboRead);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &s_savedTexture);
    glGetIntegerv(GL_RENDERBUFFER_BINDING, &s_savedRbo);
}
void endMod() {
    glBindRenderbuffer(GL_RENDERBUFFER, s_savedRbo);
    ccGLBindTexture2D(s_savedTexture);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, s_savedFboRead);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, s_savedFboDraw);
}

#include <Geode/modify/CCEGLView.hpp>
class $modify(CCEGLView) {
    $override bool initGlew() {
        bool res = CCEGLView::initGlew();
        if (!res)
            return false;
        s_glewInitialized = true;
        return true;
    }
};

#include <Geode/modify/CCEGLViewProtocol.hpp>
class $modify(CCEGLViewProtocol) {
    $override void setFrameSize(float width, float height) {
        CCEGLViewProtocol::setFrameSize(width, height);
        if (this->getFrameSize() == s_lastFrameSize)
            return;
        if (!s_glewInitialized) {
            log::debug("frame size changed to {}x{} but glew is not initialized yet", width, height);
            return;
        }

        s_lastFrameSize = this->getFrameSize();

        const auto w = static_cast<GLsizei>(width);
        const auto h = static_cast<GLsizei>(height);

        log::debug("regenerating FBOs for {}x{}", width, height);

        startMod();

        cleanup();

        glGenFramebuffers(2, s_fbos);
        glGenTextures(2, s_colors);
        glGenRenderbuffers(2, s_stencils);

        ccGLBindTexture2D(s_color.left);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        ccGLBindTexture2D(s_color.right);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glBindRenderbuffer(GL_RENDERBUFFER, s_stencil.left);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_STENCIL_INDEX8, w, h);

        glBindRenderbuffer(GL_RENDERBUFFER, s_stencil.right);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_STENCIL_INDEX8, w, h);

        bool incomplete = false;

        glBindFramebuffer(GL_FRAMEBUFFER, s_fbo.left);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, s_color.left, 0);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, s_stencil.left, 0);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            log::error("left framebuffer incomplete, cleaning up");
            incomplete = true;
        }

        glBindFramebuffer(GL_FRAMEBUFFER, s_fbo.right);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, s_color.right, 0);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, s_stencil.right, 0);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            log::error("right framebuffer incomplete, cleaning up");
            incomplete = true;
        }

        if (incomplete) {
            cleanup();
            endMod();
            return;
        }

        auto res = s_shader.compile(s_vertShader, s_fragShader);
        if (!res) {
            log::error("{}", res.unwrapErr());
            cleanup();
            endMod();
            return;
        }

        glBindAttribLocation(s_shader.program, 0, "aPosition");

        res = s_shader.link();
        if (!res) {
            log::error("{}", res.unwrapErr());
            cleanup();
            endMod();
            return;
        }

        s_leftLoc = glGetUniformLocation(s_shader.program, "left");
        s_rightLoc = glGetUniformLocation(s_shader.program, "right");

        constexpr GLfloat vertices[] = {
            // positions
            -1.0f, 1.0f,
            -1.0f, -1.0f,
            1.0f, -1.0f,

            -1.0f,  1.0f,
            1.0f, -1.0f,
            1.0f,  1.0f
        };
        glGenVertexArrays(1, &s_vao);
        glGenBuffers(1, &s_vbo);
        glBindVertexArray(s_vao);
        glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), &vertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), static_cast<void*>(nullptr));
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        endMod();
    }
};

// TODO: make more compatible
#include <Geode/modify/CCDirector.hpp>
class $modify(CCDirector) {
    $override void drawScene() {
        this->calculateDeltaTime();

        if (!m_bPaused) {
            m_pScheduler->update(m_fDeltaTime);
        }

        if (m_pobOpenGLView) {
            m_pobOpenGLView->pollInputEvents();
        }

        if (m_pNextScene) {
            this->setNextScene();
        }

        kmGLPushMatrix();

        startMod();

        s_isRight = false;
        glBindFramebuffer(GL_FRAMEBUFFER, s_fbo.left);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        if (m_pRunningScene)
            m_pRunningScene->visit();
        if (m_pNotificationNode)
            m_pNotificationNode->visit();
        if (m_bDisplayStats)
            showStats();
        if (m_bDisplayFPS)
            showFPSLabel();

        if (PlayLayer::get()) {
            PlayLayer::get()->m_background->setPositionX(PlayLayer::get()->m_background->getPositionX() - 10.0f);
        }

        s_isRight = true;
        glBindFramebuffer(GL_FRAMEBUFFER, s_fbo.right);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        if (m_pRunningScene)
            m_pRunningScene->visit();
        if (m_pNotificationNode)
            m_pNotificationNode->visit();
        if (m_bDisplayStats)
            showStats();
        if (m_bDisplayFPS)
            showFPSLabel();

        if (PlayLayer::get()) {
            PlayLayer::get()->m_background->setPositionX(PlayLayer::get()->m_background->getPositionX() + 10.0f);
        }

        endMod();

        kmGLPopMatrix();

        glBindVertexArray(s_vao);

        ccGLUseProgram(s_shader.program);

        ccGLBindTexture2DN(0, s_color.left);
        ccGLBindTexture2DN(1, s_color.right);

        glUniform1i(s_leftLoc, 0);
        glUniform1i(s_rightLoc, 1);

        glDrawArrays(GL_TRIANGLES, 0, 6);

        glBindVertexArray(0);

#ifndef GEODE_IS_MACOS
        CC_INCREMENT_GL_DRAWS(1);
#endif

        m_uTotalFrames++;

        if (m_pobOpenGLView) {
            m_pobOpenGLView->swapBuffers();
        }

        if (m_bDisplayStats) {
            calculateMPF();
        }
    }
};
