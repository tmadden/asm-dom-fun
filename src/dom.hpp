#ifndef DOM_HPP
#define DOM_HPP

#include "alia.hpp"
#include "asm-dom.hpp"
#include "color.hpp"

using std::string;

namespace dom {

using namespace alia;

struct dom_context_info
{
    asmdom::Children* current_children;
};
ALIA_DEFINE_COMPONENT_TYPE(dom_context_info_tag, dom_context_info*)

typedef alia::add_component_type_t<alia::context, dom_context_info_tag>
    dom_context;

struct click_event : targeted_event
{
};

struct value_update_event : targeted_event
{
    string value;
};

void
do_text_(dom_context ctx, readable<std::string> text);

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
do_input_(dom_context ctx, bidirectional<string> value);

template<class Signal>
void
do_input(dom_context ctx, Signal signal)
{
    do_input_(ctx, as_bidirectional_text(ctx, signal));
}

void
do_number_input_(dom_context ctx, bidirectional<string> value);

template<class Signal>
void
do_number_input(dom_context ctx, Signal value)
{
    do_number_input_(ctx, as_bidirectional_text(ctx, value));
}

void
do_button(dom_context ctx, readable<std::string> text, action<> on_click);

void
do_colored_box(dom_context ctx, readable<rgb8> color);

struct dom_external_interface : alia::external_interface
{
    alia::system* system;

    void
    request_animation_refresh();
};

struct dom_system
{
    std::function<void(dom_context)> controller;

    asmdom::VNode* current_view = nullptr;

    dom_external_interface external;

    void
    operator()(alia::context ctx);
};

} // namespace dom

#endif
