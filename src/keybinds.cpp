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
        "distance-dec"_spr,
        "Decrease Distance",
        "",
        { Keybind::create(KEY_LeftBracket, Modifier::None) },
        "SBS"
    });
    BindManager::get()->registerBindable({
        "distance-inc"_spr,
        "Increase Distance",
        "",
        { Keybind::create(KEY_RightBracket, Modifier::None) },
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
        mod->setSettingValue("distance", mod->getSettingValue<double>("distance") - 0.1);
        return ListenerResult::Propagate;
    }, InvokeBindFilter(nullptr, "distance-dec"_spr));

    new EventListener([=](const InvokeBindEvent* event) {
        if (!event->isDown())
            return ListenerResult::Propagate;
        auto* mod = Mod::get();
        mod->setSettingValue("distance", mod->getSettingValue<double>("distance") + 0.1);
        return ListenerResult::Propagate;
    }, InvokeBindFilter(nullptr, "distance-inc"_spr));
}
