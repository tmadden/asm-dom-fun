#ifndef DOM_HPP
#define DOM_HPP

#include "alia.hpp"
#include "asm-dom.hpp"
#include "color.hpp"

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
    dom_event(emscripten::val val) : val(val)
    {
    }
    emscripten::val val;
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

struct element : noncopyable
{
    element(dom::context ctx, char const* type) : ctx_(ctx)
    {
        if (get_cached_data(ctx, &node_))
            node_->object.create_as_element(type);
        if (is_refresh_event(ctx))
            refresh_tree_node(get<tree_traversal_tag>(ctx), *node_);
    }

    element&
    attr_(char const* name, readable<string> value)
    {
        refresh_signal_shadow(
            get_cached_data<captured_id>(ctx_),
            value,
            [&](string const& new_value) {
                std::cout << "attr: " << name << " to " << new_value << "\n";
                EM_ASM_(
                    {
                        Module.setAttribute(
                            $0,
                            Module['UTF8ToString']($1),
                            Module['UTF8ToString']($2));
                    },
                    node_->object.js_id,
                    name,
                    new_value.c_str());
            },
            [&]() {
                std::cout << "attr: " << name << " to indeterminate\n";
                EM_ASM_(
                    { Module.removeAttribute($0, Module['UTF8ToString']($1)); },
                    node_->object.js_id,
                    name);
            });
        return *this;
    }
    element&
    attr_(char const* name, readable<bool> value)
    {
        refresh_signal_shadow(
            get_cached_data<captured_id>(ctx_),
            value,
            [&](bool new_value) {
                std::cout << "attr: " << name << " to " << new_value << "\n";
                if (new_value)
                {
                    EM_ASM_(
                        {
                            Module.setAttribute(
                                $0,
                                Module['UTF8ToString']($1),
                                Module['UTF8ToString']($2));
                        },
                        node_->object.js_id,
                        name,
                        "");
                }
                else
                {
                    EM_ASM_(
                        {
                            Module.removeAttribute(
                                $0, Module['UTF8ToString']($1));
                        },
                        node_->object.js_id,
                        name);
                }
            },
            [&]() { std::cout << "attr: " << name << " to indeterminate\n"; });
        return *this;
    }
    template<class Value>
    element&
    attr(char const* name, Value value)
    {
        return attr_(name, signalize(value));
    }

    template<class Value>
    element&
    prop(char const* name, Value value)
    {
        auto value_ = signalize(value);
        // refresh_signal_shadow(
        //     get_cached_data<captured_id>(ctx_),
        //     value_,
        //     [&](Value const& new_value) {
        //         // EM_ASM_(
        //         //     { Module.nodes[$0][$1] = $2; },
        //         //     node_->object.js_id,
        //         //     name,
        //         //     new_value);
        //         std::cout << "prop: " << name << " to " << new_value << "\n";
        //         std::cout << emscripten::val::global(
        //                          "window")["asmDomHelpers"]["nodes"]
        //                                   [node_->object.js_id]["id"]
        //                                       .as<std::string>()
        //                   << "\n";
        //         // TODO: Need to deal with asmDomRaws.
        //         emscripten::val::global(
        //             "window")["asmDomHelpers"]["nodes"][node_->object.js_id]
        //             .set(name, emscripten::val(new_value));
        //     },
        //     [&]() {
        //         EM_ASM_(
        //             { Module.nodes[$0][$1] = emscripten::val::undefined(); },
        //             node_->object.js_id,
        //             name);
        //     });
        return *this;
    }

    template<class Function>
    element&
    children(Function fn)
    {
        auto& traversal = get<tree_traversal_tag>(ctx_);
        scoped_tree_children<element_object> tree_scope;
        if (is_refresh_event(ctx_))
            tree_scope.begin(traversal, *node_);
        fn(ctx_);
        return *this;
    }

    template<class Function>
    void
    callback(char const* event, Function&& fn)
    {
        // auto id = get_component_id(ctx_);
        // auto external_id = externalize(id);
        // auto* system = &get<system_tag>(ctx_);
        // callbacks_["onclick"] = [=](emscripten::val v) {
        //     dom_event event{std::move(v)};
        //     dispatch_targeted_event(*system, event, external_id);
        //     return true;
        // };
        // on_targeted_event<dom_event>(
        //     ctx_, id, [&](auto ctx, auto& e) { fn(e.val); });
    }

    template<class Text>
    void
    text(Text text)
    {
        children([&](auto ctx) { text_node(ctx, text); });
    }

 private:
    dom::context ctx_;
    tree_node<element_object>* node_;
};

// void
// do_text_(dom::context ctx, readable<std::string> text);

// template<class Signal>
// void
// do_text(dom::context ctx, Signal text)
// {
//     do_text_(ctx, as_text(ctx, signalize(text)));
// }

// void
// do_heading_(
//     dom::context ctx, readable<std::string> level, readable<std::string>
//     text);

// template<class Level, class Text>
// void
// do_heading(dom::context ctx, Level level, Text text)
// {
//     do_heading_(ctx, signalize(level), as_text(ctx, signalize(text)));
// }

// void
// do_input_(dom::context ctx, duplex<string> value);

// template<class Signal>
// void
// do_input(dom::context ctx, Signal signal)
// {
//     do_input_(ctx, as_duplex_text(ctx, signal));
// }

// void
// do_checkbox(dom::context ctx, duplex<bool> value);

// void
// do_button_(dom::context ctx, readable<std::string> text, action<>
// on_click);

// template<class Text>
// void
// do_button(dom::context ctx, Text text, action<> on_click)
// {
//     do_button_(ctx, signalize(text), on_click);
// }

// void
// do_link_(dom::context ctx, readable<std::string> text, action<>
// on_click);

// template<class Text>
// void
// do_link(dom::context ctx, Text text, action<> on_click)
// {
//     do_link_(ctx, signalize(text), on_click);
// }

// void
// do_colored_box(dom::context ctx, readable<rgb8> color);

// void
// do_hr(dom::context ctx);

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
