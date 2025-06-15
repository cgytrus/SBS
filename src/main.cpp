#include <Geode/Geode.hpp>

using namespace geode::prelude;

#include "Shader.hpp"
#include "parallax.hpp"

bool s_glewInitialized = false;
CCSize s_lastFrameSize = {};

static union {
    struct {
        GLuint left;
        GLuint right;
    } s_color;
    GLuint s_colors[2];
};
GLuint s_fbo;
GLuint s_depthStencil;

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
    gl_FragColor = texture2D(Position.x < 0.0 ? left : right, texCoords);
}
)";
GLuint s_vao = 0;
GLuint s_vbo = 0;

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
    if (s_depthStencil)
        glDeleteRenderbuffers(1, &s_depthStencil);
    s_depthStencil = 0;
    if (s_colors[0])
        ccGLDeleteTextureN(0, s_colors[0]);
    if (s_colors[1])
        ccGLDeleteTextureN(1, s_colors[1]);
    s_colors[0] = 0;
    s_colors[1] = 0;
    if (s_fbo)
        glDeleteFramebuffers(1, &s_fbo);
    s_fbo = 0;
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

        glGenFramebuffers(1, &s_fbo);
        glGenTextures(2, s_colors);
        glGenRenderbuffers(1, &s_depthStencil);

        ccGLBindTexture2D(s_color.left);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        ccGLBindTexture2D(s_color.right);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glBindRenderbuffer(GL_RENDERBUFFER, s_depthStencil);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, w, h);

        glBindFramebuffer(GL_FRAMEBUFFER, s_fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, s_color.left, 0);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, s_color.right, 0);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, s_depthStencil, 0);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            log::error("framebuffer incomplete, cleaning up");
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

// ShaderLayer fix
#include <Geode/modify/CCRenderTexture.hpp>
class $modify(StereoRenderTexture, CCRenderTexture) {
    struct Fields {
        CCTexture2D* m_rightTexture;
    };

    $override bool initWithWidthAndHeight(int w, int h, CCTexture2DPixelFormat eFormat, GLuint uDepthStencilFormat) {
        bool ret = CCRenderTexture::initWithWidthAndHeight(w, h, eFormat, uDepthStencilFormat);
        if (!ret)
            return false;

        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &m_nOldFBO);

        // textures must be power of two squared
        const unsigned int powW = m_pTexture->getPixelsWide();
        const unsigned int powH = m_pTexture->getPixelsHigh();
        void* data = malloc(static_cast<int>(powW * powH * 4));
        memset(data, 0, static_cast<int>(powW * powH * 4));
        m_fields->m_rightTexture = new CCTexture2D();
        m_fields->m_rightTexture->initWithData(data, m_pTexture->getPixelFormat(), powW, powH, m_pTexture->getContentSize());
        m_fields->m_rightTexture->setAliasTexParameters();
        if (data)
            free(data);

        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &m_nOldFBO);
        glBindFramebuffer(GL_FRAMEBUFFER, m_uFBO);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, m_fields->m_rightTexture->getName(), 0);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            log::error("framebuffer incomplete, cleaning up");
            cleanup();
            endMod();
            glBindFramebuffer(GL_FRAMEBUFFER, m_nOldFBO);
            return false;
        }

        glBindFramebuffer(GL_FRAMEBUFFER, m_nOldFBO);
        return true;
    }
};

bool s_enabled = false;
bool parallax::s_debug = false;

#include <Geode/modify/CCNode.hpp>
class $modify(CCNode) {
    $override void visit() {
        if (static_cast<CCNode*>(this) != CCDirector::get()->getRunningScene()) {
            // ShaderLayer fix
            if (!s_enabled)
                return CCNode::visit();
            auto* sl = typeinfo_cast<ShaderLayer*>(this->getParent());
            if (!sl)
                return CCNode::visit();
            auto* sprite = typeinfo_cast<CCSprite*>(this);
            if (!sprite || sprite != sl->m_sprite)
                return CCNode::visit();

            constexpr GLenum left[] { GL_COLOR_ATTACHMENT0 };
            glDrawBuffers(1, left);
            CCNode::visit();
            sprite->getTexture()->retain();
            sprite->setTexture(static_cast<StereoRenderTexture*>(sl->m_renderTexture)->m_fields->m_rightTexture);

            constexpr GLenum right[] { GL_COLOR_ATTACHMENT1 };
            glDrawBuffers(1, right);
            CCNode::visit();
            sprite->getTexture()->retain();
            sprite->setTexture(sl->m_renderTexture->m_pTexture);

            constexpr GLenum both[] { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
            glDrawBuffers(2, both);
            return;
        }
        s_enabled = Mod::get()->getSettingValue<bool>("enabled");
        bool parallax = Mod::get()->getSettingValue<bool>("parallax");
        bool debugMode = Mod::get()->getSettingValue<bool>("debug");
        parallax::s_debug = debugMode && Mod::get()->getSettingValue<bool>("debug-parallax");
        if (!s_enabled && !debugMode)
            return CCNode::visit();
        const bool useParallax = s_enabled && parallax || parallax::s_debug;

        startMod();

        if (s_enabled) {
            glBindFramebuffer(GL_FRAMEBUFFER, s_fbo);
            constexpr GLenum both[] { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
            glDrawBuffers(2, both);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        }

        GJBaseGameLayer* bgl = PlayLayer::get();
        if (!bgl)
            bgl = LevelEditorLayer::get();

        if (useParallax)
            parallax::apply(bgl);

        CCNode::visit();

        if (useParallax)
            parallax::cleanup();

        endMod();

        if (s_enabled) {
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
        }

        parallax::s_debug = false;
    }
};
