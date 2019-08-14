// Copyright 2019 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

#include "asm-dom.hpp"
#include <emscripten/val.h>
#include <functional>
#include <string>

void
render();

asmdom::VNode* current_view = nullptr;

int i = 1;
std::vector<std::string> actions;

bool decrease(emscripten::val)
{
    i--;
    actions.push_back("decrease");
    render();
    return true;
}

bool increase(emscripten::val)
{
    i++;
    actions.push_back("increase");
    render();
    return true;
}

void
render()
{
    asmdom::Children action_list;
    for (int i = std::max(0, int(actions.size()) - 10); i < actions.size(); ++i)
    {
        action_list.push_back(asmdom::h(
            u8"li",
            asmdom::Data(asmdom::Attrs{{u8"class", u8"list-group-item"}}),
            actions[i]));
    }

    asmdom::VNode* new_node = asmdom::h(
        u8"div",
        asmdom::Data(asmdom::Attrs{{u8"class", u8"container"}}),
        asmdom::Children{
            asmdom::h(u8"h4", std::string(std::to_string(i))),
            asmdom::h(
                u8"p",
                asmdom::Children{
                    asmdom::h(
                        u8"button",
                        asmdom::Data(
                            asmdom::Attrs{
                                {u8"class", u8"btn btn-primary mr-1"}},
                            asmdom::Callbacks{{u8"onclick", decrease}}),
                        std::string(u8"decrease")),
                    asmdom::h(
                        u8"button",
                        asmdom::Data(
                            asmdom::Attrs{{u8"class", u8"btn btn-secondary"}},
                            asmdom::Callbacks{{u8"onclick", increase}}),
                        std::string(u8"increase"))}),
            asmdom::h(
                u8"ul",
                asmdom::Data(asmdom::Attrs{{u8"class", u8"list-group"}}),
                action_list)});

    asmdom::patch(current_view, new_node);
    current_view = new_node;
}

int
main()
{
    // Initialize asm-dom.
    asmdom::Config config = asmdom::Config();
    asmdom::init(config);

    // Replace <div id="root"/> with our virtual dom.
    emscripten::val document = emscripten::val::global("document");
    emscripten::val root
        = document.call<emscripten::val>("getElementById", std::string("root"));
    current_view = asmdom::h(u8"div", std::string(u8"Initial view"));
    asmdom::patch(root, current_view);

    // Update the virtual dom.
    render();

    return 0;
};
