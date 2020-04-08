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

alia::system the_system;

dom::system the_dom;

//////

void
do_addition_ui(dom::context ctx, duplex<double> a, duplex<double> b)
{
    do_input(ctx, a);
    do_input(ctx, b);
    do_text(ctx, a + b);
}

void
do_greeting_ui(dom::context ctx, duplex<string> name)
{
    // Allow the user to input their name.
    do_input(ctx, name);

    // Greet the user.
    alia_if(name != "")
    {
        do_text(ctx, "Hello, " + name + "!");
    }
    alia_end
}

void
do_ui(dom::context ctx)
{
    // std::cout << "ticks: " << get_millisecond_counter() << std::endl;

    // dom::context ctx
    //     = add_component<context_info_tag>(vanilla_ctx,
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

    auto color = get_state(ctx, value(rgb8(0, 0, 0)));

    do_button(ctx, "black"_a, color <<= value(rgb8(50, 50, 55)));
    do_button(ctx, "white"_a, color <<= value(rgb8(230, 230, 255)));

    do_colored_box(ctx, smooth(ctx, color));

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
do_tip_calculator(dom::context ctx)
{
    // Get the state we need.
    auto bill = get_state(ctx, empty<double>()); // defaults to uninitialized
    auto tip_percentage = get_state(ctx, value(20.)); // defaults to 20%

    // if (signal_is_readable(bill))
    // {
    //     std::cout << "bill: " << read_signal(bill) << std::endl;
    // }
    // else
    // {
    //     std::cout << "bill unreadable" << std::endl;
    // }
    // if (signal_is_readable(tip_percentage))
    // {
    //     std::cout << "tip: " << read_signal(tip_percentage) << std::endl;
    // }
    // else
    // {
    //     std::cout << "tip unreadable" << std::endl;
    // }

    // Show some controls for manipulating our state.
    // std::cout << "do bill" << std::endl;
    do_input(ctx, bill);
    // std::cout << "do tip" << std::endl;
    do_input(ctx, tip_percentage);

    // Do some reactive calculations.
    auto tip = bill * tip_percentage / 100;
    auto total = bill + tip;

    // Show the results.
    do_text(ctx, printf(ctx, "tip: %.2f", tip));
    do_text(ctx, printf(ctx, "total: %.2f", total));

    // Allow the user to split the bill.
    auto n_people = get_state(ctx, value(1.));
    do_input(ctx, n_people);
    alia_if(n_people > value(1))
    {
        do_text(ctx, printf(ctx, "tip per person: %.2f", tip / n_people));
        do_text(ctx, printf(ctx, "total per person: %.2f", total / n_people));
    }
    alia_end
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

int
main()
{
    initialize(the_dom, the_system, "root", do_ui);

    // // Fetch some data.
    // emscripten_fetch_attr_t attr;
    // emscripten_fetch_attr_init(&attr);
    // strcpy(attr.requestMethod, "GET");
    // attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
    // attr.onsuccess = download_succeeded;
    // attr.onerror = download_failed;
    // emscripten_fetch(&attr, "https://reqres.in/api/users/2");
    // actions.push_back("download started");

    return 0;
};
