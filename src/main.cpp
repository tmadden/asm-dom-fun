// Copyright 2019 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

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

void
do_nav_ui(dom::context ctx)
{
    auto on_click
        = lambda_action([]() { std::cout << "It worked!!" << std::endl; });

    do_link(ctx, "Just Testing", on_click);
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

    auto color = get_state(ctx, value(rgb8(0, 0, 0)));
    {
        scoped_div buttons(ctx, value("button-container"));
        do_button(ctx, "black"_a, color <<= value(rgb8(50, 50, 55)));
        do_button(ctx, "white"_a, color <<= value(rgb8(230, 230, 255)));
    }
    do_colored_box(ctx, smooth(ctx, color));

    auto n = get_state(ctx, empty<int>());
    element(ctx, "div").attr("class", "form-group").children([&](auto ctx) {
        do_input(ctx, n);
    });

    do_cached_content(ctx, n.value_id(), [&](auto ctx) {
        std::cout << "Refreshing cached content..." << std::endl;

        ALIA_IF(n > 12)
        {
            element(ctx, "h4").attr("class", "header-title").text("HIGH!");
        }
        ALIA_END

        element(ctx, "h4")
            .attr("class", "header-title")
            .text(conditional(n > 2, "On!", "Off!"));
    });

    element(ctx, "h4")
        .attr("class", "header-title")
        .text(conditional(state, "On!", "Off!"));

    auto storage_value = get_state(ctx, "");
    element(ctx, "div").attr("class", "form-group").children([&](auto ctx) {
        do_checkbox(ctx, state, "Abacadaba");

        element(ctx, "div")
            .attr("class", "input-group")
            .children([&](auto ctx) {
                do_input(ctx, storage_value);

                element(ctx, "div")
                    .attr("class", "input-group-append")
                    .children([&](auto ctx) {
                        do_button(ctx, "Store!", lambda_action([&] {
                                      emscripten::val::global("localStorage")
                                          .call<void>(
                                              "setItem",
                                              emscripten::val("my_key"),
                                              emscripten::val(
                                                  read_signal(storage_value)));
                                  }));
                        do_button(
                            ctx, "Retrieve!", lambda_action([&] {
                                write_signal(
                                    storage_value,
                                    emscripten::val::global("localStorage")
                                        .call<std::string>(
                                            "getItem",
                                            emscripten::val("my_key")));
                            }));
                    });
            });
    });

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
