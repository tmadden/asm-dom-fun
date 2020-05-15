#include "dom.hpp"

#include <emscripten/emscripten.h>
#include <emscripten/val.h>

#include <chrono>

namespace dom {

struct text_node_data
{
    tree_node<element_object> node;
    captured_id value_id;
};

void
text_node_(dom::context ctx, readable<string> text)
{
    text_node_data* data;
    if (get_cached_data(ctx, &data))
        data->node.object.create_as_text_node("");
    if (is_refresh_event(ctx))
    {
        refresh_tree_node(get<tree_traversal_tag>(ctx), data->node);
        refresh_signal_shadow(
            data->value_id,
            text,
            [&](std::string const& new_value) {
                EM_ASM_(
                    { Module.setNodeValue($0, Module['UTF8ToString']($1)); },
                    data->node.object.js_id,
                    new_value.c_str());
            },
            [&]() {
                EM_ASM_(
                    { Module.setNodeValue($0, ""); }, data->node.object.js_id);
            });
    }
}

// struct input_data
// {
//     component_identity identity;
//     captured_id external_id;
//     string value;
//     bool invalid = false;
//     element_data element;
//     unsigned version = 0;
// };

// template<class Value>
// struct value_update_event : targeted_event
// {
//     Value value;
// };

// void
// do_input_(dom::context ctx, duplex<string> value)
// {
//     input_data* data;
//     get_cached_data(ctx, &data);

//     on_refresh(ctx, [&](auto ctx) {
//         refresh_component_identity(ctx, data->identity);

//         refresh_signal_shadow(
//             data->external_id,
//             value,
//             [&](string new_value) {
//                 data->value = std::move(new_value);
//                 data->invalid = false;
//                 ++data->version;
//             },
//             [&]() {
//                 data->value.clear();
//                 data->invalid = false;
//                 ++data->version;
//             });
//     });

//     auto* system = &ctx.get<system_tag>();
//     auto external_id = externalize(&data->identity);

//     add_element(ctx, data->element, make_id(data->version), [&]() {
//         asmdom::Attrs attrs;
//         if (data->invalid)
//             attrs["class"] = "invalid-input";
//         return asmdom::h(
//             "input",
//             asmdom::Data(
//                 attrs,
//                 asmdom::Props{{"value", emscripten::val(data->value)}},
//                 asmdom::Callbacks{
//                     {"oninput", [=](emscripten::val e) {
//                          value_update_event<std::string> update;
//                          update.value =
//                          e["target"]["value"].as<std::string>();
//                          dispatch_targeted_event(*system, update,
//                          external_id); return true;
//                      }}}));
//     });
//     on_targeted_event<value_update_event<std::string>>(
//         ctx, &data->identity, [=](auto ctx, auto& e) {
//             if (signal_ready_to_write(value))
//             {
//                 try
//                 {
//                     write_signal(value, e.value);
//                 }
//                 catch (validation_error&)
//                 {
//                     data->invalid = true;
//                 }
//             }
//             data->value = e.value;
//             ++data->version;
//         });
// }

// void
// do_button_(dom::context ctx, readable<std::string> text, action<> on_click)
// {
//     auto id = get_component_id(ctx);
//     auto external_id = externalize(id);
//     auto* system = &ctx.get<system_tag>();

//     element_data* element;
//     get_cached_data(ctx, &element);

//     if (signal_has_value(text))
//     {
//         add_element(
//             ctx,
//             *element,
//             combine_ids(ref(text.value_id()), make_id(on_click.is_ready())),
//             [=]() {
//                 return asmdom::h(
//                     "button",
//                     asmdom::Data(
//                         asmdom::Attrs{{"class", "btn btn-secondary"},
//                                       {"disabled",
//                                        on_click.is_ready() ? "false" :
//                                        "true"}},
//                         asmdom::Callbacks{{"onclick",
//                                            [=](emscripten::val) {
//                                                click_event click;
//                                                dispatch_targeted_event(
//                                                    *system, click,
//                                                    external_id);
//                                                return true;
//                                            }}}),
//                     read_signal(text));
//             });
//     }

//     on_targeted_event<click_event>(
//         ctx, id, [=](auto ctx, auto& e) { perform_action(on_click); });
// }

// void
// do_link_(dom::context ctx, readable<std::string> text, action<> on_click)
// {
//     auto id = get_component_id(ctx);
//     auto external_id = externalize(id);
//     auto* system = &ctx.get<system_tag>();

//     element_data* element;
//     get_cached_data(ctx, &element);

//     if (signal_has_value(text))
//     {
//         add_element(
//             ctx,
//             *element,
//             combine_ids(ref(text.value_id()), make_id(on_click.is_ready())),
//             [=]() {
//                 return asmdom::h(
//                     "li",
//                     asmdom::Children({asmdom::h(
//                         "a",
//                         asmdom::Data(
//                             asmdom::Attrs{
//                                 {"href", "javascript: void(0);"},
//                                 {"disabled",
//                                  on_click.is_ready() ? "false" : "true"}},
//                             asmdom::Callbacks{{"onclick",
//                                                [=](emscripten::val) {
//                                                    click_event click;
//                                                    dispatch_targeted_event(
//                                                        *system,
//                                                        click,
//                                                        external_id);
//                                                    return true;
//                                                }}}),
//                         read_signal(text))}));
//             });
//     }

//     on_targeted_event<click_event>(
//         ctx, id, [=](auto ctx, auto& e) { perform_action(on_click); });
// }

// template<class Text>
// void
// do_link(dom::context ctx, Text text, action<> on_click)
// {
//     do_link_(ctx, signalize(text), on_click);
// }

// void
// do_colored_box(dom::context ctx, readable<rgb8> color)
// {
//     add_element(
//         ctx, get_cached_data<element_data>(ctx), color.value_id(), [=]() {
//             char style[64] = {'\0'};
//             if (signal_has_value(color))
//             {
//                 rgb8 const& c = read_signal(color);
//                 snprintf(
//                     style,
//                     64,
//                     "background-color: #%02x%02x%02x",
//                     c.r,
//                     c.g,
//                     c.b);
//             }
//             return asmdom::h(
//                 "div",
//                 asmdom::Data(
//                     asmdom::Attrs{{"class", "colored-box"}, {"style",
//                     style}}));
//         });
// }

// void
// do_hr(dom::context ctx)
// {
//     add_element(ctx, get_cached_data<element_data>(ctx), unit_id, [=]() {
//         return asmdom::h("hr");
//     });
// }

// struct div_data
// {
//     element_data element;
//     keyed_data<std::string> class_name;
//     asmdom::Children children_;
// };

// void
// scoped_div::begin(dom::context ctx, readable<std::string> class_name)
// {
//     ctx_.reset(ctx);

//     get_cached_data(ctx, &data_);

//     on_refresh(ctx, [&](auto ctx) {
//         auto& dom_context = get<tree_traversal_tag>(ctx);
//         parent_children_list_ = dom_context.current_children;
//         dom_context.current_children = &data_->children_;
//         data_->children_.clear();

//         refresh_keyed_data(data_->class_name, class_name.value_id());
//         if (!is_valid(data_->class_name) && class_name.has_value())
//             set(data_->class_name, class_name.read());
//     });

//     container_.begin(ctx);
// }

// void
// scoped_div::end()
// {
//     if (ctx_)
//     {
//         dom::context ctx = *ctx_;

//         container_.end();

//         auto& dom_context = ctx.get<tree_traversal_tag>();
//         dom_context.current_children = parent_children_list_;

//         if (is_refresh_event(ctx))
//         {
//             asmdom::Attrs attrs;
//             if (is_valid(data_->class_name))
//                 attrs["class"] = get(data_->class_name);
//             parent_children_list_->push_back(
//                 asmdom::h("div", asmdom::Data(attrs), data_->children_));
//         }

//         ctx_.reset();
//     }
// }

static void
refresh_for_emscripten(void* system)
{
    refresh_system(*reinterpret_cast<alia::system*>(system));
}

struct timer_callback_data
{
    alia::system* system;
    external_component_id component;
    millisecond_count trigger_time;
};

static void
timer_callback(void* user_data)
{
    std::unique_ptr<timer_callback_data> data(
        reinterpret_cast<timer_callback_data*>(user_data));
    timer_event event;
    event.trigger_time = data->trigger_time;
    dispatch_targeted_event(*data->system, event, data->component);
}

struct dom_external_interface : default_external_interface
{
    dom_external_interface(alia::system& owner)
        : default_external_interface(owner)
    {
    }

    void
    schedule_animation_refresh()
    {
        emscripten_async_call(refresh_for_emscripten, &this->owner, -1);
    }

    void
    schedule_timer_event(
        external_component_id component, millisecond_count time)
    {
        auto timeout_data
            = new timer_callback_data{&this->owner, component, time};
        emscripten_async_call(
            timer_callback, timeout_data, time - this->get_tick_count());
    }
};

void
system::operator()(alia::context vanilla_ctx)
{
    tree_traversal<element_object> traversal;
    auto ctx = vanilla_ctx.add<tree_traversal_tag>(traversal);

    if (is_refresh_event(ctx))
    {
        std::chrono::steady_clock::time_point begin
            = std::chrono::steady_clock::now();

        traverse_object_tree(
            traversal, this->root_node, [&]() { this->controller(ctx); });

        std::chrono::steady_clock::time_point end
            = std::chrono::steady_clock::now();
        std::cout << "refresh time: "
                  << std::chrono::duration_cast<std::chrono::microseconds>(
                         end - begin)
                         .count()
                  << "[µs]" << std::endl;
    }
    else
    {
        this->controller(ctx);
    }

    if (is_refresh_event(ctx))
    {
    }
    else
    {
        this->controller(ctx);
    }
}

void
initialize(
    dom::system& dom_system,
    alia::system& alia_system,
    std::string const& dom_node_id,
    std::function<void(dom::context)> controller)
{
    // Initialize asm - dom(once).
    static bool asmdom_initialized = false;
    if (!asmdom_initialized)
    {
        asmdom::Config config = asmdom::Config();
        config.unsafePatch = true;
        config.clearMemory = true;
        asmdom::init(config);
        asmdom_initialized = true;
    }

    // Initialize the alia::system and hook it up to the dom::system.
    initialize_system(
        alia_system,
        std::ref(dom_system),
        new dom_external_interface(alia_system));
    dom_system.controller = std::move(controller);

    // Replace the requested node in the DOM with our virtual DOM.
    emscripten::val document = emscripten::val::global("document");
    emscripten::val placeholder
        = document.call<emscripten::val>("getElementById", dom_node_id);
    // For now, just create a div to hold all our content.
    emscripten::val root = document.call<emscripten::val>(
        "createElement", emscripten::val("div"));
    placeholder["parentNode"].call<emscripten::val>(
        "replaceChild", root, placeholder);
    dom_system.root_node.object.js_id
        = emscripten::val::global("window")["asmDomHelpers"]["domApi"]
              .call<int>("addNode", root);

    // Update the virtual DOM.
    refresh_system(alia_system);
}

} // namespace dom
