#include <Geode/Geode.hpp>

using namespace geode::prelude;

#include "parallax.hpp"

double s_parallaxDistance;

template <typename PositionType>
struct ParallaxData {
    double parallax;
    PositionType start;
    PositionType offset;
};

template <typename NodeType, typename PositionType>
void addParallax(std::unordered_map<NodeType*, ParallaxData<PositionType>>& map, NodeType* node, double parallax) {
    if (!node)
        return;
    if (parallax == 0.0)
        return;

    ParallaxData<PositionType> data;
    if (!map.contains(node)) {
        if constexpr (std::is_same_v<NodeType, CCNode>) {
            data = {
                0.0,
                node->getPosition(),
                { 0.0, 0.0 }
            };
        }
        else if constexpr (std::is_same_v<NodeType, GameObject>) {
            data = {
                0.0,
                node->m_positionX,
                0.0
            };
        }
    }
    else {
        data = map[node];
    }

    data.parallax += parallax;

    if constexpr (std::is_same_v<PositionType, CCPoint>) {
        node->setPosition(data.start);
        float offset = static_cast<float>(data.parallax * s_parallaxDistance);
        if (auto* parent = node->getParent())
            data.offset = parent->convertToNodeSpace(parent->convertToWorldSpace(data.start) + CCPoint{ offset, 0.0 }) - data.start;
        else
            data.offset = CCPoint{ offset, 0.0 };
        node->setPosition(data.start + data.offset);
    }
    else {
        data.offset = data.parallax * s_parallaxDistance;
        node->m_positionX = data.start + data.offset;
    }

    if (data.parallax == 0.0) {
        map.erase(node);
        return;
    }

    map[node] = data;
}

std::unordered_map<CCNode*, ParallaxData<CCPoint>> s_parallax;
void addParallax(CCNode* node, double parallax) {
    addParallax(s_parallax, node, parallax);
}

std::unordered_map<GameObject*, ParallaxData<double>> s_parallaxGameObject;
void addParallaxGameObject(GameObject* node, double parallax) {
    addParallax(s_parallaxGameObject, node, parallax);
}

bool s_firstVisit = false;

std::unordered_map<int, double> s_groupParallax;
void applyParallax1(GJBaseGameLayer* pl) {
    s_firstVisit = true;
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
    s_firstVisit = false;
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
