// Copyright 2019 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

#include "asm-dom.hpp"

#include <emscripten/emscripten.h>
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

#include "color.hpp"
#include "dom.hpp"

using std::string;

using namespace alia;
using namespace alia::literals;

using namespace dom;

//////

void
do_checkbox_(dom::context ctx, duplex<bool> value, readable<std::string> label)
{
    bool determinate = value.has_value();
    bool checked = determinate && value.read();
    bool disabled = !value.ready_to_write();

    element(ctx, "div")
        .attr("class", "custom-control custom-checkbox")
        .children([&](auto ctx) {
            element(ctx, "input")
                .attr("type", "checkbox")
                .attr("class", "custom-control-input")
                .attr("disabled", disabled)
                .attr("id", "custom-check-1")
                .prop("indeterminate", !determinate)
                .prop("checked", checked)
                .callback("onchange", [&](emscripten::val e) {
                    write_signal(value, e["target"]["checked"].as<bool>());
                });
            element(ctx, "label")
                .attr("class", "custom-control-label")
                .attr("for", "custom-check-1")
                .text(label);
        });
}

template<class Label>
void
do_checkbox(dom::context ctx, duplex<bool> value, Label label)
{
    do_checkbox_(ctx, value, signalize(label));
}

void
do_link_(dom::context ctx, readable<std::string> text, action<> on_click)
{
    element(ctx, "li").children([&](auto ctx) {
        element(ctx, "a")
            .attr("href", "javascript: void(0);")
            .attr("disabled", on_click.is_ready() ? "false" : "true")
            .children([&](auto ctx) { text_node(ctx, text); })
            .callback(
                "onclick", [&](emscripten::val) { perform_action(on_click); });
    });
}

template<class Text>
void
do_link(dom::context ctx, Text text, action<> on_click)
{
    do_link_(ctx, signalize(text), on_click);
}

void
do_ui(dom::context ctx)
{
    // std::cout << "ticks: " << get_millisecond_counter() << std::endl;

    // dom::context ctx
    //     = add_component<tree_traversal_tag>(vanilla_ctx,
    //     &the_context_info);

    // do_addition_ui(
    //     ctx, get_state(ctx, empty<double>()), get_state(ctx,
    //     empty<double>()));

    // auto x = get_state(ctx, value(4));
    // do_text(
    //     ctx, apply(ctx, ALIA_LAMBDIFY(std::to_string), smooth(ctx,
    //     x)));
    // do_input(ctx, x);
    // do_button(ctx, "reset"_a, x <<= value(4));

    ///

    ///

    // auto length = apply(ctx, alia_method(length), name);
    // alia_if(length > 10)
    // {
    //     do_text(ctx, "That's a long name!"_a);
    // }
    // alia_end

    // do_text(
    //     ctx,
    //     apply(
    //         ctx, ALIA_LAMBDIFY(std::to_string),
    //         get_animation_tick_count(ctx)));

    ////

    // auto color = get_state(ctx, value(rgb8(0, 0, 0)));

    // {
    //     scoped_div buttons(ctx, value("button-container"));
    //     do_button(ctx, "black"_a, color <<= value(rgb8(50, 50, 55)));
    //     do_button(ctx, "white"_a, color <<= value(rgb8(230, 230, 255)));
    // }

    // do_colored_box(ctx, smooth(ctx, color));

    // auto write_mask = get_state(ctx, true);
    // do_checkbox(ctx, write_mask, "Mask Writes");

    // auto state = get_state(ctx, empty<bool>());
    // do_button(ctx, "Initialize!", state <<= false);
    // do_checkbox(ctx, mask_writes(state, write_mask), "Boo!");
    // do_colored_box(
    //     ctx,
    //     smooth(
    //         ctx,
    //         conditional(state, value(rgb8(50, 50, 55)), rgb8(230, 230,
    //         255))));

    ////

    // animation_timer timer(ctx);
    // ALIA_IF(timer.is_active())
    // {
    //     do_text(
    //         ctx, apply(ctx, ALIA_LAMBDIFY(std::to_string),
    //         timer.ticks_left()));
    // }
    // ALIA_ELSE
    // {
    //     do_button(ctx, "start"_a,
    //     timer.start(value(millisecond_count(1000))));
    // }
    // ALIA_END

    // auto color = lift(ctx, interpolate)(
    //     value(rgb8(230, 230, 255)),
    //     value(rgb8(40, 40, 160)),
    //     lazy_lift(ALIA_LAMBDIFY(abs))(lazy_lift(ALIA_LAMBDIFY(sin))(
    //         get_animation_tick_count(ctx) / value(1000.))));
    // do_colored_box(ctx, color);

    // do_text(
    //     ctx,
    //     apply(
    //         ctx,
    //         ALIA_LAMBDIFY(std::to_string),
    //         value(get_millisecond_counter())));

    // do_button(ctx, "red"_a, color <<= value(rgb8(128, 64, 64)));

    // auto n = get_state(ctx, value(7));
    // do_text(ctx, apply(ctx, ALIA_LAMBDIFY(std::to_string), n));
    // do_button(ctx, "increase"_a, ++n);
    // do_button(ctx, "decrease"_a, --n);

    ///

    // auto x = get_state(ctx, value(4));
    // do_text(
    //     ctx, apply(ctx, ALIA_LAMBDIFY(std::to_string), smooth(ctx,
    //     x)));
    // do_input(ctx, x);
    // do_button(ctx, "reset"_a, x <<= value(4));
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

// static void
// download_succeeded(emscripten_fetch_t* fetch)
// {
//     actions.push_back("download succeeded");
//     actions.push_back(std::string(fetch->data, fetch->numBytes));
//     emscripten_fetch_close(fetch); // Free data associated with the fetch.
//     refresh();
// }

// static void
// download_failed(emscripten_fetch_t* fetch)
// {
//     actions.push_back("download failed");
//     emscripten_fetch_close(fetch); // Also free data on failure.
//     refresh();
// }

void
do_nav_ui(dom::context ctx)
{
    auto on_click
        = lambda_action([]() { std::cout << "It worked!!" << std::endl; });

    do_link(ctx, "Just Testing", on_click);
}

void
do_heading_(dom::context ctx, char const* level, readable<std::string> text)
{
    element(ctx, level).text(text);
}

template<class Text>
void
do_heading(dom::context ctx, char const* level, Text text)
{
    do_heading_(ctx, level, as_text(ctx, signalize(text)));
}

void
do_content_ui(dom::context ctx)
{
    auto state = get_state(ctx, false);
    // raw_timer timer(ctx);
    // if (timer.is_triggered())
    //     state.write(!read_signal(state));
    // if (!timer.is_active())
    //     timer.start(5000);
    // if (timer.is_triggered())
    //     abort_traversal(ctx);

    element(ctx, "h4")
        .attr("class", "header-title")
        .text(conditional(state, "On!", "Off!"));

    do_checkbox(ctx, state, "Abacadaba");

    for (int i = 0; i != 10; ++i)
        element(ctx, "h4").attr("class", "header-title").text("Fun!");
    // schedule_animation_refresh(ctx);
}

int
main()
{
    // static alia::system nav_sys;
    // static dom::system nav_dom;
    // initialize(nav_dom, nav_sys, "nav-root", do_nav_ui);

    static alia::system content_sys;
    static dom::system content_dom;
    initialize(content_dom, content_sys, "content-root", do_content_ui);

    return 0;
};
