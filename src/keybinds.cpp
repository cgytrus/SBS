#include <geode.custom-keybinds/include/Keybinds.hpp>

using namespace geode::prelude;
using namespace keybinds;

$execute {
    BindManager::get()->registerBindable({
        "enabled"_spr,
        "Toggle SBS",
        "",
        { Keybind::create(KEY_O, Modifier::None) },
        "SBS"
    });
    BindManager::get()->registerBindable({
        "parallax"_spr,
        "Toggle Parallax",
        "",
        { Keybind::create(KEY_P, Modifier::None) },
        "SBS"
    });
    BindManager::get()->registerBindable({
        "parallax-distance-dec"_spr,
        "Decrease Parallax Distance",
        "",
        { Keybind::create(KEY_LeftBracket, Modifier::None) },
        "SBS"
    });
    BindManager::get()->registerBindable({
        "parallax-distance-inc"_spr,
        "Increase Parallax Distance",
        "",
        { Keybind::create(KEY_RightBracket, Modifier::None) },
        "SBS"
    });
    BindManager::get()->registerBindable({
        "parallax-debug"_spr,
        "Toggle Parallax Debug",
        "",
        { Keybind::create(KEY_P, Modifier::Alt) },
        "SBS"
    });

    new EventListener([=](const InvokeBindEvent* event) {
        if (!event->isDown())
            return ListenerResult::Propagate;
        auto* mod = Mod::get();
        mod->setSettingValue("enabled", !mod->getSettingValue<bool>("enabled"));
        return ListenerResult::Propagate;
    }, InvokeBindFilter(nullptr, "enabled"_spr));

    new EventListener([=](const InvokeBindEvent* event) {
        if (!event->isDown())
            return ListenerResult::Propagate;
        auto* mod = Mod::get();
        mod->setSettingValue("parallax", !mod->getSettingValue<bool>("parallax"));
        return ListenerResult::Propagate;
    }, InvokeBindFilter(nullptr, "parallax"_spr));

    new EventListener([=](const InvokeBindEvent* event) {
        if (!event->isDown())
            return ListenerResult::Propagate;
        auto* mod = Mod::get();
        mod->setSettingValue("parallax-distance", mod->getSettingValue<double>("parallax-distance") - 0.1);
        return ListenerResult::Propagate;
    }, InvokeBindFilter(nullptr, "parallax-distance-dec"_spr));

    new EventListener([=](const InvokeBindEvent* event) {
        if (!event->isDown())
            return ListenerResult::Propagate;
        auto* mod = Mod::get();
        mod->setSettingValue("parallax-distance", mod->getSettingValue<double>("parallax-distance") + 0.1);
        return ListenerResult::Propagate;
    }, InvokeBindFilter(nullptr, "parallax-distance-inc"_spr));

    new EventListener([=](const InvokeBindEvent* event) {
        if (!event->isDown())
            return ListenerResult::Propagate;
        auto* mod = Mod::get();
        mod->setSettingValue("parallax-debug", !mod->getSettingValue<bool>("parallax-debug"));
        return ListenerResult::Propagate;
    }, InvokeBindFilter(nullptr, "parallax-debug"_spr));
}
