#include <Geode/Geode.hpp>
#include <numbers>

using namespace geode::prelude;

#include "parallax.hpp"

float s_parallaxMod = 0.0f;
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

class $modify(CCNode) {
    $override void visit() {
        if (auto* mise = typeinfo_cast<CCMenuItemSpriteExtra*>(this)) {
            addParallax(mise, mise->getScaleX() / mise->m_baseScale - 1.0f);
        }
        CCNode::visit();
    }
};

std::unordered_map<int, float> s_groupParallax;
GJBaseGameLayer* s_bgl = nullptr;
void applyParallax1(GJBaseGameLayer* bgl) {
    s_parallaxMod = Mod::get()->getSettingValue<float>("distance");
    if (const auto* menu = MenuLayer::get()) {
        if (menu->m_menuGameLayer && menu->m_menuGameLayer->m_backgroundSprite) {
            addParallax(menu->m_menuGameLayer->m_backgroundSprite, 0.1f - 1.0f);
        }
    }
    if (!bgl)
        return;
    addParallax(bgl->m_background, 0.1f - 1.0f);
    addParallax(bgl->m_middleground, 0.25f - 1.0f);
    CCDictionaryExt<int, CCArray*> groupDict = bgl->m_groupDict;
    for (const auto& command : bgl->m_effectManager->m_unkVector560) {
        if (command.m_lockToCameraX) {
            if (!s_groupParallax.contains(command.m_targetGroupID))
                s_groupParallax[command.m_targetGroupID] = 0.0f;
            s_groupParallax[command.m_targetGroupID] += static_cast<float>(command.m_moveModX);
        }
        // TODO: better heuristics
        // this just checks if its a 2.2 level because in 2.2 ppl already started using
        // locking to camera to lock things to camera
        // while in 2.1 the only way to lock to camera x was to lock to player x
        // this also can't detect if there's another move trigger alongside the
        // one that's setup to follow the camera
        if (bgl->m_level->m_gameVersion >= 22)
            continue;
        if (command.m_lockToPlayerX && !command.m_lockToPlayerY) {
            if (!s_groupParallax.contains(command.m_targetGroupID))
                s_groupParallax[command.m_targetGroupID] = 0.0f;
            s_groupParallax[command.m_targetGroupID] += static_cast<float>(command.m_moveModX);
        }
    }
    for (const auto& [ id, parallax ] : s_groupParallax) {
        if (!groupDict.contains(id))
            continue;
        for (const auto& obj : CCArrayExt<GameObject*>(groupDict[id])) {
            addParallax(obj, -parallax);
        }
    }
    s_groupParallax.clear();
    s_bgl = bgl;
}

void applyParallax2() {
    s_parallaxMod = -s_parallaxMod;
}

void applyParallax3() {
    s_parallaxMod = 0.0;
    for (const auto& node : s_hasParallax) {
        static_cast<ParallaxNode*>(node)->m_fields->m_ownParallax = 0.0f;
        static_cast<ParallaxNode*>(node)->m_fields->m_hasParallaxCache = false;
    }
    s_hasParallax.clear();
    s_bgl = nullptr;
}

CCPoint getParallaxOffset(CCNode* node) {
    if (!node)
        return { 0.0f, 0.0f };
    const float offset = static_cast<ParallaxNode*>(node)->getParallax() * s_parallaxMod;
    if (offset == 0.0f)
        return { 0.0f, 0.0f };
    kmMat4 mat;
    kmGLGetMatrix(KM_GL_MODELVIEW, &mat);
    const float d = mat.mat[0] * mat.mat[5] - mat.mat[1] * mat.mat[4];
    return { mat.mat[5] / d * offset, -mat.mat[1] / d * offset };
}

#include <Geode/modify/CCSprite.hpp>
class $modify(CCSprite) {
    struct Fields {
        CCPoint m_quadOffset = { 0.0f, 0.0f };
    };

    void offsetQuad() {
        const auto offset = getParallaxOffset(this);
        if (m_bShouldBeHidden) {
            m_fields->m_quadOffset = CCPoint{ 0.0f, 0.0f };
            return;
        }
        if (offset.isZero()) {
            if (!m_fields->m_quadOffset.isZero()) {
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
        m_sQuad.bl.vertices.x += offset.x;
        m_sQuad.bl.vertices.y += offset.y;
        m_sQuad.tr.vertices.x += offset.x;
        m_sQuad.tr.vertices.y += offset.y;
        m_sQuad.br.vertices.x += offset.x;
        m_sQuad.br.vertices.y += offset.y;
        if (m_pobTextureAtlas && m_uAtlasIndex != CCSpriteIndexNotInitialized)
            m_pobTextureAtlas->updateQuad(&m_sQuad, m_uAtlasIndex);
        else
            this->setDirty(true);
        m_fields->m_quadOffset = offset;
    }

    void resetQuad() {
        const auto off = m_fields->m_quadOffset;
        if (off.isZero()) {
            return;
        }
        m_sQuad.tl.vertices.x -= off.x;
        m_sQuad.tl.vertices.y -= off.y;
        m_sQuad.bl.vertices.x -= off.x;
        m_sQuad.bl.vertices.y -= off.y;
        m_sQuad.tr.vertices.x -= off.x;
        m_sQuad.tr.vertices.y -= off.y;
        m_sQuad.br.vertices.x -= off.x;
        m_sQuad.br.vertices.y -= off.y;
    }

    $override void draw() {
        this->offsetQuad();
        CCSprite::draw();
        this->resetQuad();
    }

    // this is a virtual in CCNode but it's only overridden in CCSprite, so should be fine
    $override void updateTransform() {
        this->resetQuad();
        CCSprite::updateTransform();
        this->offsetQuad();
    }
};

#include <Geode/modify/CCLayerColor.hpp>
class $modify(CCLayerColor) {
    void offsetBottomLeft(const CCPoint& offset) {
        m_pSquareVertices[0].x += offset.x;
        m_pSquareVertices[0].y += offset.y;
    }
    void offsetBottomRight(const CCPoint& offset) {
        m_pSquareVertices[1].x += offset.x;
        m_pSquareVertices[1].y += offset.y;
    }
    void offsetTopLeft(const CCPoint& offset) {
        m_pSquareVertices[2].x += offset.x;
        m_pSquareVertices[2].y += offset.y;
    }
    void offsetTopRight(const CCPoint& offset) {
        m_pSquareVertices[3].x += offset.x;
        m_pSquareVertices[3].y += offset.y;
    }

    $override void draw() {
        auto* self = typeinfo_cast<GJGradientLayer*>(this);
        if (!self || !s_bgl)
            return CCLayerColor::draw();
        const auto* trigger = self->m_triggerObject;
        if (trigger->m_disable)
            return CCLayerColor::draw();

        const auto ubl = getParallaxOffset(s_bgl->tryGetMainObject(trigger->m_upBottomLeftID));
        const auto dbr = getParallaxOffset(s_bgl->tryGetMainObject(trigger->m_downBottomRightID));
        const auto ltl = getParallaxOffset(s_bgl->tryGetMainObject(trigger->m_leftTopLeftID));
        const auto rtr = getParallaxOffset(s_bgl->tryGetMainObject(trigger->m_rightTopRightID));

        if (trigger->m_vertexMode) {
            offsetBottomLeft(ubl);
            offsetBottomRight(dbr);
            offsetTopLeft(ltl);
            offsetTopRight(rtr);

            CCLayerColor::draw();

            offsetBottomLeft(-ubl);
            offsetBottomRight(-dbr);
            offsetTopLeft(-ltl);
            offsetTopRight(-rtr);
        }
        else {
            const float up = ubl.y;
            const float down = dbr.y;
            const float left = ltl.x;
            const float right = rtr.x;

            offsetBottomLeft({ left, down });
            offsetBottomRight({ right, down });
            offsetTopLeft({ left, up });
            offsetTopRight({ right, up });

            CCLayerColor::draw();

            offsetBottomLeft({ -left, -down });
            offsetBottomRight({ -right, -down });
            offsetTopLeft({ -left, -up });
            offsetTopRight({ -right, -up });
        }
    }
};
