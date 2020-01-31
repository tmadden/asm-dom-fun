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
#include "timing.hpp"

using std::string;

using namespace alia;
using namespace alia::literals;

////

millisecond_count the_millisecond_tick_count;

millisecond_count
get_millisecond_tick_count()
{
    static auto start = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<
               std::chrono::duration<millisecond_count, std::milli>>(
               now - start)
        .count();
}

////

struct timing_interface
{
    virtual void
    request_timing_event()
        = 0;
};

/////

// When an accessor is set to a value, it's allowed to throw a validation
// error if the value is not acceptable.
// It should include a message that's presentable to the user.
struct validation_error : exception
{
    validation_error(string const& message) : exception(message)
    {
    }
    ~validation_error() throw()
    {
    }
};

//

// TEXT CONVERSION

// All conversion of values to and from text goes through the functions
// from_string and to_string. In order to use a particular value type with
// the text-based widgets and utilities provided here, that type must
// implement these functions.

#define ALIA_DECLARE_STRING_CONVERSIONS(T)                                     \
    void from_string(T* value, string const& s);                               \
    string to_string(T value);

// from_string(value, s) should parse the string s and store it in *value.
// It should throw a validation_error if the string doesn't parse.

// to_string(value) should simply return the string form of value.

// Implementations of from_string and to_string are provided for the following
// types.

ALIA_DECLARE_STRING_CONVERSIONS(short int)
ALIA_DECLARE_STRING_CONVERSIONS(unsigned short int)
ALIA_DECLARE_STRING_CONVERSIONS(int)
ALIA_DECLARE_STRING_CONVERSIONS(unsigned int)
ALIA_DECLARE_STRING_CONVERSIONS(long int)
ALIA_DECLARE_STRING_CONVERSIONS(unsigned long int)
ALIA_DECLARE_STRING_CONVERSIONS(long long int)
ALIA_DECLARE_STRING_CONVERSIONS(unsigned long long int)
ALIA_DECLARE_STRING_CONVERSIONS(float)
ALIA_DECLARE_STRING_CONVERSIONS(double)

ALIA_DECLARE_STRING_CONVERSIONS(std::string)

// as_text(ctx, x) creates a text-based interface to the accessor x.
template<class Readable>
void
update_text_conversion(keyed_data<string>* data, Readable x)
{
    if (signal_is_readable(x))
    {
        refresh_keyed_data(*data, x.value_id());
        if (!is_valid(*data))
            set(*data, to_string(read_signal(x)));
    }
    else
    {
        invalidate(*data);
    }
}
template<class Readable>
keyed_data_signal<string>
as_text(context ctx, Readable x)
{
    keyed_data<string>* data;
    get_cached_data(ctx, &data);
    update_text_conversion(data, x);
    return keyed_data_signal<string>(data);
}

template<class Value>
struct bidirectional_text_data
{
    captured_id input_id;
    Value input_value;
    bool output_valid = false;
    std::string output_text;
    counter_type output_version = 1;
};

template<class Value, class Readable>
void
update_bidirectional_text(bidirectional_text_data<Value>* data, Readable x)
{
    if (signal_is_readable(x))
    {
        auto const& input_id = x.value_id();
        if (!data->input_id.matches(input_id))
        {
            if (!data->output_valid || read_signal(x) != data->input_value)
            {
                data->output_text = to_string(read_signal(x));
                data->output_valid = true;
                ++data->output_version;
            }
            data->input_id.capture(input_id);
        }
    }
    else
    {
        data->output_valid = false;
    }
}

// as_bidirectional_text(ctx, x) is similar to as_text but it's bidirectional.
template<class Wrapped>
struct bidirectional_text_signal : signal<
                                       bidirectional_text_signal<Wrapped>,
                                       string,
                                       typename Wrapped::direction_tag>
{
    bidirectional_text_signal(
        Wrapped wrapped,
        bidirectional_text_data<typename Wrapped::value_type>* data)
        : wrapped_(wrapped), data_(data)
    {
    }
    bool
    is_readable() const
    {
        return data_->output_valid;
    }
    string const&
    read() const
    {
        return data_->output_text;
    }
    id_interface const&
    value_id() const
    {
        id_ = make_id(data_->output_version);
        return id_;
    }
    bool
    is_writable() const
    {
        return wrapped_.is_writable();
    }
    void
    write(string const& s) const
    {
        data_->output_text = s;
        ++data_->output_version;
        typename Wrapped::value_type value;
        from_string(&value, s);
        data_->input_value = value;
        wrapped_.write(value);
    }

 private:
    mutable simple_id<counter_type> id_;
    bidirectional_text_data<typename Wrapped::value_type>* data_;
    Wrapped wrapped_;
};
template<class Signal>
bidirectional_text_signal<Signal>
as_bidirectional_text(context ctx, Signal x)
{
    bidirectional_text_data<typename Signal::value_type>* data;
    get_cached_data(ctx, &data);
    update_bidirectional_text(data, x);
    return bidirectional_text_signal<Signal>(x, data);
}

//

template<class T>
bool
string_to_value(string const& str, T* value)
{
    std::istringstream s(str);
    T x;
    if (!(s >> x))
        return false;
    s >> std::ws;
    if (s.eof())
    {
        *value = x;
        return true;
    }
    return false;
}

template<class T>
string
value_to_string(T const& value)
{
    std::ostringstream s;
    s << value;
    return s.str();
}

template<class T>
void
float_from_string(T* value, string const& str)
{
    if (!string_to_value(str, value))
        throw validation_error("This input expects a number.");
}

#define ALIA_FLOAT_CONVERSIONS(T)                                              \
    void from_string(T* value, string const& str)                              \
    {                                                                          \
        float_from_string(value, str);                                         \
    }                                                                          \
    string to_string(T value)                                                  \
    {                                                                          \
        return value_to_string(value);                                         \
    }

ALIA_FLOAT_CONVERSIONS(float)
ALIA_FLOAT_CONVERSIONS(double)

template<class T>
void
integer_from_string(T* value, string const& str)
{
    long long n;
    if (!string_to_value(str, &n))
        throw validation_error("This input expects an integer.");
    T x = T(n);
    if (x != n)
        throw validation_error("This integer is outside the supported range.");
    *value = x;
}

#define ALIA_INTEGER_CONVERSIONS(T)                                            \
    void from_string(T* value, string const& str)                              \
    {                                                                          \
        integer_from_string(value, str);                                       \
    }                                                                          \
    string to_string(T value)                                                  \
    {                                                                          \
        return value_to_string(value);                                         \
    }

ALIA_INTEGER_CONVERSIONS(short int)
ALIA_INTEGER_CONVERSIONS(unsigned short int)
ALIA_INTEGER_CONVERSIONS(int)
ALIA_INTEGER_CONVERSIONS(unsigned int)
ALIA_INTEGER_CONVERSIONS(long int)
ALIA_INTEGER_CONVERSIONS(unsigned long int)
ALIA_INTEGER_CONVERSIONS(long long int)
ALIA_INTEGER_CONVERSIONS(unsigned long long int)

void
from_string(string* value, string const& str)
{
    *value = str;
}
string
to_string(string value)
{
    return std::move(value);
}

///////

alia::system the_system;

asmdom::VNode* current_view = nullptr;

struct dom_context_info
{
    asmdom::Children* current_children;
};
ALIA_DEFINE_COMPONENT_TYPE(dom_context_info_tag, dom_context_info*)

dom_context_info the_context_info;

typedef alia::add_component_type_t<alia::context, dom_context_info_tag>
    dom_context;

struct click_event
{
    node_id id;
};

struct value_update_event
{
    node_id id;
    string value;
};

void
refresh();

void
do_text_(dom_context ctx, readable<std::string> text)
{
    handle_event<refresh_event>(ctx, [text](auto ctx, auto& e) {
        get_component<dom_context_info_tag>(ctx)->current_children->push_back(
            asmdom::h(
                "p", signal_is_readable(text) ? read_signal(text) : string()));
    });
}

template<class Signal>
void
do_text(dom_context ctx, Signal text)
{
    do_text_(ctx, as_text(ctx, text));
}

struct input_data
{
    captured_id external_id;
    string value;
};

void
do_number_input_(dom_context ctx, bidirectional<string> value)
{
    input_data* data;
    get_cached_data(ctx, &data);

    if (signal_is_readable(value))
    {
        if (!data->external_id.matches(value.value_id()))
        {
            data->value = read_signal(value);
            data->external_id.capture(value.value_id());
        }
    }
    else
    {
        if (!data->external_id.matches(no_id))
        {
            data->value = string();
            data->external_id.capture(no_id);
        }
    }

    auto id = get_node_id(ctx);
    auto route = get_active_routing_region(ctx);

    handle_event<refresh_event>(ctx, [=](auto ctx, auto& e) {
        get_component<dom_context_info_tag>(ctx)->current_children->push_back(
            asmdom::h(
                "input",
                asmdom::Data(
                    asmdom::Attrs{{"type", "number"}},
                    asmdom::Props{{"value", emscripten::val(data->value)}},
                    asmdom::Callbacks{
                        {"oninput", [=](emscripten::val e) {
                             auto start = std::chrono::system_clock::now();

                             value_update_event update;
                             update.id = id;
                             update.value
                                 = e["target"]["value"].as<std::string>();
                             dispatch_targeted_event(the_system, update, route);
                             refresh();

                             //  std::cout << update.value << std::endl;

                             auto end = std::chrono::system_clock::now();

                             std::chrono::duration<double> elapsed_seconds
                                 = end - start;
                             //  std::cout
                             //      << "elapsed time: " <<
                             //      elapsed_seconds.count()
                             //      << "s\n";

                             return true;
                         }}})));
    });
    handle_event<value_update_event>(ctx, [=](auto ctx, auto& e) {
        {
            // std::cout << "UPDATE!: " << e.value << std::endl;
            if (e.id == id)
            {
                // std::cout << "ID matched" << std::endl;
                if (signal_is_writable(value))
                {
                    // std::cout << "signal writable" << std::endl;
                    write_signal(value, e.value);
                }
                data->value = e.value;
            }
        }
    });
}

void
do_input_(dom_context ctx, bidirectional<string> value)
{
    input_data* data;
    get_cached_data(ctx, &data);

    if (signal_is_readable(value))
    {
        if (!data->external_id.matches(value.value_id()))
        {
            data->value = read_signal(value);
            data->external_id.capture(value.value_id());
        }
    }
    else
    {
        if (!data->external_id.matches(no_id))
        {
            data->value = string();
            data->external_id.capture(no_id);
        }
    }

    auto id = get_node_id(ctx);
    auto route = get_active_routing_region(ctx);

    handle_event<refresh_event>(ctx, [=](auto ctx, auto& e) {
        get_component<dom_context_info_tag>(ctx)->current_children->push_back(
            asmdom::h(
                "input",
                asmdom::Data(
                    asmdom::Props{{"value", emscripten::val(data->value)}},
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

                             return true;
                         }}})));
    });
    handle_event<value_update_event>(ctx, [=](auto ctx, auto& e) {
        {
            if (e.id == id)
            {
                if (signal_is_writable(value))
                {
                    write_signal(value, e.value);
                }
                data->value = e.value;
            }
        }
    });
}

template<class Signal>
void
do_input(dom_context ctx, Signal signal)
{
    do_input_(ctx, as_bidirectional_text(ctx, signal));
}

template<class Signal>
void
do_number_input(dom_context ctx, Signal value)
{
    do_number_input_(ctx, as_bidirectional_text(ctx, value));
}

void
do_button(dom_context ctx, readable<std::string> text, action<> on_click)
{
    auto id = get_node_id(ctx);
    auto route = get_active_routing_region(ctx);
    handle_event<refresh_event>(ctx, [=](auto ctx, auto& e) {
        if (signal_is_readable(text))
        {
            get_component<dom_context_info_tag>(ctx)
                ->current_children->push_back(asmdom::h(
                    "button",
                    asmdom::Data(
                        asmdom::Attrs{{"class", "btn btn-primary mr-1"}},
                        asmdom::Callbacks{
                            {"onclick",
                             [=](emscripten::val) {
                                 auto start = std::chrono::system_clock::now();

                                 click_event click;
                                 click.id = id;
                                 dispatch_targeted_event(
                                     the_system, click, route);
                                 refresh();

                                 auto end = std::chrono::system_clock::now();

                                 std::chrono::duration<double> elapsed_seconds
                                     = end - start;
                                 //  std::cout << "elapsed time: "
                                 //            << elapsed_seconds.count() <<
                                 //            "s\n";

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
    the_millisecond_tick_count = get_millisecond_tick_count();

    asmdom::Children children;
    the_context_info.current_children = &children;

    refresh_event event;
    dispatch_event(the_system, event);

    // std::cout << children.size() << std::endl;

    asmdom::VNode* root = asmdom::h(
        "div", asmdom::Data(asmdom::Attrs{{"class", "container"}}), children);

    asmdom::patch(current_view, root);
    current_view = root;
}

//////

std::vector<std::string> actions;

void
do_colored_box(dom_context ctx, readable<rgb8> color)
{
    handle_event<refresh_event>(ctx, [color](auto ctx, auto& e) {
        char style[64] = {'\0'};
        if (signal_is_readable(color))
        {
            rgb8 const& c = read_signal(color);
            sprintf(style, "background-color: #%02x%02x%02x", c.r, c.g, c.b);
        }
        std::cout << style << std::endl;
        get_component<dom_context_info_tag>(ctx)->current_children->push_back(
            asmdom::h(
                "div",
                asmdom::Data(asmdom::Attrs{{"class", "colored-box"},
                                           {"style", style}})));
    });
}

void
do_addition_ui(
    dom_context ctx, bidirectional<double> a, bidirectional<double> b)
{
    do_input(ctx, a);
    do_input(ctx, b);
    do_text(ctx, a + b);
}

void
do_greeting_ui(dom_context ctx, bidirectional<string> name)
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
do_ui(context vanilla_ctx)
{
    // std::cout << "ticks: " << get_millisecond_counter() << std::endl;

    dom_context ctx
        = add_component<dom_context_info_tag>(vanilla_ctx, &the_context_info);

    do_addition_ui(
        ctx, get_state(ctx, empty<double>()), get_state(ctx, empty<double>()));

    // auto x = get_state(ctx, val(4));
    // do_text(
    //     ctx, apply(ctx, ALIA_LAMBDIFY(std::to_string), smooth_value(ctx,
    //     x)));
    // do_number_input(ctx, x);
    // do_button(ctx, "reset"_a, x <<= val(4));

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

    // auto color = get_state(ctx, val(rgb8(0, 0, 0)));

    // do_button(ctx, "black"_a, color <<= val(rgb8(50, 50, 55)));
    // do_button(ctx, "white"_a, color <<= val(rgb8(230, 230, 255)));

    // do_colored_box(ctx, smooth_value(ctx, color));

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
    //     timer.start(val(millisecond_count(1000))));
    // }
    // ALIA_END

    // auto color = lift(ctx, interpolate)(
    //     val(rgb8(230, 230, 255)),
    //     val(rgb8(40, 40, 160)),
    //     lazy_lift(ALIA_LAMBDIFY(abs))(lazy_lift(ALIA_LAMBDIFY(sin))(
    //         get_animation_tick_count(ctx) / val(1000.))));
    // do_colored_box(ctx, color);

    // do_text(
    //     ctx,
    //     apply(
    //         ctx,
    //         ALIA_LAMBDIFY(std::to_string),
    //         val(get_millisecond_counter())));

    // do_button(ctx, "red"_a, color <<= val(rgb8(128, 64, 64)));

    // auto n = get_state(ctx, val(7));
    // do_text(ctx, apply(ctx, ALIA_LAMBDIFY(std::to_string), n));
    // do_button(ctx, "increase"_a, ++n);
    // do_button(ctx, "decrease"_a, --n);

    ///

    // auto x = get_state(ctx, val(4));
    // do_text(
    //     ctx, apply(ctx, ALIA_LAMBDIFY(std::to_string), smooth_value(ctx,
    //     x)));
    // do_number_input(ctx, x);
    // do_button(ctx, "reset"_a, x <<= val(4));
}

void
do_tip_calculator(dom_context ctx)
{
    // Get the state we need.
    auto bill = get_state(ctx, empty<double>()); // defaults to uninitialized
    auto tip_percentage = get_state(ctx, val(20.)); // defaults to 20%

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
    do_number_input(ctx, bill);
    // std::cout << "do tip" << std::endl;
    do_number_input(ctx, tip_percentage);

    // Do some reactive calculations.
    auto tip = bill * tip_percentage / 100;
    auto total = bill + tip;

    // Show the results.
    do_text(ctx, printf(ctx, "tip: %.2f", tip));
    do_text(ctx, printf(ctx, "total: %.2f", total));

    // Allow the user to split the bill.
    auto n_people = get_state(ctx, val(1.));
    do_number_input(ctx, n_people);
    alia_if(n_people > val(1))
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

    emscripten_set_main_loop(refresh, 0, 0);

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
