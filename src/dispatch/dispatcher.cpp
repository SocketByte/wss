#include "dispatcher.h"

void WSS::Dispatcher::InitCommands(CLI::App& app) {
    const auto dispatch = app.add_subcommand("dispatch", "Run the WSS dispatch server");

    const auto show = dispatch->add_subcommand("show", "Show a widget");
    show->add_option("widget", "The name of the widget to show")->required();
    show->add_option("monitor", "The monitor ID to show the widget on")->default_val(0);
    show->callback([this, show]() {
        std::string widgetName = show->get_option("widget")->as<std::string>();
        int monitorId = show->get_option("monitor")->as<int>();

        json payload = {{"widgetName", widgetName}, {"monitorId", monitorId}, {"visible", true}};

        m_ZMQReq.Request("widget-set-visible", payload);
    });

    const auto hide = dispatch->add_subcommand("hide", "Hide a widget");
    hide->add_option("widget", "The name of the widget to hide")->required();
    hide->add_option("monitor", "The monitor ID to show the widget on")->default_val(0);
    hide->callback([this, hide]() {
        std::string widgetName = hide->get_option("widget")->as<std::string>();
        int monitorId = hide->get_option("monitor")->as<int>();

        json payload = {{"widgetName", widgetName}, {"monitorId", monitorId}, {"visible", false}};

        m_ZMQReq.Request("widget-set-visible", payload);
    });

    const auto toggle = dispatch->add_subcommand("toggle", "Toggle visibility of a widget");
    toggle->add_option("widget", "The name of the widget to hide")->required();
    toggle->add_option("monitor", "The monitor ID to show the widget on")->default_val(0);
    toggle->callback([this, toggle]() {
        std::string widgetName = toggle->get_option("widget")->as<std::string>();
        int monitorId = toggle->get_option("monitor")->as<int>();

        json payload = {{"widgetName", widgetName}, {"monitorId", monitorId}};

        m_ZMQReq.Request("widget-toggle-visible", payload);
    });
}