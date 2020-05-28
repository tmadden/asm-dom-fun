#ifndef DOM_HPP
#define DOM_HPP

#include "alia.hpp"
#include "asm-dom.hpp"
#include "color.hpp"

#include <functional>

#include <emscripten/emscripten.h>
#include <emscripten/val.h>

using std::string;

namespace dom {

using namespace alia;

struct element_object;

ALIA_DEFINE_TAGGED_TYPE(tree_traversal_tag, tree_traversal<element_object>&)

typedef alia::extend_context_type_t<alia::context, tree_traversal_tag> context;

struct dom_event : targeted_event
{
    dom_event(emscripten::val event) : event(event)
    {
    }
    emscripten::val event;
};

struct callback_data
{
    component_identity identity;
    std::function<void(emscripten::val)> callback;
};

struct element_object
{
    void
    create_as_element(char const* type)
    {
        assert(this->js_id == 0);
        this->js_id = EM_ASM_INT(
            { return Module.createElement(Module['UTF8ToString']($0)); }, type);
    }

    void
    create_as_text_node(char const* value)
    {
        assert(this->js_id == 0);
        this->js_id = EM_ASM_INT(
            { return Module.createTextNode(Module['UTF8ToString']($0)); },
            value);
    }

    void
    relocate(element_object& parent, element_object* after)
    {
        assert(this->js_id != 0);
        EM_ASM_(
            { Module.insertAfter($0, $1, $2); },
            parent.js_id,
            this->js_id,
            after ? after->js_id : 0);
    }

    void
    remove()
    {
        assert(this->js_id != 0);
        EM_ASM_({ Module.removeChild($0); }, this->js_id);
    }

    ~element_object()
    {
        if (this->js_id != 0)
        {
            EM_ASM_({ Module.releaseNode($0); }, this->js_id);
        }
    }

    int js_id = 0;
    element_object* next = nullptr;
    element_object* children = nullptr;
};

void
text_node_(dom::context ctx, readable<string> text);

template<class Text>
void
text_node(dom::context ctx, Text text)
{
    text_node_(ctx, signalize(text));
}

void
do_element_attribute(
    context ctx,
    element_object& object,
    char const* name,
    readable<string> value);

void
do_element_attribute(
    context ctx,
    element_object& object,
    char const* name,
    readable<bool> value);

template<class Context>
struct element_handle : noncopyable
{
    element_handle(Context ctx, char const* type) : ctx_(ctx)
    {
        initializing_ = get_cached_data(ctx, &node_);
        if (initializing_)
            node_->object.create_as_element(type);
        if (is_refresh_event(ctx))
            refresh_tree_node(get<tree_traversal_tag>(ctx), *node_);
    }

    template<class Value>
    element_handle&
    attr(char const* name, Value value)
    {
        do_element_attribute(ctx_, node_->object, name, signalize(value));
        return *this;
    }

    template<class Value>
    element_handle&
    prop(char const* name, Value value)
    {
        auto& stored_id = get_cached_data<captured_id>(ctx_);
        on_refresh(ctx_, [&](auto ctx) {
            auto value_ = signalize(value);
            refresh_signal_shadow(
                stored_id,
                value_,
                [&](Value const& new_value) {
                    emscripten::val::global(
                        "window")["asmDomHelpers"]["nodes"][node_->object.js_id]
                        .set(name, emscripten::val(new_value));
                    // Add the property name to the element's 'asmDomRaws' list.
                    // asm-dom uses this to track what it needs to clean up when
                    // recycling a DOM node.
                    EM_ASM_(
                        {
                            const element = Module.nodes[$0];
                            if (!element.hasOwnProperty('asmDomRaws'))
                                element['asmDomRaws'] = [];
                            element['asmDomRaws'].push(
                                Module['UTF8ToString']($1));
                        },
                        node_->object.js_id,
                        name);
                },
                [&]() {
                    EM_ASM_(
                        {
                            Module.nodes[$0][$1] = emscripten::val::undefined();
                            // Remove the property name from the element's
                            // 'asmDomRaws' list. (See above.)
                            const asmDomRaws = Module.nodes[$0]['asmDomRaws'];
                            const index = asmDomRaws.indexOf(
                                Module['UTF8ToString']($1));
                            if (index > -1)
                            {
                                asmDomRaws.splice(index, 1);
                            }
                        },
                        node_->object.js_id,
                        name);
                });
        });
        return *this;
    }

    template<class Function>
    element_handle&
    children(Function&& fn)
    {
        auto& traversal = get<tree_traversal_tag>(ctx_);
        scoped_tree_children<element_object> tree_scope;
        if (is_refresh_event(ctx_))
            tree_scope.begin(traversal, *node_);
        fn(ctx_);
        return *this;
    }

    template<class Function>
    element_handle&
    callback(char const* event_type, Function&& fn)
    {
        auto& data = get_cached_data<callback_data>(ctx_);
        if (initializing_)
            install_element_callback(ctx_, node_->object, data, event_type);
        on_targeted_event<dom_event>(
            ctx_, &data.identity, [&](auto ctx, auto& e) { fn(e.event); });
        return *this;
    }

    template<class Text>
    element_handle&
    text(Text text)
    {
        return children([&](auto ctx) { text_node(ctx, text); });
    }

 private:
    Context ctx_;
    tree_node<element_object>* node_;
    bool initializing_;
};

template<class Context>
element_handle<Context>
element(Context ctx, char const* type)
{
    return element_handle<Context>(ctx, type);
}

void
do_input_(dom::context ctx, duplex<string> value);

template<class Signal>
void
do_input(dom::context ctx, Signal signal)
{
    do_input_(ctx, as_duplex_text(ctx, signal));
}

void
do_checkbox(dom::context ctx, duplex<bool> value);

void
do_button_(dom::context ctx, readable<std::string> text, action<> on_click);

template<class Text>
void
do_button(dom::context ctx, Text text, action<> on_click)
{
    do_button_(ctx, signalize(text), on_click);
}

void
do_checkbox_(dom::context ctx, duplex<bool> value, readable<std::string> label);

template<class Label>
void
do_checkbox(dom::context ctx, duplex<bool> value, Label label)
{
    do_checkbox_(ctx, value, signalize(label));
}

void
do_link_(dom::context ctx, readable<std::string> text, action<> on_click);

template<class Text>
void
do_link(dom::context ctx, Text text, action<> on_click)
{
    do_link_(ctx, signalize(text), on_click);
}

// void
// do_colored_box(dom::context ctx, readable<rgb8> color);

// struct div_data;

// struct scoped_div : noncopyable
// {
//     scoped_div(dom::context ctx, readable<std::string> class_name)
//     {
//         begin(ctx, class_name);
//     }

//     ~scoped_div()
//     {
//         end();
//     }

//     void
//     begin(dom::context ctx, readable<std::string> class_name);

//     void
//     end();

//  private:
//     optional_context<dom::context> ctx_;
//     div_data* data_ = nullptr;
//     asmdom::Children* parent_children_list_ = nullptr;
//     scoped_component_container container_;
// };

// template<class DoContents>
// void
// do_div(
//     dom::context ctx, readable<std::string> class_name, DoContents
//     do_contents)
// {
//     scoped_div div(ctx, class_name);
//     do_contents(ctx);
// }

struct system
{
    std::function<void(dom::context)> controller;

    tree_node<element_object> root_node;

    alia::system alia_system;

    void
    operator()(alia::context ctx);
};

void
initialize(
    dom::system& dom_system,
    alia::system& alia_system,
    std::string const& dom_node_id,
    std::function<void(dom::context)> controller);

} // namespace dom

#endif
