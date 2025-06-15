#include <Geode/Geode.hpp>
#include <numbers>

using namespace geode::prelude;

#include "parallax.hpp"

std::set<CCNode*> s_hasParallax;

#include <Geode/modify/CCNode.hpp>
class $modify(ParallaxNode, CCNode) {
    struct Fields {
        float m_ownParallax = 0.0f;
        float m_parallax = 0.0f;
        bool m_hasParallaxCache = false;
    };

    float getParallax() {
        if (m_fields->m_hasParallaxCache)
            return m_fields->m_parallax;
        m_fields->m_parallax = m_fields->m_ownParallax;
        if (this->getParent()) {
            m_fields->m_parallax += static_cast<ParallaxNode*>(this->getParent())->getParallax();
        }
        s_hasParallax.emplace(this);
        m_fields->m_hasParallaxCache = true;
        return m_fields->m_parallax;
    }

    void destructor() {
        s_hasParallax.erase(this);
        CCNode::~CCNode();
    }
};

void addParallax(CCNode* node, float parallax) {
    if (!node)
        return;
    if (parallax == 0.0f)
        return;
    float& nodeParallax = static_cast<ParallaxNode*>(node)->m_fields->m_ownParallax;
    nodeParallax += parallax;
    if (nodeParallax != 0.0f)
        s_hasParallax.emplace(node);
}

float s_parallaxMod = 0.0f;
float s_parallaxDistance = 0.0f;
std::unordered_map<int, float> s_groupParallax;
std::unordered_map<int, float> s_groupNonParallax;
std::vector<const GroupCommandObject2*> s_tryLater;
std::vector<const GroupCommandObject2*> s_tryLater2;
size_t s_maxIter = 0;
GJBaseGameLayer* s_bgl = nullptr;
float s_prevCamPos = 0.0f;
float s_lastCamDelta = 0.0f;
void applyParallax(GJBaseGameLayer* gl) {
    if (!s_debug)
        s_parallaxDistance = Mod::get()->getSettingValue<float>("distance");
    s_maxIter = Mod::get()->getSettingValue<size_t>("parallax-max-iter");
    if (const auto* menu = MenuLayer::get()) {
        if (menu->m_menuGameLayer && menu->m_menuGameLayer->m_backgroundSprite) {
            addParallax(menu->m_menuGameLayer->m_backgroundSprite, 0.1f - 1.0f);
        }
    }
    if (!gl)
        return;
    addParallax(gl->m_background, 0.1f - 1.0f);
    addParallax(gl->m_middleground, 0.25f - 1.0f);
    // TODO: better heuristics
    float camPos = gl->m_gameState.m_cameraPosition.x;
    float camDelta = camPos - s_prevCamPos;
    if (camDelta != 0.0f)
        s_lastCamDelta = camDelta;
    s_prevCamPos = camPos;
    CCDictionaryExt<int, CCArray*> groupDict = gl->m_groupDict;
    constexpr int MoveCommandType = 0;
    constexpr int FollowCommandType = 2;
    size_t successful = 0;
    std::vector<const GroupCommandObject2*>& tryLater = s_tryLater;
    std::vector<const GroupCommandObject2*>& otherTryLater = s_tryLater2;
    for (const auto& command : gl->m_effectManager->m_unkVector560) {
        if (command.m_commandType != MoveCommandType) {
            tryLater.push_back(&command);
            continue;
        }
        if (command.m_lockToCameraX) {
            if (!s_groupParallax.contains(command.m_targetGroupID))
                s_groupParallax[command.m_targetGroupID] = 0.0f;
            s_groupParallax[command.m_targetGroupID] += static_cast<float>(command.m_moveModX);
            successful++;
            continue;
        }
        // TODO: handle cases where player x != camera x
        if (command.m_lockToPlayerX && !command.m_lockToPlayerY) {
            if (!s_groupParallax.contains(command.m_targetGroupID))
                s_groupParallax[command.m_targetGroupID] = 0.0f;
            s_groupParallax[command.m_targetGroupID] += static_cast<float>(command.m_moveModX);
            successful++;
            continue;
        }
        tryLater.push_back(&command);
    }
    size_t i = 1;
    while (i != s_maxIter) {
        std::swap(tryLater, otherTryLater);
        bool last = successful == 0;
        successful = 0;
        for (const auto& command : otherTryLater) {
            if (last) {
                //log::debug("{}", command->m_targetGroupID);
                s_groupNonParallax[command->m_targetGroupID] += 1.0f - (s_lastCamDelta - static_cast<float>(
                    command->m_oldDeltaX + command->m_oldDeltaX_3)) / s_lastCamDelta;
                successful++;
                //switch (command->m_commandType) {
                //    case MoveCommandType:
                //        // TODO: handle non-linear easing
                //        s_groupNonParallax[command->m_targetGroupID] += static_cast<float>(command->m_moveOffset.x / command->m_duration);
                //        break;
                //    case FollowCommandType:
                //        s_groupNonParallax[command->m_targetGroupID] += s_groupNonParallax[command->m_centerGroupID] *
                //            static_cast<float>(command->m_followXMod);
                //        break;
                //    default:
                //        break;
                //}
                continue;
            }
            switch (command->m_commandType) {
                case MoveCommandType:
                    if (!s_groupParallax.contains(command->m_targetGroupID)) {
                        if (!s_groupNonParallax.contains(command->m_targetGroupID)) {
                            tryLater.push_back(command);
                            continue;
                        }
                        s_groupNonParallax[command->m_targetGroupID] += static_cast<float>(command->m_moveOffset.x / command->m_duration);
                        successful++;
                        continue;
                    }
                    // TODO: handle non-linear easing
                    s_groupParallax[command->m_targetGroupID] += static_cast<float>(command->m_moveOffset.x / command->m_duration);
                    successful++;
                    break;
                case FollowCommandType:
                    if (!s_groupParallax.contains(command->m_centerGroupID)) {
                        if (!s_groupNonParallax.contains(command->m_centerGroupID)) {
                            tryLater.push_back(command);
                            continue;
                        }
                        s_groupNonParallax[command->m_targetGroupID] += s_groupNonParallax[command->m_centerGroupID] * static_cast<float>(command->m_followXMod);
                        successful++;
                        continue;
                    }
                    s_groupParallax[command->m_targetGroupID] += s_groupParallax[command->m_centerGroupID] * static_cast<float>(command->m_followXMod);
                    successful++;
                    break;
                default:
                    break;
            }
        }
        otherTryLater.clear();
        i++;
        if (last && successful == 0)
            break;
    }
    //log::debug("iterations: {}", i);
    //if (gl->m_level->m_gameVersion < 22) {
    //    for (const auto& command : gl->m_effectManager->m_unkVector560) {
    //        if (!s_groupParallax.contains(command.m_targetGroupID))
    //            continue;
    //        //if (command.m_lockToPlayerX && !command.m_lockToPlayerY)
    //        //    continue;
    //        s_groupParallax[command.m_targetGroupID] += 1.0f - (s_lastCamDelta - static_cast<float>(command.m_oldDeltaX + command.m_oldDeltaX_3)) / s_lastCamDelta;
    //    }
    //}
    for (const auto& [ id, parallax ] : s_groupParallax) {
        if (!groupDict.contains(id))
            continue;
        for (const auto& obj : CCArrayExt<GameObject*>(groupDict[id])) {
            float final = parallax;
            for (short j = 0; j < obj->m_groupCount; j++) {
                const short& group = obj->m_groups->at(j);
                if (s_groupNonParallax.contains(group))
                    final += s_groupNonParallax[group];
            }
            addParallax(obj, -final);
            addParallax(obj->m_particle, -final);
            addParallax(obj->m_glowSprite, -final);
        }
    }
    s_groupParallax.clear();
    s_groupNonParallax.clear();
    s_bgl = gl;
}

class $modify(CCNode) {
    $override void visit() {
        if (s_parallaxDistance == 0.0f && !s_debug)
            return CCNode::visit();
        if (auto* mise = typeinfo_cast<CCMenuItemSpriteExtra*>(this)) {
            addParallax(mise, mise->getScaleX() / mise->m_baseScale - 1.0f);
        }
        CCNode::visit();
    }
};

void cleanupParallax() {
    s_bgl = nullptr;
    for (const auto& node : s_hasParallax) {
        static_cast<ParallaxNode*>(node)->m_fields->m_ownParallax = 0.0f;
        static_cast<ParallaxNode*>(node)->m_fields->m_hasParallaxCache = false;
    }
    s_hasParallax.clear();
    s_parallaxDistance = 0.0f;
}

ccVertex3F getParallaxOffset(CCNode* node) {
    if (!node)
        return { 0.0f, 0.0f, 0.0f };
    float z = static_cast<ParallaxNode*>(node)->getParallax();
    const float offset = z * s_parallaxDistance * s_parallaxMod;
    if (!s_debug)
        z = 0.0f;
    if (offset == 0.0f)
        return { 0.0f, 0.0f, z };
    kmMat4 mat;
    kmGLGetMatrix(KM_GL_MODELVIEW, &mat);
    const float d = mat.mat[0] * mat.mat[5] - mat.mat[1] * mat.mat[4];
    return { mat.mat[5] / d * offset, -mat.mat[1] / d * offset, z };
}

void drawBoth() {
    s_parallaxMod = 0.0f;
    constexpr GLenum buffers[] { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    glDrawBuffers(2, buffers);
}
void drawLeft() {
    s_parallaxMod = 1.0f;
    constexpr GLenum buffers[] { GL_COLOR_ATTACHMENT0 };
    glDrawBuffers(1, buffers);
}
void drawRight() {
    s_parallaxMod = -1.0f;
    constexpr GLenum buffers[] { GL_COLOR_ATTACHMENT1 };
    glDrawBuffers(1, buffers);
}

#include <Geode/modify/CCSprite.hpp>
class $modify(CCSprite) {
    struct Fields {
        ccVertex3F m_quadOffset = { 0.0f, 0.0f, 0.0f };
    };

    void offsetQuad() {
        const auto offset = getParallaxOffset(this);
        if (m_bShouldBeHidden) {
            m_fields->m_quadOffset = { 0.0f, 0.0f, 0.0f };
            return;
        }
        if (offset.x == 0.0f && offset.y == 0.0f && offset.z == 0.0f) {
            if (m_fields->m_quadOffset.x != 0.0f || m_fields->m_quadOffset.y != 0.0f ||
                m_fields->m_quadOffset.z != 0.0f) {
                if (m_pobTextureAtlas && m_uAtlasIndex != CCSpriteIndexNotInitialized)
                    m_pobTextureAtlas->updateQuad(&m_sQuad, m_uAtlasIndex);
                else
                    this->setDirty(true);
            }
            m_fields->m_quadOffset = offset;
            return;
        }

        m_sQuad.tl.vertices.x += offset.x;
        m_sQuad.tl.vertices.y += offset.y;
        m_sQuad.tl.vertices.z += offset.z;
        m_sQuad.bl.vertices.x += offset.x;
        m_sQuad.bl.vertices.y += offset.y;
        m_sQuad.bl.vertices.z += offset.z;
        m_sQuad.tr.vertices.x += offset.x;
        m_sQuad.tr.vertices.y += offset.y;
        m_sQuad.tr.vertices.z += offset.z;
        m_sQuad.br.vertices.x += offset.x;
        m_sQuad.br.vertices.y += offset.y;
        m_sQuad.br.vertices.z += offset.z;

        if (m_pobTextureAtlas && m_uAtlasIndex != CCSpriteIndexNotInitialized)
            m_pobTextureAtlas->updateQuad(&m_sQuad, m_uAtlasIndex);
        else
            this->setDirty(true);
        m_fields->m_quadOffset = offset;
    }

    void resetQuad() {
        const auto off = m_fields->m_quadOffset;
        if (off.x == 0.0f && off.y == 0.0f && off.z == 0.0f) {
            return;
        }

        m_sQuad.tl.vertices.x -= off.x;
        m_sQuad.tl.vertices.y -= off.y;
        m_sQuad.tl.vertices.z -= off.z;
        m_sQuad.bl.vertices.x -= off.x;
        m_sQuad.bl.vertices.y -= off.y;
        m_sQuad.bl.vertices.z -= off.z;
        m_sQuad.tr.vertices.x -= off.x;
        m_sQuad.tr.vertices.y -= off.y;
        m_sQuad.tr.vertices.z -= off.z;
        m_sQuad.br.vertices.x -= off.x;
        m_sQuad.br.vertices.y -= off.y;
        m_sQuad.br.vertices.z -= off.z;
    }

    void drawHook() {
        this->offsetQuad();
        CCSprite::draw();
        this->resetQuad();
    }

    $override void draw() {
        // ShaderLayer fix
        auto* sl = typeinfo_cast<ShaderLayer*>(this->getParent());
        if (sl && this == sl->m_sprite)
            return CCSprite::draw();

        if (s_parallaxDistance == 0.0f)
            return this->drawHook();

        drawLeft();
        this->drawHook();

        drawRight();
        this->drawHook();

        drawBoth();
    }

    // this is a virtual in CCNode but it's only overridden in CCSprite, so should be fine
    $override void updateTransform() {
        this->resetQuad();
        CCSprite::updateTransform();
        this->offsetQuad();
    }
};

#include <Geode/modify/CCSpriteBatchNode.hpp>
class $modify(CCSpriteBatchNode) {
    $override void draw() {
        if (s_parallaxDistance == 0.0f)
            return CCSpriteBatchNode::draw();

        drawLeft();
        CCSpriteBatchNode::draw();

        drawRight();
        CCSpriteBatchNode::draw();

        drawBoth();
    }
};

#include <Geode/modify/CCLayerColor.hpp>
class $modify(CCLayerColor) {
    struct Fields {
        ccVertex3F m_myEpicVertices[4];
    };
    void offsetBottomLeft(const float& x, const float& y) {
        m_pSquareVertices[0].x += x;
        m_pSquareVertices[0].y += y;
    }
    void offsetBottomRight(const float& x, const float& y) {
        m_pSquareVertices[1].x += x;
        m_pSquareVertices[1].y += y;
    }
    void offsetTopLeft(const float& x, const float& y) {
        m_pSquareVertices[2].x += x;
        m_pSquareVertices[2].y += y;
    }
    void offsetTopRight(const float& x, const float& y) {
        m_pSquareVertices[3].x += x;
        m_pSquareVertices[3].y += y;
    }

    void drawDebug(float bl, float br, float tl, float tr) {
        m_fields->m_myEpicVertices[0].x = m_pSquareVertices[0].x;
        m_fields->m_myEpicVertices[0].y = m_pSquareVertices[0].y;
        m_fields->m_myEpicVertices[0].z = bl;

        m_fields->m_myEpicVertices[1].x = m_pSquareVertices[1].x;
        m_fields->m_myEpicVertices[1].y = m_pSquareVertices[1].y;
        m_fields->m_myEpicVertices[1].z = br;

        m_fields->m_myEpicVertices[2].x = m_pSquareVertices[2].x;
        m_fields->m_myEpicVertices[2].y = m_pSquareVertices[2].y;
        m_fields->m_myEpicVertices[2].z = tl;

        m_fields->m_myEpicVertices[3].x = m_pSquareVertices[3].x;
        m_fields->m_myEpicVertices[3].y = m_pSquareVertices[3].y;
        m_fields->m_myEpicVertices[3].z = tr;

        this->getShaderProgram()->use();
        this->getShaderProgram()->setUniformsForBuiltins();
        ccGLEnableVertexAttribs(3);
        glVertexAttribPointer(0, 3, 0x1406, false, 0, &m_fields->m_myEpicVertices);
        glVertexAttribPointer(1, 4, 0x1406, false, 0, &m_pSquareColors);
        ccGLBlendFunc(m_tBlendFunc.src, m_tBlendFunc.dst);
        glDrawArrays(5, 0, 4);
        CC_INCREMENT_GL_DRAWS(1);
    }

    void drawHook(const GJGradientLayer* self) {
        if (!s_bgl)
            return CCLayerColor::draw();

        const auto* trigger = self->m_triggerObject;
        if (trigger->m_disable)
            return CCLayerColor::draw();

        const auto ubl = getParallaxOffset(s_bgl->tryGetMainObject(trigger->m_upBottomLeftID));
        const auto dbr = getParallaxOffset(s_bgl->tryGetMainObject(trigger->m_downBottomRightID));
        const auto ltl = getParallaxOffset(s_bgl->tryGetMainObject(trigger->m_leftTopLeftID));
        const auto rtr = getParallaxOffset(s_bgl->tryGetMainObject(trigger->m_rightTopRightID));

        if (trigger->m_vertexMode) {
            if (s_debug) {
                this->drawDebug(ubl.z, dbr.z, ltl.z, rtr.z);
                return;
            }

            offsetBottomLeft(ubl.x, ubl.y);
            offsetBottomRight(dbr.x, dbr.y);
            offsetTopLeft(ltl.x, ltl.y);
            offsetTopRight(rtr.x, rtr.y);

            CCLayerColor::draw();

            offsetBottomLeft(-ubl.x, -ubl.y);
            offsetBottomRight(-dbr.x, -dbr.y);
            offsetTopLeft(-ltl.x, -ltl.y);
            offsetTopRight(-rtr.x, -rtr.y);
        }
        else {
            // dont bother, idk how this would work
            if (s_debug)
                return CCLayerColor::draw();

            const float up = ubl.y;
            const float down = dbr.y;
            const float left = ltl.x;
            const float right = rtr.x;

            offsetBottomLeft(left, down);
            offsetBottomRight(right, down);
            offsetTopLeft(left, up);
            offsetTopRight(right, up);

            CCLayerColor::draw();

            offsetBottomLeft(-left, -down);
            offsetBottomRight(-right, -down);
            offsetTopLeft(-left, -up);
            offsetTopRight(-right, -up);
        }
    }

    $override void draw() {
        auto* self = typeinfo_cast<GJGradientLayer*>(this);
        if (!self)
            return CCLayerColor::draw();

        if (s_parallaxDistance == 0.0f)
            return this->drawHook(self);

        drawLeft();
        this->drawHook(self);

        drawRight();
        this->drawHook(self);

        drawBoth();
    }
};

#include <Geode/modify/CCParticleSystemQuad.hpp>
class $modify(CCParticleSystemQuad) {
    $override void draw() {
        // TODO: make debug mode not  have to do this 👍
        if (s_debug) {
            const auto offset = getParallaxOffset(this);
            if (offset.z == 0.0f)
                return CCParticleSystemQuad::draw();
            for (size_t i = 0; i < m_uParticleCount; i++) {
                m_pQuads[i].tl.vertices.z += offset.z;
                m_pQuads[i].bl.vertices.z += offset.z;
                m_pQuads[i].tr.vertices.z += offset.z;
                m_pQuads[i].br.vertices.z += offset.z;
            }
            CCParticleSystemQuad::postStep();
            for (size_t i = 0; i < m_uParticleCount; i++) {
                m_pQuads[i].tl.vertices.z -= offset.z;
                m_pQuads[i].bl.vertices.z -= offset.z;
                m_pQuads[i].tr.vertices.z -= offset.z;
                m_pQuads[i].br.vertices.z -= offset.z;
            }
            return CCParticleSystemQuad::draw();
        }

        if (s_parallaxDistance == 0.0f)
            return CCParticleSystemQuad::draw();

        drawLeft();
        kmGLPushMatrix();
        const auto left = getParallaxOffset(this);
        kmGLTranslatef(left.x, left.y, 0.0f);
        CCParticleSystemQuad::draw();
        kmGLPopMatrix();

        drawRight();
        kmGLPushMatrix();
        const auto right = getParallaxOffset(this);
        kmGLTranslatef(right.x, right.y, 0.0f);
        CCParticleSystemQuad::draw();
        kmGLPopMatrix();

        drawBoth();
    }
};
