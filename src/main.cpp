#include <Geode/Geode.hpp>

using namespace geode::prelude;

#include "Shader.hpp"

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
    } s_depthStencil;
    GLuint s_depthStencils[2];
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
    if (s_depthStencils[0] || s_depthStencils[1])
        glDeleteRenderbuffers(2, s_depthStencils);
    s_depthStencils[0] = 0;
    s_depthStencils[1] = 0;
    if (s_colors[0])
        ccGLDeleteTextureN(0, s_colors[0]);
    if (s_colors[1])
        ccGLDeleteTextureN(1, s_colors[1]);
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
        glGenRenderbuffers(2, s_depthStencils);

        ccGLBindTexture2D(s_color.left);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        ccGLBindTexture2D(s_color.right);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glBindRenderbuffer(GL_RENDERBUFFER, s_depthStencil.left);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, w, h);

        glBindRenderbuffer(GL_RENDERBUFFER, s_depthStencil.right);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, w, h);

        bool incomplete = false;

        glBindFramebuffer(GL_FRAMEBUFFER, s_fbo.left);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, s_color.left, 0);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, s_depthStencil.left, 0);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            log::error("left framebuffer incomplete, cleaning up");
            incomplete = true;
        }

        glBindFramebuffer(GL_FRAMEBUFFER, s_fbo.right);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, s_color.right, 0);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, s_depthStencil.right, 0);
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

double s_parallaxDistance;

struct ParallaxData {
    double parallax;
    CCPoint start;
    CCPoint offset;
};
std::unordered_map<CCNode*, ParallaxData> s_parallax;
void addParallax(CCNode* node, double parallax) {
    if (!node)
        return;
    if (parallax == 0.0)
        return;

    ParallaxData data;
    if (!s_parallax.contains(node)) {
        data = {
            0.0,
            node->getPosition(),
            { 0.0, 0.0 }
        };
    }
    else {
        data = s_parallax[node];
    }

    data.parallax += parallax;

    node->setPosition(data.start);
    float offset = static_cast<float>(data.parallax * s_parallaxDistance);
    if (auto* parent = node->getParent())
        data.offset = parent->convertToNodeSpace(parent->convertToWorldSpace(data.start) + CCPoint{ offset, 0.0 }) - data.start;
    else
        data.offset = CCPoint{ offset, 0.0 };
    node->setPosition(data.start + data.offset);

    if (data.parallax == 0.0) {
        s_parallax.erase(node);
        return;
    }

    s_parallax[node] = data;
}

struct ParallaxDataGameObject {
    double parallax;
    double start;
    double offset;
};
std::unordered_map<GameObject*, ParallaxDataGameObject> s_parallaxGameObject;
void addParallaxGameObject(GameObject* node, double parallax) {
    if (!node)
        return;
    if (parallax == 0.0)
        return;

    ParallaxDataGameObject data;
    if (!s_parallaxGameObject.contains(node)) {
        data = {
            0.0,
            node->m_positionX,
            0.0
        };
    }
    else {
        data = s_parallaxGameObject[node];
    }

    data.parallax += parallax;

    data.offset = data.parallax * s_parallaxDistance;
    node->m_positionX = data.start + data.offset;

    if (data.parallax == 0.0) {
        s_parallaxGameObject.erase(node);
        return;
    }

    s_parallaxGameObject[node] = data;
}

std::unordered_map<int, double> s_groupParallax;
void applyParallax1(GJBaseGameLayer* pl) {
    s_parallaxDistance = Mod::get()->getSettingValue<double>("distance");
    if (const auto* menu = MenuLayer::get()) {
        if (menu->m_menuGameLayer && menu->m_menuGameLayer->m_backgroundSprite) {
            addParallax(menu->m_menuGameLayer->m_backgroundSprite, 0.1 - 1.0);
        }
    }
    if (!pl)
        return;
    CCDictionaryExt<int, CCArray*> groupDict = pl->m_groupDict;
    for (const auto& command : pl->m_effectManager->m_unkVector560) {
        if (command.m_lockToCameraX) {
            if (!s_groupParallax.contains(command.m_targetGroupID))
                s_groupParallax[command.m_targetGroupID] = 0.0f;
            s_groupParallax[command.m_targetGroupID] += command.m_moveModX;
        }
        // TODO: better heuristics
        // this just checks if its a 2.2 level because in 2.2 ppl already started using
        // locking to camera to lock things to camera
        // while in 2.1 the only way to lock to camera x was to lock to player x
        // this also can't detect if there's another move trigger alongside the
        // one that's setup to follow the camera
        if (pl->m_level->m_gameVersion >= 22)
            continue;
        if (command.m_lockToPlayerX && !command.m_lockToPlayerY) {
            if (!s_groupParallax.contains(command.m_targetGroupID))
                s_groupParallax[command.m_targetGroupID] = 0.0f;
            s_groupParallax[command.m_targetGroupID] += command.m_moveModX;
        }
    }
    for (const auto& [ id, parallax ] : s_groupParallax) {
        if (!groupDict.contains(id))
            continue;
        for (const auto& obj : CCArrayExt<GameObject*>(groupDict[id])) {
            addParallaxGameObject(obj, -parallax);
        }
    }
    s_groupParallax.clear();
    pl->updateGradientLayers();
}

void applyParallax2(GJBaseGameLayer* pl) {
    for (const auto& [ node, data ] : s_parallax) {
        node->setPosition(data.start - data.offset);
    }
    for (const auto& [ node, data ] : s_parallaxGameObject) {
        node->m_positionX = data.start - data.offset;
    }
    if (pl)
        pl->updateGradientLayers();
}

void applyParallax3() {
    for (const auto& [ node, data ] : s_parallax) {
        node->setPosition(data.start);
    }
    for (const auto& [ node, data ] : s_parallaxGameObject) {
        node->m_positionX = data.start;
    }
    s_parallax.clear();
    s_parallaxGameObject.clear();
}

bool s_firstVisit = false;

#include <Geode/modify/CCNode.hpp>
class $modify(CCNode) {
    $override void visit() {
        if (s_firstVisit) {
            if (auto* self = typeinfo_cast<CCMenuItemSpriteExtra*>(this)) {
                addParallax(this, self->getScaleX() / self->m_baseScale - 1.0);
            }
            else if (auto* self = typeinfo_cast<GJBaseGameLayer*>(this)) {
                // TODO: fix background parallax idk
                addParallax(self->m_background, 0.1 - 1.0);
                addParallax(self->m_middleground, 0.25 - 1.0);
                for (const auto& [ node, data ] : s_parallaxGameObject) {
                    addParallax(node, data.parallax);
                }
            }
        }
        CCNode::visit();
    }
};

// TODO: make more compatible
#include <Geode/modify/CCDirector.hpp>
class $modify(CCDirector) {
    $override void drawScene() {
        if (!Mod::get()->getSettingValue<bool>("enabled")) {
            CCDirector::drawScene();
            return;
        }
        const bool useParallax = Mod::get()->getSettingValue<bool>("parallax");

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

        GJBaseGameLayer* pl = PlayLayer::get();
        if (!pl)
            pl = LevelEditorLayer::get();

        if (useParallax) {
            applyParallax1(pl);
            s_firstVisit = true;
        }

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

        if (useParallax) {
            s_firstVisit = false;
            applyParallax2(pl);
        }

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

        if (useParallax) {
            applyParallax3();
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
