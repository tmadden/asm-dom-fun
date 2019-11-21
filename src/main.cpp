// Copyright 2019 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

#include "asm-dom.hpp"

#include <emscripten/fetch.h>
#include <emscripten/val.h>

#include <functional>
#include <iostream>
#include <string>

#include <chrono>
#include <ctime>

#define ALIA_IMPLEMENTATION
#define ALIA_LOWERCASE_MACROS
#include "alia.hpp"

///////

using namespace alia;
using namespace alia::literals;

alia::system the_system;

asmdom::VNode* current_view = nullptr;

struct graph_placeholder
{
};
typedef graph_placeholder const* widget_id;

widget_id
get_widget_id(context ctx)
{
    graph_placeholder* id;
    get_cached_data(ctx, &id);
    return id;
}

struct refresh_event
{
    asmdom::Children* current_children;
};

struct click_event
{
    widget_id id;
};

using std::string;

struct value_update_event
{
    widget_id id;
    string value;
};

void
refresh();

void
do_text(context ctx, readable<std::string> text)
{
    handle_event<refresh_event>(ctx, [text](auto ctx, auto& e) {
        if (signal_is_readable(text))
        {
            e.current_children->push_back(asmdom::h("h4", read_signal(text)));
        }
    });
}

struct input_data
{
    captured_id external_id;
    string value;
};

void
do_number_input(context ctx, bidirectional<string> value)
{
    input_data* data;
    get_cached_data(ctx, &data);

    if (!data->external_id.matches(value.value_id()))
    {
        auto new_value
            = signal_is_readable(value) ? read_signal(value) : string();
        // It's possible that we actually caused the change in the external text
        // (e.g., when we're immediately sending out changes), so if we already
        // have the new value, don't actually reset.
        if (data->value != new_value)
        {
            // TODO: What is the reset logic here? Is there any?
            data->value = new_value;
        }
    }

    auto id = get_widget_id(ctx);
    auto route = get_active_routing_region(ctx);

    handle_event<refresh_event>(ctx, [=](auto ctx, auto& e) {
        if (signal_is_readable(value))
        {
            e.current_children->push_back(asmdom::h(
                "input",
                asmdom::Data(
                    asmdom::Attrs{{"type", "number"}},
                    asmdom::Props{
                        {"value", emscripten::val(read_signal(value))}},
                    asmdom::Callbacks{
                        {"oninput", [=](emscripten::val e) {
                             auto start = std::chrono::system_clock::now();

                             value_update_event update;
                             update.id = id;
                             update.value
                                 = e["target"]["value"].as<std::string>();
                             dispatch_targeted_event(the_system, update, route);
                             refresh();

                             auto end = std::chrono::system_clock::now();

                             std::chrono::duration<double> elapsed_seconds
                                 = end - start;
                             std::time_t end_time
                                 = std::chrono::system_clock::to_time_t(end);

                             std::cout
                                 << "finished computation at "
                                 << std::ctime(&end_time)
                                 << "elapsed time: " << elapsed_seconds.count()
                                 << "s\n";

                             return true;
                         }}})));
        }
    });
    handle_event<value_update_event>(ctx, [=](auto ctx, auto& e) {
        {
            if (e.id == id)
            {
                if (signal_is_writable(value))
                    write_signal(value, e.value);
                data->value = e.value;
            }
        }
    });
}

void
do_button(context ctx, readable<std::string> text, action<> on_click)
{
    auto id = get_widget_id(ctx);
    auto route = get_active_routing_region(ctx);
    handle_event<refresh_event>(ctx, [=](auto ctx, auto& e) {
        if (signal_is_readable(text))
        {
            e.current_children->push_back(asmdom::h(
                "button",
                asmdom::Data(
                    asmdom::Attrs{{"class", "btn btn-primary mr-1"}},
                    asmdom::Callbacks{
                        {"onclick",
                         [=](emscripten::val) {
                             auto start = std::chrono::system_clock::now();

                             click_event click;
                             click.id = id;
                             dispatch_targeted_event(the_system, click, route);
                             refresh();

                             auto end = std::chrono::system_clock::now();

                             std::chrono::duration<double> elapsed_seconds
                                 = end - start;
                             std::time_t end_time
                                 = std::chrono::system_clock::to_time_t(end);

                             std::cout
                                 << "finished computation at "
                                 << std::ctime(&end_time)
                                 << "elapsed time: " << elapsed_seconds.count()
                                 << "s\n";

                             return true;
                         }}}),
                read_signal(text)));
        }
    });
    handle_event<click_event>(ctx, [=](auto ctx, auto& e) {
        {
            if (e.id == id && action_is_ready(on_click))
            {
                perform_action(on_click);
            }
        }
    });
}

void
refresh()
{
    asmdom::Children children;

    refresh_event event;
    event.current_children = &children;

    dispatch_event(the_system, event);

    asmdom::VNode* root = asmdom::h(
        "div", asmdom::Data(asmdom::Attrs{{"class", "container"}}), children);

    asmdom::patch(current_view, root);
    current_view = root;
}

//////

void
refresh();

std::vector<std::string> actions;

void
do_ui(context ctx)
{
    auto n = get_state(ctx, 1_a);
    do_text(ctx, apply(ctx, ALIA_LAMBDIFY(std::to_string), n));
    do_button(ctx, "increase"_a, ++n);
    do_button(ctx, "decrease"_a, --n);

    auto x = get_state(ctx, "7"_a);
    do_text(ctx, x);
    do_number_input(ctx, x);
    // ctx, fake_writability(apply(ctx, ALIA_LAMBDIFY(std::to_string), n)));
    do_button(ctx, "reset"_a, x <<= "4"_a);
}

// void
// refresh()
// {
//     // {
//     //     div top_level(ctx, {{"class", "container"}});

//     //     h4(ctx, std::to_string(i));

//     //     {
//     //         p button_row(ctx);
//     //         button(
//     //             ctx,
//     //             {{"class", "btn btn-primary mr-1"}},
//     //             {{"onclick", i <<= i - 1}},
//     //             "decrease");
//     //         button(
//     //             ctx,
//     //             {{"class", "btn btn-secondary"}},
//     //             {{"onclick", i <<= i - 1}},
//     //             "increase");
//     //     }

//     //     p(ctx, [&](auto ctx) {
//     //         button(
//     //             ctx,
//     //             {{"class", "btn btn-primary mr-1"}},
//     //             {{"onclick", i <<= i - 1}},
//     //             "decrease");
//     //         button(
//     //             ctx,
//     //             {{"class", "btn btn-secondary"}},
//     //             {{"onclick", i <<= i - 1}},
//     //             "increase");
//     //     });

//     //     p(ctx, [&](auto ctx) {
//     //         button(
//     //             ctx,
//     //             {_class = "btn btn-primary mr-1"},
//     //             {_onclick = i <<= i - 1},
//     //             "decrease");
//     //         button(
//     //             ctx,
//     //             {_class = "btn btn-secondary"},
//     //             {_onclick = i <<= i - 1},
//     //             "increase");
//     //     });

//     //     do_action_list(ctx);
//     // }

//     // asmdom::Children action_list;
//     // for (int i = std::max(0, int(actions.size()) - 10); i <
//     actions.size();
//     // ++i)
//     // {
//     //     action_list.push_back(asmdom::h(
//     //         "li",
//     //         asmdom::Data(asmdom::Attrs{{"class",
//     "list-group-item"}}),
//     //         actions[i]));
//     // }

//     // asmdom::VNode* new_node = asmdom::h(
//     //     "div",
//     //     asmdom::Data(asmdom::Attrs{{"class", "container"}}),
//     //     asmdom::Children{
//     //         asmdom::h("h4", std::string(std::to_string(i))),
//     //         asmdom::h(
//     //             "p",
//     //             asmdom::Children{
//     //                 asmdom::h(
//     //                     "button",
//     //                     asmdom::Data(
//     //                         asmdom::Attrs{{"class", "btn
//     btn-primary mr-1"}},
//     //                         asmdom::Callbacks{{"onclick",
//     decrease}}),
//     //                     std::string("decrease")),
//     //                 asmdom::h(
//     //                     "button",
//     //                     asmdom::Data(
//     //                         asmdom::Attrs{{"class", "btn
//     btn-secondary"}},
//     //                         asmdom::Callbacks{{"onclick",
//     increase}}),
//     //                     std::string("increase"))}),
//     //         asmdom::h(
//     //             "ul",
//     //             asmdom::Data(asmdom::Attrs{{"class",
//     "list-group"}}),
//     //             action_list)});

//     // asmdom::patch(current_view, new_node);
//     // current_view = new_node;
// }

static void
download_succeeded(emscripten_fetch_t* fetch)
{
    actions.push_back("download succeeded");
    actions.push_back(std::string(fetch->data, fetch->numBytes));
    emscripten_fetch_close(fetch); // Free data associated with the fetch.
    refresh();
}

static void
download_failed(emscripten_fetch_t* fetch)
{
    actions.push_back("download failed");
    emscripten_fetch_close(fetch); // Also free data on failure.
    refresh();
}

int
main()
{
    the_system.controller = do_ui;

    // Initialize asm-dom.
    asmdom::Config config = asmdom::Config();
    asmdom::init(config);

    // Replace <div id="root"/> with our virtual dom.
    emscripten::val document = emscripten::val::global("document");
    emscripten::val root
        = document.call<emscripten::val>("getElementById", std::string("root"));
    current_view = asmdom::h("div", std::string("Initial view"));
    asmdom::patch(root, current_view);

    // Fetch some data.
    emscripten_fetch_attr_t attr;
    emscripten_fetch_attr_init(&attr);
    strcpy(attr.requestMethod, "GET");
    attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
    attr.onsuccess = download_succeeded;
    attr.onerror = download_failed;
    emscripten_fetch(&attr, "https://reqres.in/api/users/2");
    actions.push_back("download started");

    // Update the virtual dom.
    refresh();

    return 0;
};
