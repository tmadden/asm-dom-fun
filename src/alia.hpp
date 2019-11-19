// MIT License
//
// Copyright (c) 2012 - present  .decimal, LLC & Partners HealthCare
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef ALIA_CORE_HPP
#define ALIA_CORE_HPP

#include <cassert>
#include <cstdint>
#include <exception>
#include <iostream>
#include <memory>
#include <ostream>
#include <string>
#include <type_traits>

// This file defines some generic functionality that's commonly used throughout
// alia.

namespace alia {

typedef long long counter_type;

// Inspired by Boost, inheriting from noncopyable disables copying for a type.
// The namespace prevents unintended ADL if used by applications.
namespace impl {
namespace noncopyable_ {
struct noncopyable
{
    noncopyable()
    {
    }

 private:
    noncopyable(noncopyable const& other) = delete;
    noncopyable&
    operator=(noncopyable const& other)
        = delete;
};
} // namespace noncopyable_
} // namespace impl
typedef impl::noncopyable_::noncopyable noncopyable;

// general-purpose exception class for alia
struct exception : std::exception
{
    exception(std::string const& msg) : msg_(new std::string(msg))
    {
    }

    ~exception() throw()
    {
    }

    virtual char const*
    what() const throw()
    {
        return msg_->c_str();
    }

    // Add another level of context to the error messsage.
    void
    add_context(std::string const& str)
    {
        *msg_ += "\n" + str;
    }

 private:
    std::shared_ptr<std::string> msg_;
};

// ALIA_UNUSED is used to denote that a variable may be unused.
#ifdef __GNUC__
#define ALIA_UNUSED [[gnu::unused]]
#else
#define ALIA_UNUSED
#endif

// implementation of C++17's void_t that works on C++11 compilers
template<typename... Ts>
struct make_void
{
    typedef void type;
};
template<typename... Ts>
using void_t = typename make_void<Ts...>::type;

// ALIA_LAMBDIFY(f) produces a lambda that calls f, which is essentially a
// version of f that can be passed as an argument and still allows normal
// overload resolution.
#define ALIA_LAMBDIFY(f) [](auto&&... args) { return f(args...); }
#ifdef ALIA_LOWERCASE_MACROS
#define alia_lambdify(f) ALIA_LAMBDIFY(f)
#endif

} // namespace alia



#include <type_traits>
#include <typeindex>
#include <unordered_map>

// This file provides a means for defining arbitrary collections of components
// that constitute the context in which an application operates. The set of
// components required for an application obviously varies across applications
// (and even across modules within a complex application). The goal here is to
// allow applications to freely mix together components from multiple sources
// (which may not know about one another).
//
// Some additional design considerations follow. Note that these refer to
// contexts rather than component collections, since that is the primary
// motivating example of a component collection.
//
// 1. An application should be able to easily define its own components and mix
// those into its context. This includes application-level state. If there is
// state that is essentially global to the application (e.g., the active user),
// application code should be able to retrieve this from the application
// context. Similarly, a component of the application should be able to extend
// the application's context with state that is specific to that component
// (but ubiquitous within it).
//
// 2. Functions that take contexts as arguments should be able to define the set
// of components that they require as part of the type signature of the context.
// Any caller whose context includes a superset of those components should be
// able to call the function with an implicit conversion of the context
// parameter. This should all be possible without needing to define functions as
// templates (otherwise alia-based applications would end up being entirely
// header-based) and with minimal (ideally zero) runtime overhead in converting
// the caller's context to the type expected by the function.
//
// 3. Retrieving frames/capabilities from a context should require minimal
// (ideally zero) runtime overhead.
//
// 4. Static type checking of context conversions/retrievals should be
// possible but optional. Developers should not have to pay these compile-time
// costs unless it's desired. It should be possible to use a mixed workflow
// where these checks are replaced by runtime checks for iterative development
// but enabled for CI/release builds.
//
// In order to satisfy #4, this file looks for a #define called
// ALIA_DYNAMIC_COMPONENT_CHECKING. If this is set, code related to statically
// checking components is omitted and dynamic checks are substituted where
// appropriate. Note that when ALIA_DYNAMIC_COMPONENT_CHECKING is NOT set,
// ALIA_STATIC_COMPONENT_CHECKING is set and static checks are included.
//
// The statically typed component_collection object is a simple wrapper around
// the dynamically typed storage object. It adds a compile-time type list
// indicating what's actually supposed to be in the collection. This allows
// collections to be converted to other collection types without any run-time
// overhead. This does imply some run-time overhead for retrieving components
// from the collection, but that can be mitigated by providing zero-cost
// retrieval for select (core) components. This also implies that the collection
// object must be passed by value (or const& - though there's no real point in
// that) whereas passing by reference would be more obvious, but that seems
// unavoidable given the requirements.

#ifndef ALIA_DYNAMIC_COMPONENT_CHECKING
#define ALIA_STATIC_COMPONENT_CHECKING
#endif

namespace alia {

template<class Components, class Storage>
struct component_collection;

#ifdef ALIA_STATIC_COMPONENT_CHECKING

namespace detail {

// A component has a tag type and a data type associated with it. The tag
// type is used only at compile time to identify the component while the data
// type actually stores any run-time data associated with the component.
template<class Tag, class Data>
struct component
{
    typedef Tag tag;
    typedef Data data_type;
};

// component_list<Components...> defines a simple compile-time list of
// components. This is held by a component_collection to provide compile-time
// tracking of its contents.
template<class... Components>
struct component_list
{
};

// component_list_contains_tag<Tag,Components>::value yields a compile-time
// boolean indicating whether or not :Components contains a component with a
// tag matching :Tag.
template<class List, class Tag>
struct component_list_contains_tag
{
};
// base case (list is empty, so :Tag not found)
template<class Tag>
struct component_list_contains_tag<component_list<>, Tag> : std::false_type
{
};
// case where tag matches
template<class Tag, class Data, class... Rest>
struct component_list_contains_tag<
    component_list<component<Tag, Data>, Rest...>,
    Tag> : std::true_type
{
};
// non-matching (recursive) case
template<class Tag, class OtherTag, class Data, class... Rest>
struct component_list_contains_tag<
    component_list<component<OtherTag, Data>, Rest...>,
    Tag> : component_list_contains_tag<component_list<Rest...>, Tag>
{
};

// component_collection_contains_tag<Collection,Tag>::value yields a
// compile-time boolean indicating whether or not :Collection contains an entry
// with a tag matching :Tag.
template<class Collection, class Tag>
struct component_collection_contains_tag
{
};
template<class Components, class Storage, class Tag>
struct component_collection_contains_tag<
    component_collection<Components, Storage>,
    Tag> : component_list_contains_tag<Components, Tag>
{
};

// list_contains_component<List,Component>::value yields a compile-time
// boolean indicating whether or not :Component appears in the component_list
// :List.
template<class List, class Component>
struct list_contains_component
{
};
// base case (list is empty, so :Component not found)
template<class Component>
struct list_contains_component<component_list<>, Component> : std::false_type
{
};
// case where component matches
template<class Component, class... Rest>
struct list_contains_component<component_list<Component, Rest...>, Component>
    : std::true_type
{
};
// non-matching (recursive) case
template<class Component, class OtherComponent, class... Rest>
struct list_contains_component<
    component_list<OtherComponent, Rest...>,
    Component> : list_contains_component<component_list<Rest...>, Component>
{
};

// collection_contains_component<Collection,Component>::value yields a
// compile-time boolean indicating whether or not :Collection contains
// :Component.
template<class Collection, class Component>
struct collection_contains_component
{
};
template<class Components, class Storage, class Component>
struct collection_contains_component<
    component_collection<Components, Storage>,
    Component> : list_contains_component<Components, Component>
{
};

// add_component_to_list<List,Component>::type yields the list that results
// from adding :Component to the head of :List.
// Note that this doesn't perform any checks for duplicate tags.
template<class List, class Component>
struct add_component_to_list
{
};
template<class Component, class... Components>
struct add_component_to_list<component_list<Components...>, Component>
{
    typedef component_list<Component, Components...> type;
};

// remove_component_from_list<List,Tag>::type yields the list that results from
// removing the component matching :Tag from :List.
// Note that removing a component that's not actually in the list is not
// considered an error.
template<class List, class Tag>
struct remove_component_from_list
{
};
// base case (list is empty)
template<class Tag>
struct remove_component_from_list<component_list<>, Tag>
{
    typedef component_list<> type;
};
// case where component matches
template<class Tag, class Data, class... Rest>
struct remove_component_from_list<
    component_list<component<Tag, Data>, Rest...>,
    Tag> : remove_component_from_list<component_list<Rest...>, Tag>
{
};
// non-matching case
template<class Tag, class OtherTag, class Data, class... Rest>
struct remove_component_from_list<
    component_list<component<OtherTag, Data>, Rest...>,
    Tag>
    : add_component_to_list<
          typename remove_component_from_list<component_list<Rest...>, Tag>::
              type,
          component<OtherTag, Data>>
{
};

// collection_contains_all_components<Collection,Components...>::value yields a
// compile-time boolean indicating whether or not :Collection contains all
// components in :Components.
template<class Collection, class... Components>
struct collection_contains_all_components
{
};
// base case (list is empty)
template<class Collection>
struct collection_contains_all_components<Collection> : std::true_type
{
};
// recursive case
template<class Collection, class Component, class... Rest>
struct collection_contains_all_components<Collection, Component, Rest...>
    : std::conditional_t<
          collection_contains_component<Collection, Component>::value,
          collection_contains_all_components<Collection, Rest...>,
          std::false_type>
{
};

// merge_component_lists<A,B>::type yields a list of components that includes
// all components from :A and :B (with no duplicates).
template<class A, class B>
struct merge_component_lists
{
};
// base case (:A is empty)
template<class B>
struct merge_component_lists<component_list<>, B>
{
    typedef B type;
};
// recursive case
template<class B, class AHead, class... ARest>
struct merge_component_lists<component_list<AHead, ARest...>, B>
    : add_component_to_list<
          // Ensure that :AHead isn't duplicated. (This may be a noop.)
          typename remove_component_from_list<
              typename merge_component_lists<component_list<ARest...>, B>::type,
              typename AHead::tag>::type,
          AHead>
{
};

// component_collection_is_convertible<From,To>::value yields a
// compile-time boolean indicating whether or not the type :From can be
// converted to the type :To (both must be component_collections).
// The requirements for this are that a) the storage types are the same and b)
// :From has a superset of the components of :To.
template<class From, class To>
struct component_collection_is_convertible
{
};
// case where storage types differ
template<
    class FromComponents,
    class FromStorage,
    class ToComponents,
    class ToStorage>
struct component_collection_is_convertible<
    component_collection<FromComponents, FromStorage>,
    component_collection<ToComponents, ToStorage>> : std::false_type
{
};
// case where storage types are the same, so components must be checked
template<class Storage, class FromComponents, class... ToComponents>
struct component_collection_is_convertible<
    component_collection<FromComponents, Storage>,
    component_collection<component_list<ToComponents...>, Storage>>
    : collection_contains_all_components<
          component_collection<FromComponents, Storage>,
          ToComponents...>
{
};

} // namespace detail

template<class Components, class Storage>
struct component_collection
{
    typedef Components components;
    typedef Storage storage_type;

    component_collection(Storage* storage) : storage(storage)
    {
    }

    // copy constructor (from convertible collections)
    template<class Other>
    component_collection(
        Other other,
        std::enable_if_t<detail::component_collection_is_convertible<
            Other,
            component_collection>::value>* = 0)
        : storage(other.storage)
    {
    }

    // assignment operator (from convertible collections)
    template<class Other>
    std::enable_if_t<
        detail::component_collection_is_convertible<
            Other,
            component_collection>::value,
        component_collection&>
    operator=(Other other)
    {
        storage = other.storage;
        return *this;
    }

    Storage* storage;
};

// empty_component_collection<Storage> yields a component collection with
// no components and :Storage as its storage type.
template<class Storage>
using empty_component_collection
    = component_collection<detail::component_list<>, Storage>;

// add_component_type<Collection,Tag,Data>::type gives the type that results
// from extending :Collection with the component defined by :Tag and :Data.
template<class Collection, class Tag, class Data>
struct add_component_type
{
};
template<class Tag, class Data, class Storage, class... Components>
struct add_component_type<
    component_collection<detail::component_list<Components...>, Storage>,
    Tag,
    Data>
{
    static_assert(
        !detail::component_list_contains_tag<
            detail::component_list<Components...>,
            Tag>::value,
        "duplicate component tag");
    typedef component_collection<
        detail::component_list<detail::component<Tag, Data>, Components...>,
        Storage>
        type;
};
template<class Collection, class Tag, class Data>
using add_component_type_t =
    typename add_component_type<Collection, Tag, Data>::type;

// remove_component_type<Collection,Tag,Data>::type yields the type that results
// from removing the component associated with :Tag from :Collection.
template<class Collection, class Tag>
struct remove_component_type
{
};
template<class Tag, class Storage, class Components>
struct remove_component_type<component_collection<Components, Storage>, Tag>
{
    static_assert(
        detail::component_list_contains_tag<Components, Tag>::value,
        "attempting to remove a component tag that doesn't exist");
    typedef component_collection<
        typename detail::remove_component_from_list<Components, Tag>::type,
        Storage>
        type;
};
template<class Collection, class Tag>
using remove_component_type_t =
    typename remove_component_type<Collection, Tag>::type;

// merge_components<A,B>::type yields a component collection type that contains
// all the components from :A and :B (but no duplicates).
// Note that the resulting component collection inherits the storage type of :A.
template<class A, class B>
struct merge_components
{
    typedef component_collection<
        typename detail::merge_component_lists<
            typename A::components,
            typename B::components>::type,
        typename A::storage_type>
        type;
};
template<class A, class B>
using merge_components_t = typename merge_components<A, B>::type;

#endif

#ifdef ALIA_DYNAMIC_COMPONENT_CHECKING

struct dynamic_component_list
{
};

template<class Components, class Storage>
struct component_collection
{
    typedef Components components;
    typedef Storage storage_type;

    component_collection(Storage* storage) : storage(storage)
    {
    }

    Storage* storage;
};

// empty_component_collection<Storage> yields a component collection with
// no components and :Storage as its storage type.
template<class Storage>
using empty_component_collection
    = component_collection<dynamic_component_list, Storage>;

// add_component_type<Collection,Tag,Data>::type gives the type that results
// from extending :Collection with the component defined by :Tag and :Data.
template<class Collection, class Tag, class Data>
struct add_component_type
{
    typedef Collection type;
};
template<class Collection, class Tag, class Data>
using add_component_type_t =
    typename add_component_type<Collection, Tag, Data>::type;

// remove_component_type<Collection,Tag,Data>::type yields the type that results
// from removing the component associated with :Tag from :Collection.
template<class Collection, class Tag>
struct remove_component_type
{
    typedef Collection type;
};
template<class Collection, class Tag>
using remove_component_type_t =
    typename remove_component_type<Collection, Tag>::type;

// merge_components<A,B>::type yields a component collection type that contains
// all the components from :A and :B (but no duplicates).
// Note that the resulting component collection inherits the storage type of :A.
template<class A, class B>
struct merge_components
{
    typedef A type;
};
template<class A, class B>
using merge_components_t = typename merge_components<A, B>::type;

#endif

// Extend a collection by adding a new component.
// :Tag is the tag of the component.
// :data is the data associated with the new component.
// :new_storage is a pointer to the storage object to use for the extended
// collection. It must outlive the returned collection. (Its contents before
// calling this function don't matter.)
template<class Tag, class Data, class Collection>
add_component_type_t<Collection, Tag, Data>
add_component(
    typename Collection::storage_type* new_storage,
    Collection collection,
    Data data)
{
    // Copy over existing components.
    *new_storage = *collection.storage;
    // Add the new data to the storage object.
    add_component<Tag>(*new_storage, data);
    // Create a collection to reference the new storage object.
    return add_component_type_t<Collection, Tag, Data>(new_storage);
}

// Remove a component from a collection.
// :Tag is the tag of the component.
// :new_storage is a pointer to the storage object to use for the new
// collection. It must outlive the returned collection. (Its contents before
// calling this function don't matter.)
// Note that this function is allowed to reuse the storage object from the
// input collection, so that is also required to outlive the returned
// collection.
template<class Tag, class Collection>
remove_component_type_t<Collection, Tag>
remove_component(
    typename Collection::storage_type* new_storage, Collection collection)
{
#ifdef ALIA_STATIC_COMPONENT_CHECKING
    // Since we're using static checking, it doesn't matter if the runtime
    // storage includes an extra component. Static checks will prevent its use.
    return remove_component_type_t<Collection, Tag>(collection.storage);
#else
    // Copy over existing components.
    *new_storage = *collection.storage;
    // Remove the component from the storage object.
    remove_component<Tag>(*new_storage);
    // Create a collection to reference the new storage object.
    return remove_component_type_t<Collection, Tag>(new_storage);
#endif
}

// Determine if a component is in a collection.
// :Tag identifies the component.
template<class Tag, class Collection>
bool
has_component(Collection collection)
{
#ifdef ALIA_STATIC_COMPONENT_CHECKING
    return detail::component_collection_contains_tag<Collection, Tag>::value;
#else
    return has_component<Tag>(*collection.storage);
#endif
}

// Get a reference to the data associated with a component in a collection.
// :Tag identifies the component.
// If static checking is enabled, this generates a compile-time error if :Tag
// isn't contained in :collection.
template<class Tag, class Collection>
auto
get_component(Collection collection)
{
#ifdef ALIA_STATIC_COMPONENT_CHECKING
    static_assert(
        detail::component_collection_contains_tag<Collection, Tag>::value,
        "component not found in collection");
#endif
    return get_component<Tag>(*collection.storage);
}

// generic_component_storage is one possible implementation of the underlying
// container for storing components and their associated data.
// :Data is the type used to store component data.
template<class Data>
struct generic_component_storage
{
    std::unordered_map<std::type_index, Data> components;
};

// The following functions constitute the interface expected of storage objects.

// Does the storage object have a component with the given tag?
template<class Tag, class Data>
bool
has_component(generic_component_storage<Data>& storage)
{
    return storage.components.find(std::type_index(typeid(Tag)))
           != storage.components.end();
}

// Add a component.
template<class Tag, class StorageData, class ComponentData>
void
add_component(
    generic_component_storage<StorageData>& storage, ComponentData&& data)
{
    storage.components[std::type_index(typeid(Tag))]
        = std::forward<ComponentData&&>(data);
}

// Remove a component.
template<class Tag, class Data>
void
remove_component(generic_component_storage<Data>& storage)
{
    storage.components.erase(std::type_index(typeid(Tag)));
}

// Get the data for a component.
template<class Tag, class Data>
Data&
get_component(generic_component_storage<Data>& storage)
{
    return storage.components.at(std::type_index(typeid(Tag)));
}

} // namespace alia



namespace alia {

struct data_traversal_tag
{
};
struct data_traversal;

struct event_traversal_tag
{
};
struct event_traversal;

struct any_pointer
{
    any_pointer()
    {
    }

    template<class T>
    any_pointer(T* ptr) : ptr(ptr)
    {
    }

    template<class T>
    operator T*()
    {
        return reinterpret_cast<T*>(ptr);
    }

    void* ptr;
};

template<class T>
bool
operator==(any_pointer p, T* other)
{
    return reinterpret_cast<T*>(p.ptr) == other;
}
template<class T>
bool
operator==(T* other, any_pointer p)
{
    return other == reinterpret_cast<T*>(p.ptr);
}
template<class T>
bool
operator!=(any_pointer p, T* other)
{
    return reinterpret_cast<T*>(p.ptr) != other;
}
template<class T>
bool
operator!=(T* other, any_pointer p)
{
    return other != reinterpret_cast<T*>(p.ptr);
}

// The structure we use to store components. It provides direct storage of the
// commonly-used components in the core of alia.
struct component_storage
{
    data_traversal* data = 0;
    event_traversal* event = 0;
    // generic storage for other components
    generic_component_storage<any_pointer> other;
};

// All component access is done through the following 'manipulator' structure.
// Specializations can be defined for tags that have direct storage.

template<class Tag>
struct component_manipulator
{
    static bool
    has(component_storage& storage)
    {
        return has_component<Tag>(storage.other);
    }
    static void
    add(component_storage& storage, any_pointer data)
    {
        add_component<Tag>(storage.other, data);
    }
    static void
    remove(component_storage& storage)
    {
        remove_component<Tag>(storage.other);
    }
    static any_pointer
    get(component_storage& storage)
    {
        return get_component<Tag>(storage.other);
    }
};

template<>
struct component_manipulator<data_traversal_tag>
{
    static bool
    has(component_storage& storage)
    {
        return storage.data != 0;
    }
    static void
    add(component_storage& storage, data_traversal* data)
    {
        storage.data = data;
    }
    static void
    remove(component_storage& storage)
    {
        storage.data = 0;
    }
    static data_traversal*
    get(component_storage& storage)
    {
#ifdef ALIA_DYNAMIC_COMPONENT_CHECKING
        if (!storage.data)
            throw "missing data traversal component";
#endif
        return storage.data;
    }
};

template<>
struct component_manipulator<event_traversal_tag>
{
    static bool
    has(component_storage& storage)
    {
        return storage.event != 0;
    }
    static void
    add(component_storage& storage, event_traversal* event)
    {
        storage.event = event;
    }
    static void
    remove(component_storage& storage)
    {
        storage.event = 0;
    }
    static event_traversal*
    get(component_storage& storage)
    {
#ifdef ALIA_DYNAMIC_COMPONENT_CHECKING
        if (!storage.event)
            throw "missing event traversal component";
#endif
        return storage.event;
    }
};

// The following is the implementation of the interface expected of component
// storage objects. It simply forwards the requests along to the appropriate
// manipulator.

template<class Tag>
bool
has_component(component_storage& storage)
{
    return component_manipulator<Tag>::has(storage);
}

template<class Tag, class Data>
void
add_component(component_storage& storage, Data&& data)
{
    component_manipulator<Tag>::add(storage, std::forward<Data&&>(data));
}

template<class Tag>
void
remove_component(component_storage& storage)
{
    component_manipulator<Tag>::remove(storage);
}

template<class Tag>
auto
get_component(component_storage& storage)
{
    return component_manipulator<Tag>::get(storage);
}

// Finally, the typedef for the context...

typedef add_component_type_t<
    empty_component_collection<component_storage>,
    event_traversal_tag,
    event_traversal*>
    dataless_context;

typedef add_component_type_t<
    dataless_context,
    data_traversal_tag,
    data_traversal*>
    context;

// And some convenience functions...

template<class Context>
data_traversal&
get_data_traversal(Context ctx)
{
    return *get_component<data_traversal_tag>(ctx);
}

bool
is_refresh_pass(context ctx);

template<class Context>
event_traversal&
get_event_traversal(Context ctx)
{
    return *get_component<event_traversal_tag>(ctx);
}

} // namespace alia


#include <functional>
#include <memory>
#include <sstream>

// This file implements the concept of IDs in alia.

namespace alia {

// id_interface defines the interface required of all ID types.
struct id_interface
{
 public:
    virtual ~id_interface()
    {
    }

    // Create a standalone copy of the ID.
    virtual id_interface*
    clone() const = 0;

    // Given another ID of the same type, set it equal to a standalone copy
    // of this ID.
    virtual void
    deep_copy(id_interface* copy) const = 0;

    // Given another ID of the same type, return true iff it's equal to this
    // one.
    virtual bool
    equals(id_interface const& other) const = 0;

    // Given another ID of the same type, return true iff it's less than this
    // one.
    virtual bool
    less_than(id_interface const& other) const = 0;
};

// The following convert the interface of the ID operations into the usual form
// that one would expect, as free functions.

inline bool
operator==(id_interface const& a, id_interface const& b)
{
    // Apparently it's faster to compare the name pointers for equality before
    // resorting to actually comparing the typeid objects themselves.
    return (typeid(a).name() == typeid(b).name() || typeid(a) == typeid(b))
           && a.equals(b);
}

inline bool
operator!=(id_interface const& a, id_interface const& b)
{
    return !(a == b);
}

bool
operator<(id_interface const& a, id_interface const& b);

// The following allow the use of IDs as keys in a map.
// The IDs are stored separately as captured_ids in the mapped values and
// pointers are used as keys. This allows searches to be done on pointers to
// other IDs.

struct id_interface_pointer_less_than_test
{
    bool
    operator()(id_interface const* a, id_interface const* b) const
    {
        return *a < *b;
    }
};

// Given an ID and some storage, clone the ID into the storage as efficiently as
// possible. Specifically, if :storage already contains an ID of the same type,
// perform a deep copy into the existing ID. Otherwise, delete the existing ID
// (if any) and create a new clone to store there.
void
clone_into(id_interface*& storage, id_interface const* id);
// Same, but where the storage is a shared_ptr.
void
clone_into(std::shared_ptr<id_interface>& storage, id_interface const* id);

// captured_id is used to capture an ID for long-term storage (beyond the point
// where the id_interface reference will be valid).
struct captured_id
{
    captured_id()
    {
    }
    captured_id(id_interface const& id)
    {
        this->capture(id);
    }
    captured_id(captured_id const& other)
    {
        if (other.is_initialized())
            this->capture(other.get());
    }
    captured_id(captured_id&& other)
    {
        id_ = std::move(other.id_);
    }
    captured_id&
    operator=(captured_id const& other)
    {
        if (other.is_initialized())
            this->capture(other.get());
        else
            this->clear();
        return *this;
    }
    captured_id&
    operator=(captured_id&& other)
    {
        id_ = std::move(other.id_);
        return *this;
    }
    void
    clear()
    {
        id_.reset();
    }
    void
    capture(id_interface const& new_id)
    {
        clone_into(id_, &new_id);
    }
    bool
    is_initialized() const
    {
        return id_ ? true : false;
    }
    id_interface const&
    get() const
    {
        return *id_;
    }
    bool
    matches(id_interface const& id) const
    {
        return id_ && *id_ == id;
    }
    friend void
    swap(captured_id& a, captured_id& b)
    {
        swap(a.id_, b.id_);
    }

 private:
    std::shared_ptr<id_interface> id_;
};
bool
operator==(captured_id const& a, captured_id const& b);
bool
operator!=(captured_id const& a, captured_id const& b);
bool
operator<(captured_id const& a, captured_id const& b);

// ref(id) wraps a reference to an id_interface so that it can be combined.
struct id_ref : id_interface
{
    id_ref() : id_(0), ownership_()
    {
    }

    id_ref(id_interface const& id) : id_(&id), ownership_()
    {
    }

    id_interface*
    clone() const
    {
        id_ref* copy = new id_ref;
        this->deep_copy(copy);
        return copy;
    }

    bool
    equals(id_interface const& other) const
    {
        id_ref const& other_id = static_cast<id_ref const&>(other);
        return *id_ == *other_id.id_;
    }

    bool
    less_than(id_interface const& other) const
    {
        id_ref const& other_id = static_cast<id_ref const&>(other);
        return *id_ < *other_id.id_;
    }

    void
    deep_copy(id_interface* copy) const
    {
        auto& typed_copy = *static_cast<id_ref*>(copy);
        if (ownership_)
        {
            typed_copy.ownership_ = ownership_;
            typed_copy.id_ = id_;
        }
        else
        {
            typed_copy.ownership_.reset(id_->clone());
            typed_copy.id_ = typed_copy.ownership_.get();
        }
    }

 private:
    id_interface const* id_;
    std::shared_ptr<id_interface> ownership_;
};
inline id_ref
ref(id_interface const& id)
{
    return id_ref(id);
}

// simple_id<Value> takes a regular type (Value) and implements id_interface for
// it. The type Value must be copyable, comparable for equality and ordering
// (i.e., supply == and < operators), and convertible to a string.
template<class Value>
struct simple_id : id_interface
{
 public:
    simple_id()
    {
    }

    simple_id(Value value) : value_(value)
    {
    }

    Value const&
    value() const
    {
        return value_;
    }

    id_interface*
    clone() const
    {
        return new simple_id(value_);
    }

    bool
    equals(id_interface const& other) const
    {
        simple_id const& other_id = static_cast<simple_id const&>(other);
        return value_ == other_id.value_;
    }

    bool
    less_than(id_interface const& other) const
    {
        simple_id const& other_id = static_cast<simple_id const&>(other);
        return value_ < other_id.value_;
    }

    void
    deep_copy(id_interface* copy) const
    {
        *static_cast<simple_id*>(copy) = *this;
    }

 private:
    Value value_;
};

// make_id(value) creates a simple_id with the given value.
template<class Value>
simple_id<Value>
make_id(Value value)
{
    return simple_id<Value>(value);
}

// simple_id_by_reference is like simple_id but takes a pointer to the value.
// The value is only copied if the ID is cloned or deep-copied.
template<class Value>
struct simple_id_by_reference : id_interface
{
    simple_id_by_reference() : value_(0), storage_()
    {
    }

    simple_id_by_reference(Value const* value) : value_(value), storage_()
    {
    }

    id_interface*
    clone() const
    {
        simple_id_by_reference* copy = new simple_id_by_reference;
        this->deep_copy(copy);
        return copy;
    }

    bool
    equals(id_interface const& other) const
    {
        simple_id_by_reference const& other_id
            = static_cast<simple_id_by_reference const&>(other);
        return *value_ == *other_id.value_;
    }

    bool
    less_than(id_interface const& other) const
    {
        simple_id_by_reference const& other_id
            = static_cast<simple_id_by_reference const&>(other);
        return *value_ < *other_id.value_;
    }

    void
    deep_copy(id_interface* copy) const
    {
        auto& typed_copy = *static_cast<simple_id_by_reference*>(copy);
        if (storage_)
        {
            typed_copy.storage_ = this->storage_;
            typed_copy.value_ = this->value_;
        }
        else
        {
            typed_copy.storage_.reset(new Value(*this->value_));
            typed_copy.value_ = typed_copy.storage_.get();
        }
    }

 private:
    Value const* value_;
    std::shared_ptr<Value> storage_;
};

// make_id_by_reference(value) creates a simple_id_by_reference for :value.
template<class Value>
simple_id_by_reference<Value>
make_id_by_reference(Value const& value)
{
    return simple_id_by_reference<Value>(&value);
}

// id_pair implements the ID interface for a pair of IDs.
template<class Id0, class Id1>
struct id_pair : id_interface
{
    id_pair()
    {
    }

    id_pair(Id0 const& id0, Id1 const& id1) : id0_(id0), id1_(id1)
    {
    }

    id_interface*
    clone() const
    {
        id_pair* copy = new id_pair;
        this->deep_copy(copy);
        return copy;
    }

    bool
    equals(id_interface const& other) const
    {
        id_pair const& other_id = static_cast<id_pair const&>(other);
        return id0_.equals(other_id.id0_) && id1_.equals(other_id.id1_);
    }

    bool
    less_than(id_interface const& other) const
    {
        id_pair const& other_id = static_cast<id_pair const&>(other);
        return id0_.less_than(other_id.id0_)
               || (id0_.equals(other_id.id0_) && id1_.less_than(other_id.id1_));
    }

    void
    deep_copy(id_interface* copy) const
    {
        id_pair* typed_copy = static_cast<id_pair*>(copy);
        id0_.deep_copy(&typed_copy->id0_);
        id1_.deep_copy(&typed_copy->id1_);
    }

 private:
    Id0 id0_;
    Id1 id1_;
};

// combine_ids(id0, id1) combines id0 and id1 into a single ID pair.
template<class Id0, class Id1>
auto
combine_ids(Id0 const& id0, Id1 const& id1)
{
    return id_pair<Id0, Id1>(id0, id1);
}

// Combine more than two IDs into nested pairs.
template<class Id0, class Id1, class... Rest>
auto
combine_ids(Id0 const& id0, Id1 const& id1, Rest const&... rest)
{
    return combine_ids(combine_ids(id0, id1), rest...);
}

// Allow combine_ids() to take a single argument for variadic purposes.
template<class Id0>
auto
combine_ids(Id0 const& id0)
{
    return id0;
}

// no_id can be used when you have nothing to identify.
struct no_id_type
{
};
static simple_id<no_id_type*> const no_id(nullptr);

// unit_id can be used when there is only possible identify.
struct unit_id_type
{
};
static simple_id<unit_id_type*> const unit_id(nullptr);

} // namespace alia


#include <functional>




// This file defines the core types and functions of the signals module.

namespace alia {

// Signals are passed by const reference into UI functions.
// They're typically created directly at the call site as function arguments
// and are only valid for the life of the function call.
// Signals wrappers are templated and store copies of the actual wrapped
// accessor, which allows them to be easily composed at the call site,
// without requiring any memory allocation.

// direction tags
struct read_only_signal
{
};
struct write_only_signal
{
};
struct bidirectional_signal
{
};

// signal_direction_is_compatible<Expected,Actual>::value yields a compile-time
// boolean indicating whether or not a signal with :Actual direction can be used
// in a context expecting :Expected direction.
// In the general case, the signals are not compatible.
template<class Expected, class Actual>
struct signal_direction_is_compatible : std::false_type
{
};
// If the directions are the same, this is trivially true.
template<class Same>
struct signal_direction_is_compatible<Same, Same> : std::true_type
{
};
// A bidirectional signal can work as anything.
template<class Expected>
struct signal_direction_is_compatible<Expected, bidirectional_signal>
    : std::true_type
{
};
// Resolve ambiguity.
template<>
struct signal_direction_is_compatible<
    bidirectional_signal,
    bidirectional_signal> : std::true_type
{
};

// signal_direction_intersection<A,B>::type, where A and B are signal
// directions, yields a direction that only has the capabilities that are common
// to both A and B.
template<class A, class B>
struct signal_direction_intersection
{
};
// If the directions are the same, this is trivial.
template<class Same>
struct signal_direction_intersection<Same, Same>
{
    typedef Same type;
};
// A bidirectional signal has both capabilities.
template<class A>
struct signal_direction_intersection<A, bidirectional_signal>
{
    typedef A type;
};
template<class B>
struct signal_direction_intersection<bidirectional_signal, B>
{
    typedef B type;
};
// Resolve ambiguity.
template<>
struct signal_direction_intersection<bidirectional_signal, bidirectional_signal>
{
    typedef bidirectional_signal type;
};

// signal_direction_union<A,B>::type, where A and B are signal directions,
// yields a direction that has the union of the capabilities of A and B.
template<class A, class B>
struct signal_direction_union;
// If the directions are the same, this is trivial.
template<class Same>
struct signal_direction_union<Same, Same>
{
    typedef Same type;
};
template<class A, class B>
struct signal_direction_union
{
    // All other combinations yield bidirectional signals.
    typedef bidirectional_signal type;
};

// untyped_signal_base defines functionality common to all signals, irrespective
// of the type of the value that the signal carries.
struct untyped_signal_base
{
    // Can the signal currently be read from?
    virtual bool
    is_readable() const = 0;

    // A signal must supply an ID that uniquely identifies its value.
    // The ID is required to be valid if is_readable() returns true.
    // (It may be valid even if is_readable() returns false, which would mean
    // that the signal can identify its value but doesn't know it yet.)
    // The returned ID reference is only valid as long as the signal itself is
    // valid.
    virtual id_interface const&
    value_id() const = 0;

    // Can the signal currently be written to?
    virtual bool
    is_writable() const = 0;
};

template<class Value>
struct signal_interface : untyped_signal_base
{
    typedef Value value_type;

    // Read the signal's value. The reference returned here is only guaranteed
    // to be valid as long as the accessor itself is valid.
    virtual Value const&
    read() const = 0;

    // Write the signal's value.
    virtual void
    write(Value const& value) const = 0;
};

template<class Derived, class Value, class Direction>
struct signal_base : signal_interface<Value>
{
    typedef Direction direction_tag;

    template<class Index>
    auto operator[](Index const& index) const;
};

template<class Derived, class Value, class Direction>
struct signal : signal_base<Derived, Value, Direction>
{
};

template<class Derived, class Value>
struct signal<Derived, Value, read_only_signal>
    : signal_base<Derived, Value, read_only_signal>
{
    // These must be defined to satisfy the interface requirements, but they
    // obviously won't be used on a read-only signal.
    // LCOV_EXCL_START
    bool
    is_writable() const
    {
        return false;
    }
    void
    write(Value const& value) const
    {
    }
    // LCOV_EXCL_STOP
};

template<class Derived, class Value>
struct signal<Derived, Value, write_only_signal>
    : signal_base<Derived, Value, write_only_signal>
{
    // These must be defined to satisfy the interface requirements, but they
    // obviously won't be used on a write-only signal.
    // LCOV_EXCL_START
    id_interface const&
    value_id() const
    {
        return no_id;
    }
    bool
    is_readable() const
    {
        return false;
    }
    Value const&
    read() const
    {
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnull-dereference"
#endif
        return *(Value const*) nullptr;
#ifdef __clang__
#pragma clang diagnostic pop
#endif
    }
    // LCOV_EXCL_STOP
};

// signal_ref is a reference to a signal that acts as a signal itself.
template<class Value, class Direction>
struct signal_ref : signal<signal_ref<Value, Direction>, Value, Direction>
{
    // Construct from any signal with compatible direction.
    template<class OtherSignal, class OtherDirection>
    signal_ref(
        signal<OtherSignal, Value, OtherDirection> const& signal,
        std::enable_if_t<
            signal_direction_is_compatible<Direction, OtherDirection>::value,
            int> = 0)
        : ref_(&signal)
    {
    }
    // Construct from another signal_ref. - This is meant to prevent unnecessary
    // layers of indirection.
    signal_ref(signal_ref<Value, Direction> const& other) : ref_(other.ref_)
    {
    }

    // implementation of signal_interface...

    bool
    is_readable() const
    {
        return ref_->is_readable();
    }
    Value const&
    read() const
    {
        return ref_->read();
    }
    id_interface const&
    value_id() const
    {
        return ref_->value_id();
    }
    bool
    is_writable() const
    {
        return ref_->is_writable();
    }
    void
    write(Value const& value) const
    {
        ref_->write(value);
    }

 private:
    signal_interface<Value> const* ref_;
};

// readable<Value> denotes a reference to a readable signal carrying values of
// type :Value.
template<class Value>
using readable = signal_ref<Value, read_only_signal>;

// writable<Value> denotes a reference to a writable signal carrying values of
// type :Value.
template<class Value>
using writable = signal_ref<Value, write_only_signal>;

// bidirectional<Value> denotes a reference to a bidirectional signal carrying
// values of type :Value.
template<class Value>
using bidirectional = signal_ref<Value, bidirectional_signal>;

// is_signal_type<T>::value yields a compile-time boolean indicating whether or
// not T is an alia signal type.
template<class T>
struct is_signal_type : std::is_base_of<untyped_signal_base, T>
{
};

// signal_can_read<Signal>::value yields a compile-time boolean indicating
// whether or not the given signal type supports reading.
template<class Signal>
struct signal_can_read : signal_direction_is_compatible<
                             read_only_signal,
                             typename Signal::direction_tag>
{
};

// is_readable_signal_type<T>::value yields a compile-time boolean indicating
// whether or not T is an alia signal that supports reading.
template<class T>
struct is_readable_signal_type : std::conditional_t<
                                     is_signal_type<T>::value,
                                     signal_can_read<T>,
                                     std::false_type>
{
};

// Is :signal currently readable?
// Unlike calling signal.is_readable() directly, this will generate a
// compile-time error if the signal's type doesn't support reading.
template<class Signal>
std::enable_if_t<signal_can_read<Signal>::value, bool>
signal_is_readable(Signal const& signal)
{
    return signal.is_readable();
}

// Read a signal's value.
// Unlike calling signal.read() directly, this will generate a compile-time
// error if the signal's type doesn't support reading and a run-time error if
// the signal isn't currently readable.
template<class Signal>
std::enable_if_t<
    signal_can_read<Signal>::value,
    typename Signal::value_type const&>
read_signal(Signal const& signal)
{
    assert(signal.is_readable());
    return signal.read();
}

// signal_can_write<Signal>::value yields a compile-time boolean indicating
// whether or not the given signal type supports writing.
template<class Signal>
struct signal_can_write : signal_direction_is_compatible<
                              write_only_signal,
                              typename Signal::direction_tag>
{
};

// is_writable_signal_type<T>::value yields a compile-time boolean indicating
// whether or not T is an alia signal that supports writing.
template<class T>
struct is_writable_signal_type : std::conditional_t<
                                     is_signal_type<T>::value,
                                     signal_can_write<T>,
                                     std::false_type>
{
};

// Is :signal currently writable?
// Unlike calling signal.is_writable() directly, this will generate a
// compile-time error if the signal's type doesn't support writing.
template<class Signal>
std::enable_if_t<signal_can_write<Signal>::value, bool>
signal_is_writable(Signal const& signal)
{
    return signal.is_writable();
}

// Write a signal's value.
// Unlike calling signal.write() directly, this will generate a compile-time
// error if the signal's type doesn't support writing and a run-time error if
// the signal isn't currently writable.
template<class Signal, class Value>
std::enable_if_t<signal_can_write<Signal>::value>
write_signal(Signal const& signal, Value const& value)
{
    assert(signal.is_writable());
    signal.write(value);
}

// signal_is_bidirectional<Signal>::value yields a compile-time boolean
// indicating whether or not the given signal type supports both reading and
// writing.
template<class Signal>
struct signal_is_bidirectional : signal_direction_is_compatible<
                                     bidirectional_signal,
                                     typename Signal::direction_tag>
{
};

// is_bidirectional_signal_type<T>::value yields a compile-time boolean
// indicating whether or not T is an alia signal that supports both reading and
// writing.
template<class T>
struct is_bidirectional_signal_type : std::conditional_t<
                                          is_signal_type<T>::value,
                                          signal_is_bidirectional<T>,
                                          std::false_type>
{
};

} // namespace alia

#include <cassert>

// This file defines the data retrieval library used for associating mutable
// state and cached data with alia content graphs. It is designed so that each
// node emitted by an application is associated with a unique instance of data,
// even if there is no specific external identifier for that node.
//
// More generally, if you replace "node" with "subexpression evaluation" in the
// previous sentence, it can be used to associate data with particular points in
// the evaluation of any function. This can be useful in situations where you
// need to evaluate a particular function many times with slightly different
// inputs and you want to reuse the work that was done in earlier evaluations
// without a lot of manual bookkeeping.
//
// To understand what's going on here, imagine the evaluation of a function on
// a simple in-order, single-threaded processor. We can represent all possible
// execution flows using a single DAG where each node represents the execution
// of a particular instruction by the processor and edges represent the
// transition to the next instruction. Nodes with multiple edges leaving them
// represent the execution of branch instructions, while nodes with multiple
// edges coming in are points where multiple branches merge back into a single
// flow.
//
// Since the graph is a DAG, loops are represented by unrolling them.
// Similarly, function calls are represented by inlining the callee's graph
// into the caller's graph (with appropriate argument substitutions).
// Note that both of these features make the graph potentially infinite.
// Furthermore, if calls to function pointers are involved, parts of the graph
// may be entirely unknown.
//
// Thus, for an arbitrary function, we cannot construct its graph a priori.
// However, we CAN observe a particular evaluation of the function and
// construct its path through the graph. We can also observe multiple
// evaluations and construct the portion of the DAG that these executions
// cover. In other words, if we're only interested in portions of the graph
// that are reached by actual evaluations of the function, we can lazily
// construct them by simply observing those evaluations.
//
// And that is essentially what this library does. In order to use it, you
// must annotate the control flow in your function, and it uses these
// annotations to trace each evaluation's flow through the graph, constructing
// unexplored regions as they're encountered. The graph is used to store data
// that is made available to your function as it executes.
//
// One problem with all this is that sometimes a subexpression evaluation
// (content node) is associated with a particular piece of input data and the
// evaluation of that input data is not fixed within the graph (e.g., it's in a
// list of items where you can remove or shuffle items). In cases like this, we
// allow the application to attach an explicit name to the subgraph
// representing the evaluation of that expression, and we ensure that that
// subgraph is always used where that name is encountered.

namespace alia {

// It's worth noting here that the storage of the graph is slightly different
// from what's described above. In reality, the only nodes the library knows
// about are the annotated branch nodes and ones where you request data.
// Other nodes are irrelevant, and the library never knows about them.
// Furthermore, not all edges need to be stored explicitly.

// A data node is a node in the graph that represents the retrieval of data,
// and thus it stores the data associated with that retrieval.
// Data nodes are stored as linked lists, held by data_blocks.
//
// Note that a data node is capable of storing any type of data.
// data_node is a base class for all data nodes.
// typed_data_node<T> represents data nodes that store values of type T.
//
struct data_node : noncopyable
{
    data_node() : next(0)
    {
    }
    virtual ~data_node()
    {
    }
    data_node* next;
};
template<class T>
struct typed_data_node : data_node
{
    T value;
};

struct named_block_ref_node;

// A data_block represents a block of execution. During a single evaluation,
// either all nodes in the block are executed or all nodes are bypassed, and, if
// executed, they are always executed in the same order. (It's conceptually
// similar to a 'basic block' except that other nodes may be executed in between
// nodes in a data_block.)
struct data_block : noncopyable
{
    // the list of nodes in this block
    data_node* nodes = nullptr;

    // a flag to track if the block's cache is clear
    bool cache_clear = true;

    // list of named blocks referenced from this data block - The references
    // maintain shared ownership of the named blocks. The order of the
    // references indicates the order in which the block references appeared in
    // the last pass. When the content graph is stable and this order is
    // constant, we can find the blocks with a very small, constant cost.
    named_block_ref_node* named_blocks = nullptr;

    ~data_block();
};

// Clear all data from a data block.
// Note that this recursively processes child blocks.
void
clear_data_block(data_block& block);

struct naming_map_node;

// data_graph stores the data graph associated with a function.
struct data_graph : noncopyable
{
    data_block root_block;

    naming_map_node* map_list = nullptr;

    // This list stores unused references to named blocks. When named block
    // references disappear from a traversal, it's possible that they've done
    // so only because the traversal was interrupted by an exception.
    // Therefore, they're kept here temporarily to keep the named blocks alive
    // until a complete traversal can establish new references to the named
    // blocks. They're cleaned up when someone calls gc_named_data(graph)
    // following a complete traversal.
    named_block_ref_node* unused_named_block_refs = nullptr;
};

struct naming_map;

// data_traversal stores the state associated with a single traversal of a
// data_graph.
struct data_traversal
{
    data_graph* graph;
    naming_map* active_map;
    data_block* active_block;
    named_block_ref_node* predicted_named_block;
    named_block_ref_node* used_named_blocks;
    named_block_ref_node** named_block_next_ptr;
    data_node** next_data_ptr;
    bool gc_enabled;
    bool cache_clearing_enabled;
};

// The utilities here operate on data_traversals. However, the data_graph
// library is intended to be used in scenarios where the data_traversal object
// is part of a larger context. Thus, any utilities here that are intended to be
// used directly by the application developer are designed to accept a generic
// context parameter. The only requirement on that paramater is that it defines
// the function get_data_traversal(ctx), which returns a reference to a
// data_traversal.

// If using this library directly, the data_traversal itself can serve as the
// context.
inline data_traversal&
get_data_traversal(data_traversal& ctx)
{
    return ctx;
}

// A scoped_data_block activates the associated data_block at the beginning
// of its scope and deactivates it at the end. It's useful anytime there is a
// branch in the code and you need to activate the block associated with the
// taken branch while that branch is active.
// Note that the macros defined below make heavy use of this and reduce the
// need for applications to use it directly.
struct scoped_data_block : noncopyable
{
    scoped_data_block() : traversal_(0)
    {
    }

    template<class Context>
    scoped_data_block(Context& ctx, data_block& block)
    {
        begin(ctx, block);
    }

    ~scoped_data_block()
    {
        end();
    }

    template<class Context>
    void
    begin(Context& ctx, data_block& block)
    {
        begin(get_data_traversal(ctx), block);
    }

    void
    begin(data_traversal& traversal, data_block& block);

    void
    end();

 private:
    data_traversal* traversal_;
    // old state
    data_block* old_active_block_;
    named_block_ref_node* old_predicted_named_block_;
    named_block_ref_node* old_used_named_blocks_;
    named_block_ref_node** old_named_block_next_ptr_;
    data_node** old_next_data_ptr_;
};

// A named_block is like a scoped_data_block, but instead of supplying a
// data_block directly, you provide an ID, and it finds the block associated
// with that ID and activates it.
//
// This is the mechanism for dealing with dynamically ordered data.
// named_blocks are free to move around within the graph as long as they
// maintain the same IDs.
//
// A naming_context provides a context for IDs. IDs used within one naming
// context can be reused within another without conflict.
//
// named_blocks are automatically garbage collected when the library detects
// that they've disappeared from the graph. The logic for this is fairly
// sophisticated, and it generally won't mistakingly collect named_blocks in
// inactive regions of the graph. However, it still may not always do what you
// want. In those cases, you can specify the manual_delete flag. This will
// prevent the library from collecting the block. It can be deleted manually by
// calling delete_named_data(ctx, id). If that never happens, it will be deleted
// when the data_graph that it belongs to is destroyed. (But this is likely to
// still be a memory leak, since the data_graph might live on as long as the
// application is running.)

// The manual deletion flag is specified via its own structure to make it very
// obvious at the call site.
struct manual_delete
{
    explicit manual_delete(bool value) : value(value)
    {
    }
    bool value;
};

struct named_block : noncopyable
{
    named_block()
    {
    }

    template<class Context>
    named_block(
        Context& ctx,
        id_interface const& id,
        manual_delete manual = manual_delete(false))
    {
        begin(ctx, id, manual);
    }

    template<class Context>
    void
    begin(
        Context& ctx,
        id_interface const& id,
        manual_delete manual = manual_delete(false))
    {
        begin(get_data_traversal(ctx), get_naming_map(ctx), id, manual);
    }

    void
    begin(
        data_traversal& traversal,
        naming_map& map,
        id_interface const& id,
        manual_delete manual);

    void
    end();

 private:
    scoped_data_block scoped_data_block_;
};

struct naming_context : noncopyable
{
    naming_context()
    {
    }

    template<class Context>
    naming_context(Context& ctx)
    {
        begin(ctx);
    }

    ~naming_context()
    {
        end();
    }

    template<class Context>
    void
    begin(Context& ctx)
    {
        begin(get_data_traversal(ctx));
    }

    void
    begin(data_traversal& traversal);

    void
    end()
    {
    }

    data_traversal&
    traversal()
    {
        return *traversal_;
    }
    naming_map&
    map()
    {
        return *map_;
    }

 private:
    data_traversal* traversal_;
    naming_map* map_;
};
inline data_traversal&
get_data_traversal(naming_context& ctx)
{
    return ctx.traversal();
}
inline naming_map&
get_naming_map(naming_context& ctx)
{
    return ctx.map();
}

// retrieve_naming_map gets a data_map from a data_traveral and registers it
// with the underlying graph. It can be used to retrieve additional naming maps
// from a data graph, in case you want to manage them yourself.
naming_map*
retrieve_naming_map(data_traversal& traversal);

// delete_named_block(ctx, id) deletes the data associated with a particular
// named block, as identified by the given ID.

void
delete_named_block(data_graph& graph, id_interface const& id);

template<class Context>
void
delete_named_block(Context& ctx, id_interface const& id)
{
    delete_named_block(*get_data_traversal(ctx).graph, id);
}

// This is a macro that, given a context, an uninitialized named_block, and an
// ID, combines the ID with another ID which is unique to that location in the
// code (but not the graph), and then initializes the named_block with the
// combined ID.
// This is not as generally useful as naming_context, but it can be used to
// identify the combinaion of a function and its argument.
#define ALIA_BEGIN_LOCATION_SPECIFIC_NAMED_BLOCK(ctx, named_block, id)         \
    {                                                                          \
        static int _alia_dummy_static;                                         \
        named_block.begin(ctx, combine_ids(make_id(&_alia_dummy_static), id)); \
    }

// disable_gc(traversal) disables the garbage collector for a data traversal.
// It's used when you don't intend to visit the entire active part of the graph
// and thus don't want the garbage collector to collect the unvisited parts.
// It should be invoked before actually beginning a traversal.
// When using this, if you visit named blocks, you must visit all blocks in a
// data_block in the same order that they were last visited with the garbage
// collector enabled. However, you don't have to finish the entire sequence.
// If you violate this rule, you'll get a named_block_out_of_order exception.
struct named_block_out_of_order : exception
{
    named_block_out_of_order()
        : exception("named block order must remain constant with GC disabled")
    {
    }
};
void
disable_gc(data_traversal& traversal);

// scoped_cache_clearing_disabler will prevent the library from clearing the
// cache of inactive blocks within its scope.
struct scoped_cache_clearing_disabler
{
    scoped_cache_clearing_disabler() : traversal_(0)
    {
    }
    template<class Context>
    scoped_cache_clearing_disabler(Context& ctx)
    {
        begin(ctx);
    }
    ~scoped_cache_clearing_disabler()
    {
        end();
    }
    template<class Context>
    void
    begin(Context& ctx)
    {
        begin(get_data_traversal(ctx));
    }
    void
    begin(data_traversal& traversal);
    void
    end();

 private:
    data_traversal* traversal_;
    bool old_cache_clearing_state_;
};

// get_data(traversal, &ptr) represents a data node in the data graph.
// The call retrieves data from the graph at the current point in the
// traversal, assigns its address to *ptr, and advances the traversal to the
// next node.
// The return value is true if the data at the node was just constructed and
// false if it already existed.
//
// Note that get_data should normally not be used directly by the application.

template<class Context, class T>
bool
get_data(Context& ctx, T** ptr)
{
    data_traversal& traversal = get_data_traversal(ctx);
    data_node* node = *traversal.next_data_ptr;
    if (node)
    {
        if (!dynamic_cast<typed_data_node<T>*>(node))
            assert(dynamic_cast<typed_data_node<T>*>(node));
        typed_data_node<T>* typed_node = static_cast<typed_data_node<T>*>(node);
        traversal.next_data_ptr = &node->next;
        *ptr = &typed_node->value;
        return false;
    }
    else
    {
        typed_data_node<T>* new_node = new typed_data_node<T>;
        *traversal.next_data_ptr = new_node;
        traversal.next_data_ptr = &new_node->next;
        *ptr = &new_node->value;
        return true;
    }
}

// get_cached_data(ctx, &ptr) is identical to get_data(ctx, &ptr), but the
// data stored in the node is understood to be a cached value of data that's
// generated by the application. The system assumes that the data can be
// regenerated if it's lost.

struct cached_data
{
    virtual ~cached_data()
    {
    }
};

template<class T>
struct typed_cached_data : cached_data
{
    T value;
};

struct cached_data_holder
{
    cached_data_holder() : data(0)
    {
    }
    ~cached_data_holder()
    {
        delete data;
    }
    cached_data* data;
};

template<class Context, class T>
bool
get_cached_data(Context& ctx, T** ptr)
{
    cached_data_holder* holder;
    get_data(ctx, &holder);
    if (holder->data)
    {
        assert(dynamic_cast<typed_cached_data<T>*>(holder->data));
        typed_cached_data<T>* data
            = static_cast<typed_cached_data<T>*>(holder->data);
        *ptr = &data->value;
        return false;
    }
    typed_cached_data<T>* data = new typed_cached_data<T>;
    holder->data = data;
    *ptr = &data->value;
    return true;
}

// Clear all cached data stored within a data block.
// Note that this recursively processes child blocks.
void
clear_cached_data(data_block& block);

// get_keyed_data(ctx, key, &signal) is a utility for retrieving cached data
// from a data graph.
// It stores not only the data but also a key that identifies the data.
// The key is presented at each retrieval, and if it changes, the associated
// data is invalidated and must be recomputed.

// The return value is true iff the data needs to be recomputed.

template<class Data>
struct keyed_data
{
    captured_id key;
    bool is_valid;
    Data value;
    keyed_data() : is_valid(false)
    {
    }
};

template<class Data>
bool
is_valid(keyed_data<Data> const& data)
{
    return data.is_valid;
}

template<class Data>
void
invalidate(keyed_data<Data>& data)
{
    data.is_valid = false;
    data.key.clear();
}

template<class Data>
void
mark_valid(keyed_data<Data>& data)
{
    data.is_valid = true;
}

template<class Data>
bool
refresh_keyed_data(keyed_data<Data>& data, id_interface const& key)
{
    if (!data.key.matches(key))
    {
        data.is_valid = false;
        data.key.capture(key);
        return true;
    }
    return false;
}

template<class Data>
void
set(keyed_data<Data>& data, Data const& value)
{
    data.value = value;
    mark_valid(data);
}

template<class Data>
Data const&
get(keyed_data<Data> const& data)
{
    assert(is_valid(data));
    return data.value;
}

template<class Data>
struct keyed_data_signal
    : signal<keyed_data_signal<Data>, Data, bidirectional_signal>
{
    keyed_data_signal()
    {
    }
    keyed_data_signal(keyed_data<Data>* data) : data_(data)
    {
    }
    bool
    is_readable() const
    {
        return data_->is_valid;
    }
    Data const&
    read() const
    {
        return data_->value;
    }
    id_interface const&
    value_id() const
    {
        return data_->key.is_initialized() ? data_->key.get() : no_id;
    }
    bool
    is_writable() const
    {
        return true;
    }
    void
    write(Data const& value) const
    {
        alia::set(*data_, value);
    }

 private:
    keyed_data<Data>* data_;
};

template<class Data>
keyed_data_signal<Data>
make_signal(keyed_data<Data>* data)
{
    return keyed_data_signal<Data>(data);
}

template<class Context, class Data>
bool
get_keyed_data(
    Context& ctx, id_interface const& key, keyed_data_signal<Data>* signal)
{
    keyed_data<Data>* ptr;
    get_cached_data(ctx, &ptr);
    refresh_keyed_data(*ptr, key);
    *signal = make_signal(ptr);
    return !is_valid(*ptr);
};

// This is another form of get_keyed_data where there's no signal to guard
// access to the retrieved data. Thus, it's up to the caller to track whether
// or not the data is properly initialized.

template<class Data>
struct raw_keyed_data
{
    captured_id key;
    Data data;
};

template<class Context, class Data>
bool
get_keyed_data(Context& ctx, id_interface const& key, Data** data)
{
    raw_keyed_data<Data>* ptr;
    bool is_new = false;
    if (get_cached_data(ctx, &ptr))
    {
        ptr->key.capture(key);
        is_new = true;
    }
    else if (!ptr->key.matches(key))
    {
        ptr->key.capture(key);
        ptr->data = Data();
        is_new = true;
    }
    *data = &ptr->data;
    return is_new;
};

// scoped_data_traversal can be used to manage a traversal of a graph.
// begin(graph, traversal) will initialize traversal to act as a traversal of
// graph.
struct scoped_data_traversal
{
    scoped_data_traversal()
    {
    }

    scoped_data_traversal(data_graph& graph, data_traversal& traversal)
    {
        begin(graph, traversal);
    }

    ~scoped_data_traversal()
    {
        end();
    }

    void
    begin(data_graph& graph, data_traversal& traversal);

    void
    end();

 private:
    scoped_data_block root_block_;
    naming_context root_map_;
};

} // namespace alia


namespace alia {

struct system
{
    data_graph data;
    std::function<void(context)> controller;
};

} // namespace alia





// This file defines various utilities for working with signals.
// (These are mostly meant to be used internally.)

namespace alia {

// regular_signal is a partial implementation of the signal interface for
// cases where the value ID of the signal is simply the value itself.
template<class Derived, class Value, class Direction>
struct regular_signal : signal<Derived, Value, Direction>
{
    id_interface const&
    value_id() const
    {
        if (this->is_readable())
        {
            id_ = make_id_by_reference(this->read());
            return id_;
        }
        return no_id;
    }

 private:
    mutable simple_id_by_reference<Value> id_;
};

// lazy_reader is used to create signals that lazily generate their values.
// It provides storage for the computed value and ensures that it's only
// computed once.
template<class Value>
struct lazy_reader
{
    lazy_reader() : already_generated_(false)
    {
    }
    template<class Generator>
    Value const&
    read(Generator const& generator) const
    {
        if (!already_generated_)
        {
            value_ = generator();
            already_generated_ = true;
        }
        return value_;
    }

 private:
    mutable bool already_generated_;
    mutable Value value_;
};

} // namespace alia


// This file defines various utilities for constructing basic signals.

namespace alia {

// empty<Value>() gives a signal that never has a value.
template<class Value>
struct empty_signal : signal<empty_signal<Value>, Value, bidirectional_signal>
{
    empty_signal()
    {
    }
    id_interface const&
    value_id() const
    {
        return no_id;
    }
    bool
    is_readable() const
    {
        return false;
    }
    // Since this is never readable, read() should never be called.
    // LCOV_EXCL_START
    Value const&
    read() const
    {
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnull-dereference"
#endif
        return *(Value const*) nullptr;
#ifdef __clang__
#pragma clang diagnostic pop
#endif
    }
    // LCOV_EXCL_STOP
    bool
    is_writable() const
    {
        return false;
    }
    // Since this is never writable, write() should never be called.
    // LCOV_EXCL_START
    void
    write(Value const& value) const
    {
    }
    // LCOV_EXCL_STOP
};
template<class Value>
empty_signal<Value>
empty()
{
    return empty_signal<Value>();
}

// value(v) creates a read-only signal that carries the value v.
template<class Value>
struct value_signal
    : regular_signal<value_signal<Value>, Value, read_only_signal>
{
    explicit value_signal(Value const& v) : v_(v)
    {
    }
    bool
    is_readable() const
    {
        return true;
    }
    Value const&
    read() const
    {
        return v_;
    }

 private:
    Value v_;
};
template<class Value>
value_signal<Value>
value(Value const& v)
{
    return value_signal<Value>(v);
}

// This is a special overload of value() for C-style string literals.
struct string_literal_signal
    : signal<string_literal_signal, std::string, read_only_signal>
{
    string_literal_signal(char const* x) : text_(x)
    {
    }
    id_interface const&
    value_id() const
    {
        return unit_id;
    }
    bool
    is_readable() const
    {
        return true;
    }
    std::string const&
    read() const
    {
        return lazy_reader_.read([&] { return std::string(text_); });
    }

 private:
    char const* text_;
    lazy_reader<std::string> lazy_reader_;
};
inline string_literal_signal
value(char const* text)
{
    return string_literal_signal(text);
}

// literal operators
namespace literals {
inline value_signal<unsigned long long int>
operator"" _a(unsigned long long int n)
{
    return value(n);
}
inline value_signal<long double> operator"" _a(long double n)
{
    return value(n);
}
inline string_literal_signal operator"" _a(char const* s, size_t n)
{
    return string_literal_signal(s);
}
} // namespace literals

// direct(x), where x is a non-const reference, creates a bidirectional signal
// that directly exposes the value of x.
template<class Value>
struct direct_signal
    : regular_signal<direct_signal<Value>, Value, bidirectional_signal>
{
    explicit direct_signal(Value* v) : v_(v)
    {
    }
    bool
    is_readable() const
    {
        return true;
    }
    Value const&
    read() const
    {
        return *v_;
    }
    bool
    is_writable() const
    {
        return true;
    }
    void
    write(Value const& value) const
    {
        *v_ = value;
    }

 private:
    Value* v_;
};
template<class Value>
direct_signal<Value>
direct(Value& x)
{
    return direct_signal<Value>(&x);
}

// direct(x), where x is a const reference, creates a read-only signal that
// directly exposes the value of x.
template<class Value>
struct direct_const_signal
    : regular_signal<direct_const_signal<Value>, Value, read_only_signal>
{
    explicit direct_const_signal(Value const* v) : v_(v)
    {
    }
    bool
    is_readable() const
    {
        return true;
    }
    Value const&
    read() const
    {
        return *v_;
    }

 private:
    Value const* v_;
};
template<class Value>
direct_const_signal<Value>
direct(Value const& x)
{
    return direct_const_signal<Value>(&x);
}

} // namespace alia




namespace alia {

// lazy_apply(f, args...), where :args are all signals, yields a signal
// to the result of lazily applying the function :f to the values of :args.

// Note that doing this in true variadic fashion is a little insane, so I'm
// just doing the two overloads I need for now...

template<class Result, class Function, class Arg>
struct lazy_apply1_signal : signal<
                                lazy_apply1_signal<Result, Function, Arg>,
                                Result,
                                read_only_signal>
{
    lazy_apply1_signal(Function const& f, Arg const& arg) : f_(f), arg_(arg)
    {
    }
    id_interface const&
    value_id() const
    {
        return arg_.value_id();
    }
    bool
    is_readable() const
    {
        return arg_.is_readable();
    }
    Result const&
    read() const
    {
        return lazy_reader_.read([&] { return f_(arg_.read()); });
    }

 private:
    Function f_;
    Arg arg_;
    lazy_reader<Result> lazy_reader_;
};
template<class Function, class Arg>
auto
lazy_apply(Function const& f, Arg const& arg)
{
    return lazy_apply1_signal<decltype(f(read_signal(arg))), Function, Arg>(
        f, arg);
}

template<class Result, class Function, class Arg0, class Arg1>
struct lazy_apply2_signal
    : signal<
          lazy_apply2_signal<Result, Function, Arg0, Arg1>,
          Result,
          read_only_signal>
{
    lazy_apply2_signal(Function const& f, Arg0 const& arg0, Arg1 const& arg1)
        : f_(f), arg0_(arg0), arg1_(arg1)
    {
    }
    id_interface const&
    value_id() const
    {
        id_ = combine_ids(ref(arg0_.value_id()), ref(arg1_.value_id()));
        return id_;
    }
    bool
    is_readable() const
    {
        return arg0_.is_readable() && arg1_.is_readable();
    }
    Result const&
    read() const
    {
        return lazy_reader_.read(
            [&]() { return f_(arg0_.read(), arg1_.read()); });
    }

 private:
    Function f_;
    Arg0 arg0_;
    Arg1 arg1_;
    mutable id_pair<id_ref, id_ref> id_;
    lazy_reader<Result> lazy_reader_;
};
template<class Function, class Arg0, class Arg1>
auto
lazy_apply(Function const& f, Arg0 const& arg0, Arg1 const& arg1)
{
    return lazy_apply2_signal<
        decltype(f(read_signal(arg0), read_signal(arg1))),
        Function,
        Arg0,
        Arg1>(f, arg0, arg1);
}

template<class Function>
auto
lazy_lift(Function const& f)
{
    return [=](auto&&... args) { return lazy_apply(f, args...); };
}

// apply(ctx, f, args...), where :args are all signals, yields a signal to the
// result of applying the function :f to the values of :args. Unlike lazy_apply,
// this is eager and caches and the result.

enum class apply_status
{
    UNCOMPUTED,
    READY,
    FAILED
};

template<class Value>
struct apply_result_data
{
    counter_type result_version = 0;
    Value result;
    apply_status status = apply_status::UNCOMPUTED;
};

template<class Value>
void
reset(apply_result_data<Value>& data)
{
    if (data.status != apply_status::UNCOMPUTED)
    {
        ++data.result_version;
        data.status = apply_status::UNCOMPUTED;
    }
}

template<class Value>
struct apply_signal : signal<apply_signal<Value>, Value, read_only_signal>
{
    apply_signal(apply_result_data<Value>& data) : data_(&data)
    {
    }
    id_interface const&
    value_id() const
    {
        id_ = make_id(data_->result_version);
        return id_;
    }
    bool
    is_readable() const
    {
        return data_->status == apply_status::READY;
    }
    Value const&
    read() const
    {
        return data_->result;
    }

 private:
    apply_result_data<Value>* data_;
    mutable simple_id<counter_type> id_;
};

template<class Value>
apply_signal<Value>
make_apply_signal(apply_result_data<Value>& data)
{
    return apply_signal<Value>(data);
}

template<class Result>
void
process_apply_args(
    context ctx, apply_result_data<Result>& data, bool& args_ready)
{
}
template<class Result, class Arg, class... Rest>
void
process_apply_args(
    context ctx,
    apply_result_data<Result>& data,
    bool& args_ready,
    Arg const& arg,
    Rest const&... rest)
{
    captured_id* cached_id;
    get_cached_data(ctx, &cached_id);
    if (!signal_is_readable(arg))
    {
        reset(data);
        args_ready = false;
    }
    else if (!cached_id->matches(arg.value_id()))
    {
        reset(data);
        cached_id->capture(arg.value_id());
    }
    process_apply_args(ctx, data, args_ready, rest...);
}

template<class Function, class... Args>
auto
apply(context ctx, Function const& f, Args const&... args)
{
    apply_result_data<decltype(f(read_signal(args)...))>* data_ptr;
    get_cached_data(ctx, &data_ptr);
    auto& data = *data_ptr;
    if (is_refresh_pass(ctx))
    {
        bool args_ready = true;
        process_apply_args(ctx, data, args_ready, args...);
        if (data.status == apply_status::UNCOMPUTED && args_ready)
        {
            try
            {
                data.result = f(read_signal(args)...);
                data.status = apply_status::READY;
            }
            catch (...)
            {
                data.status = apply_status::FAILED;
            }
        }
    }
    return make_apply_signal(data);
}

template<class Function>
auto
lift(context ctx, Function const& f)
{
    return [=](auto&&... args) { return apply(ctx, f, args...); };
}

// alia_method(m) wraps a method name in a lambda so that it can be passed as a
// function object.
#define ALIA_METHOD(m) [](auto&& x, auto&&... args) { return x.m(args...); }
#ifdef ALIA_LOWERCASE_MACROS
#define alia_method(m) ALIA_METHOD(m)
#endif

} // namespace alia


// This file defines the operators for signals.

namespace alia {

#define ALIA_DEFINE_BINARY_SIGNAL_OPERATOR(op)                                 \
    template<                                                                  \
        class A,                                                               \
        class B,                                                               \
        std::enable_if_t<                                                      \
            is_signal_type<A>::value && is_signal_type<B>::value,              \
            int> = 0>                                                          \
    auto operator op(A const& a, B const& b)                                   \
    {                                                                          \
        return lazy_apply([](auto a, auto b) { return a op b; }, a, b);        \
    }

ALIA_DEFINE_BINARY_SIGNAL_OPERATOR(+)
ALIA_DEFINE_BINARY_SIGNAL_OPERATOR(-)
ALIA_DEFINE_BINARY_SIGNAL_OPERATOR(*)
ALIA_DEFINE_BINARY_SIGNAL_OPERATOR(/)
ALIA_DEFINE_BINARY_SIGNAL_OPERATOR (^)
ALIA_DEFINE_BINARY_SIGNAL_OPERATOR(%)
ALIA_DEFINE_BINARY_SIGNAL_OPERATOR(&)
ALIA_DEFINE_BINARY_SIGNAL_OPERATOR(|)
ALIA_DEFINE_BINARY_SIGNAL_OPERATOR(<<)
ALIA_DEFINE_BINARY_SIGNAL_OPERATOR(>>)
ALIA_DEFINE_BINARY_SIGNAL_OPERATOR(==)
ALIA_DEFINE_BINARY_SIGNAL_OPERATOR(!=)
ALIA_DEFINE_BINARY_SIGNAL_OPERATOR(<)
ALIA_DEFINE_BINARY_SIGNAL_OPERATOR(<=)
ALIA_DEFINE_BINARY_SIGNAL_OPERATOR(>)
ALIA_DEFINE_BINARY_SIGNAL_OPERATOR(>=)

#undef ALIA_DEFINE_BINARY_SIGNAL_OPERATOR

#define ALIA_DEFINE_UNARY_SIGNAL_OPERATOR(op)                                  \
    template<class A, std::enable_if_t<is_signal_type<A>::value, int> = 0>     \
    auto operator op(A const& a)                                               \
    {                                                                          \
        return lazy_apply([](auto a) { return op a; }, a);                     \
    }

ALIA_DEFINE_UNARY_SIGNAL_OPERATOR(-)
ALIA_DEFINE_UNARY_SIGNAL_OPERATOR(!)

#undef ALIA_DEFINE_UNARY_SIGNAL_OPERATOR

// The || and && operators require special implementations because they don't
// necessarily need to evaluate both of their arguments...

template<class Arg0, class Arg1>
struct logical_or_signal
    : signal<logical_or_signal<Arg0, Arg1>, bool, read_only_signal>
{
    logical_or_signal(Arg0 const& arg0, Arg1 const& arg1)
        : arg0_(arg0), arg1_(arg1)
    {
    }
    id_interface const&
    value_id() const
    {
        id_ = combine_ids(ref(arg0_.value_id()), ref(arg1_.value_id()));
        return id_;
    }
    bool
    is_readable() const
    {
        // Obviously, this is readable if both of its arguments are readable.
        // However, it's also readable if only one is readable and its value is
        // true.
        return (arg0_.is_readable() && arg1_.is_readable())
               || (arg0_.is_readable() && arg0_.read())
               || (arg1_.is_readable() && arg1_.read());
    }
    bool const&
    read() const
    {
        value_ = (arg0_.is_readable() && arg0_.read())
                 || (arg1_.is_readable() && arg1_.read());
        return value_;
    }

 private:
    Arg0 arg0_;
    Arg1 arg1_;
    mutable id_pair<id_ref, id_ref> id_;
    mutable bool value_;
};
template<
    class A,
    class B,
    std::enable_if_t<
        std::is_base_of<untyped_signal_base, A>::value
            && std::is_base_of<untyped_signal_base, B>::value,
        int> = 0>
auto
operator||(A const& a, B const& b)
{
    return logical_or_signal<A, B>(a, b);
}

template<class Arg0, class Arg1>
struct logical_and_signal
    : signal<logical_and_signal<Arg0, Arg1>, bool, read_only_signal>
{
    logical_and_signal(Arg0 const& arg0, Arg1 const& arg1)
        : arg0_(arg0), arg1_(arg1)
    {
    }
    id_interface const&
    value_id() const
    {
        id_ = combine_ids(ref(arg0_.value_id()), ref(arg1_.value_id()));
        return id_;
    }
    bool
    is_readable() const
    {
        // Obviously, this is readable if both of its arguments are readable.
        // However, it's also readable if only one is readable and its value is
        // false.
        return (arg0_.is_readable() && arg1_.is_readable())
               || (arg0_.is_readable() && !arg0_.read())
               || (arg1_.is_readable() && !arg1_.read());
    }
    bool const&
    read() const
    {
        value_
            = !((arg0_.is_readable() && !arg0_.read())
                || (arg1_.is_readable() && !arg1_.read()));
        return value_;
    }

 private:
    Arg0 arg0_;
    Arg1 arg1_;
    mutable id_pair<id_ref, id_ref> id_;
    mutable bool value_;
};
template<
    class A,
    class B,
    std::enable_if_t<
        std::is_base_of<untyped_signal_base, A>::value
            && std::is_base_of<untyped_signal_base, B>::value,
        int> = 0>
auto
operator&&(A const& a, B const& b)
{
    return logical_and_signal<A, B>(a, b);
}

// This is the equivalent of the ternary operator (or std::conditional) for
// signals.
//
// conditional(b, t, f), where :b, :t and :f are all signals, yields :t
// if :b's value is true and :f if :b's value is false.
//
// :t and :f must have the same value type, and :b's value type must be testable
// in a boolean context.
//
// Note that this is a normal function call, so, unlike an if statement or the
// ternary operator, both :t and :f are fully evaluated. However, they are only
// read if they're selected.
//
template<class Condition, class T, class F>
struct signal_mux : signal<
                        signal_mux<Condition, T, F>,
                        typename T::value_type,
                        typename signal_direction_intersection<
                            typename T::direction_tag,
                            typename F::direction_tag>::type>
{
    signal_mux(Condition condition, T t, F f)
        : condition_(condition), t_(t), f_(f)
    {
    }
    bool
    is_readable() const
    {
        return condition_.is_readable()
               && (condition_.read() ? t_.is_readable() : f_.is_readable());
    }
    typename T::value_type const&
    read() const
    {
        return condition_.read() ? t_.read() : f_.read();
    }
    id_interface const&
    value_id() const
    {
        if (!condition_.is_readable())
            return no_id;
        id_ = combine_ids(
            make_id(condition_.read()),
            condition_.read() ? ref(t_.value_id()) : ref(f_.value_id()));
        return id_;
    }
    bool
    is_writable() const
    {
        return condition_.is_readable()
               && (condition_.read() ? t_.is_writable() : f_.is_writable());
    }
    void
    write(typename T::value_type const& value) const
    {
        if (condition_.read())
            t_.write(value);
        else
            f_.write(value);
    }

 private:
    Condition condition_;
    T t_;
    F f_;
    mutable id_pair<simple_id<bool>, id_ref> id_;
};
template<class Condition, class T, class F>
signal_mux<Condition, T, F>
conditional(Condition const& condition, T const& t, F const& f)
{
    return signal_mux<Condition, T, F>(condition, t, f);
}

// Given a signal to a structure, signal->*field_ptr returns a signal to the
// specified field within the structure.
template<class StructureSignal, class Field>
struct field_signal : signal<
                          field_signal<StructureSignal, Field>,
                          Field,
                          typename StructureSignal::direction_tag>
{
    typedef typename StructureSignal::value_type structure_type;
    typedef Field structure_type::*field_ptr;
    field_signal(StructureSignal structure, field_ptr field)
        : structure_(structure), field_(field)
    {
    }
    bool
    is_readable() const
    {
        return structure_.is_readable();
    }
    Field const&
    read() const
    {
        structure_type const& structure = structure_.read();
        return structure.*field_;
    }
    id_interface const&
    value_id() const
    {
        // Apparently pointers-to-members aren't comparable for order, so
        // instead we use the address of the field if it were in a structure
        // that started at address 0.
        id_ = combine_ids(
            ref(structure_.value_id()),
            make_id(&(((structure_type*) 0)->*field_)));
        return id_;
    }
    bool
    is_writable() const
    {
        return structure_.is_readable() && structure_.is_writable();
    }
    void
    write(Field const& x) const
    {
        structure_type s = structure_.read();
        s.*field_ = x;
        structure_.write(s);
    }

 private:
    StructureSignal structure_;
    field_ptr field_;
    mutable id_pair<id_ref, simple_id<Field*>> id_;
};
template<class StructureSignal, class Field>
std::enable_if_t<
    is_signal_type<StructureSignal>::value,
    field_signal<StructureSignal, Field>>
operator->*(
    StructureSignal const& structure, Field StructureSignal::value_type::*field)
{
    return field_signal<StructureSignal, Field>(structure, field);
}

// ALIA_FIELD(x, f) is equivalent to x->*T::f where T is the value type of x.
#define ALIA_FIELD(x, f) ((x)->*&std::decay<decltype(read_signal(x))>::type::f)
#ifdef ALIA_LOWERCASE_MACROS
#define alia_field(x, f) ALIA_FIELD(x, f)
#endif

// has_value_type<T>::value yields a compile-time boolean indicating whether or
// not T has a value_type member (which is the case for standard containers).
template<class T, class = void_t<>>
struct has_value_type : std::false_type
{
};
template<class T>
struct has_value_type<T, void_t<typename T::value_type>> : std::true_type
{
};

// has_mapped_type<T>::value yields a compile-time boolean indicating whether or
// not T has a mapped_type member (which is the case for standard associative
// containers, or at least the ones that aren't sets).
template<class T, class = void_t<>>
struct has_mapped_type : std::false_type
{
};
template<class T>
struct has_mapped_type<T, void_t<typename T::mapped_type>> : std::true_type
{
};

// subscript_result_type<Container, Index>::type gives the expected type of the
// value that results from invoking the subscript operator on a Container. (This
// is necessary to deal with containers that return proxies.)
//
// The logic is as follows:
// 1 - If the container has a mapped_type field, use that.
// 2 - Otherwise, if the container has a value_type field, use that.
// 3 - Otherwise, just see what operator[] returns.
//
template<class Container, class Index, class = void>
struct subscript_result_type
{
};
template<class Container, class Index>
struct subscript_result_type<
    Container,
    Index,
    std::enable_if_t<has_mapped_type<Container>::value>>
{
    typedef typename Container::mapped_type type;
};
template<class Container, class Index>
struct subscript_result_type<
    Container,
    Index,
    std::enable_if_t<
        !has_mapped_type<Container>::value && has_value_type<Container>::value>>
{
    typedef typename Container::value_type type;
};
template<class Container, class Index>
struct subscript_result_type<
    Container,
    Index,
    std::enable_if_t<
        !has_mapped_type<Container>::value
        && !has_value_type<Container>::value>>
{
    typedef std::decay_t<decltype(
        std::declval<Container>()[std::declval<Index>()])>
        type;
};

// has_at_indexer<Container, Index>::value yields a compile-time boolean
// indicating whether or not Container has an 'at' method that takes an Index.
template<class Container, class Index, class = void_t<>>
struct has_at_indexer : std::false_type
{
};
template<class Container, class Index>
struct has_at_indexer<
    Container,
    Index,
    void_t<decltype(std::declval<Container const&>().at(
        std::declval<Index>()))>> : std::true_type
{
};

template<class Container, class Index>
auto
invoke_const_subscript(
    Container const& container,
    Index const& index,
    std::enable_if_t<!has_at_indexer<Container, Index>::value>* = 0)
    -> decltype(container[index])
{
    return container[index];
}

template<class Container, class Index>
auto
invoke_const_subscript(
    Container const& container,
    Index const& index,
    std::enable_if_t<has_at_indexer<Container, Index>::value>* = 0)
    -> decltype(container.at(index))
{
    return container.at(index);
}

// const_subscript_returns_reference<Container,Index>::value yields a
// compile-time boolean indicating where or not invoke_const_subscript returns
// by reference (vs by value).
template<class Container, class Index>
struct const_subscript_returns_reference
    : std::is_reference<decltype(invoke_const_subscript(
          std::declval<Container>(), std::declval<Index>()))>
{
};

template<class Container, class Index, class = void>
struct const_subscript_invoker
{
};

template<class Container, class Index>
struct const_subscript_invoker<
    Container,
    Index,
    std::enable_if_t<
        const_subscript_returns_reference<Container, Index>::value>>
{
    auto const&
    operator()(Container const& container, Index const& index) const
    {
        return invoke_const_subscript(container, index);
    }
};

template<class Container, class Index>
struct const_subscript_invoker<
    Container,
    Index,
    std::enable_if_t<
        !const_subscript_returns_reference<Container, Index>::value>>
{
    auto const&
    operator()(Container const& container, Index const& index) const
    {
        storage_ = invoke_const_subscript(container, index);
        return storage_;
    }

 private:
    mutable typename subscript_result_type<Container, Index>::type storage_;
};

template<class ContainerSignal, class IndexSignal, class Value>
std::enable_if_t<signal_can_write<ContainerSignal>::value>
write_subscript(
    ContainerSignal const& container,
    IndexSignal const& index,
    Value const& value)
{
    auto new_container = container.read();
    new_container[index.read()] = value;
    container.write(new_container);
}

template<class ContainerSignal, class IndexSignal, class Value>
std::enable_if_t<!signal_can_write<ContainerSignal>::value>
write_subscript(
    ContainerSignal const& container,
    IndexSignal const& index,
    Value const& value)
{
}

template<class ContainerSignal, class IndexSignal>
struct subscript_signal : signal<
                              subscript_signal<ContainerSignal, IndexSignal>,
                              typename subscript_result_type<
                                  typename ContainerSignal::value_type,
                                  typename IndexSignal::value_type>::type,
                              typename ContainerSignal::direction_tag>
{
    subscript_signal()
    {
    }
    subscript_signal(ContainerSignal array, IndexSignal index)
        : container_(array), index_(index)
    {
    }
    bool
    is_readable() const
    {
        return container_.is_readable() && index_.is_readable();
    }
    typename subscript_signal::value_type const&
    read() const
    {
        return invoker_(container_.read(), index_.read());
    }
    id_interface const&
    value_id() const
    {
        id_ = combine_ids(ref(container_.value_id()), ref(index_.value_id()));
        return id_;
    }
    bool
    is_writable() const
    {
        return container_.is_readable() && index_.is_readable()
               && container_.is_writable();
    }
    void
    write(typename subscript_signal::value_type const& x) const
    {
        write_subscript(container_, index_, x);
    }

 private:
    ContainerSignal container_;
    IndexSignal index_;
    mutable id_pair<alia::id_ref, alia::id_ref> id_;
    const_subscript_invoker<
        typename ContainerSignal::value_type,
        typename IndexSignal::value_type>
        invoker_;
};
template<class ContainerSignal, class IndexSignal>
subscript_signal<ContainerSignal, IndexSignal>
make_subscript_signal(
    ContainerSignal const& container, IndexSignal const& index)
{
    return subscript_signal<ContainerSignal, IndexSignal>(container, index);
}

template<class Derived, class Value, class Direction>
template<class Index>
auto signal_base<Derived, Value, Direction>::
operator[](Index const& index) const
{
    return make_subscript_signal(static_cast<Derived const&>(*this), index);
}

} // namespace alia


// This file defines the alia action interface, some common implementations of
// it, and some utilities for working with it.
//
// An action is essentially a response to an event that's dispatched by alia.
// When specifying a component element that can generate events, the application
// supplies the action that should be performed when the corresponding event is
// generated. Using this style allows event handling to be written in a safer
// and more reactive manner.
//
// Actions are very similar to signals in the way that they are used in an
// application. Like signals, they're typically created directly at the call
// site as function arguments and are only valid for the life of the function
// call.

namespace alia {

// untyped_action_interface defines functionality common to all actions,
// irrespective of the type of arguments that the action takes.
struct untyped_action_interface
{
    // Is this action ready to be performed?
    virtual bool
    is_ready() const = 0;
};

template<class... Args>
struct action_interface : untyped_action_interface
{
    // Perform this action.
    virtual void
    perform(Args... args) const = 0;
};

// is_action_type<T>::value yields a compile-time boolean indicating whether or
// not T is an alia action type.
template<class T>
struct is_action_type : std::is_base_of<untyped_action_interface, T>
{
};

// Is the given action ready?
template<class... Args>
bool
action_is_ready(action_interface<Args...> const& action)
{
    return action.is_ready();
}

// Perform an action.
template<class... Args>
void
perform_action(action_interface<Args...> const& action, Args... args)
{
    assert(action.is_ready());
    action.perform(args...);
}

// action_ref is a reference to an action that implements the action interface
// itself.
template<class... Args>
struct action_ref : action_interface<Args...>
{
    // Construct from a reference to another action.
    action_ref(action_interface<Args...> const& ref) : action_(&ref)
    {
    }
    // Construct from another action_ref. - This is meant to prevent unnecessary
    // layers of indirection.
    action_ref(action_ref<Args...> const& other) : action_(other.action_)
    {
    }

    bool
    is_ready() const
    {
        return action_->is_ready();
    }

    void
    perform(Args... args) const
    {
        action_->perform(args...);
    }

 private:
    action_interface<Args...> const* action_;
};

template<class... Args>
using action = action_ref<Args...>;

// comma operator
//
// Using the comma operator between two actions creates a combined action that
// performs the two actions in sequence.

template<class First, class Second, class... Args>
struct action_pair : First::action_interface
{
    action_pair()
    {
    }

    action_pair(First const& first, Second const& second)
        : first_(first), second_(second)
    {
    }

    bool
    is_ready() const
    {
        return first_.is_ready() && second_.is_ready();
    }

    void
    perform(Args... args) const
    {
        first_.perform(args...);
        second_.perform(args...);
    }

 private:
    First first_;
    Second second_;
};

template<
    class First,
    class Second,
    std::enable_if_t<
        is_action_type<First>::value && is_action_type<Second>::value,
        int> = 0>
auto
operator,(First const& first, Second const& second)
{
    return action_pair<First, Second>(first, second);
}

// operator <<=
//
// sink <<= source, where :sink and :source are both signals, creates an action
// that will set the value of :sink to the value held in :source. In order for
// the action to be considered ready, :source must be readable and :sink must be
// writable.

template<class Sink, class Source>
struct copy_action : action_interface<>
{
    copy_action(Sink const& sink, Source const& source)
        : sink_(sink), source_(source)
    {
    }

    bool
    is_ready() const
    {
        return source_.is_readable() && sink_.is_writable();
    }

    void
    perform() const
    {
        sink_.write(source_.read());
    }

 private:
    Sink sink_;
    Source source_;
};

template<
    class Sink,
    class Source,
    std::enable_if_t<
        is_writable_signal_type<Sink>::value
            && is_readable_signal_type<Source>::value,
        int> = 0>
auto
operator<<=(Sink const& sink, Source const& source)
{
    return copy_action<Sink, Source>(sink, source);
}

// For most compound assignment operators (e.g., +=), a += b, where :a and :b
// are signals, creates an action that sets :a equal to :a + :b.

#define ALIA_DEFINE_COMPOUND_ASSIGNMENT_OPERATOR(assignment_form, normal_form) \
    template<                                                                  \
        class A,                                                               \
        class B,                                                               \
        std::enable_if_t<                                                      \
            is_bidirectional_signal_type<A>::value                             \
                && is_readable_signal_type<B>::value,                          \
            int> = 0>                                                          \
    auto operator assignment_form(A const& a, B const& b)                      \
    {                                                                          \
        return a <<= (a normal_form b);                                        \
    }

ALIA_DEFINE_COMPOUND_ASSIGNMENT_OPERATOR(+=, +)
ALIA_DEFINE_COMPOUND_ASSIGNMENT_OPERATOR(-=, -)
ALIA_DEFINE_COMPOUND_ASSIGNMENT_OPERATOR(*=, *)
ALIA_DEFINE_COMPOUND_ASSIGNMENT_OPERATOR(/=, /)
ALIA_DEFINE_COMPOUND_ASSIGNMENT_OPERATOR(^=, ^)
ALIA_DEFINE_COMPOUND_ASSIGNMENT_OPERATOR(%=, %)
ALIA_DEFINE_COMPOUND_ASSIGNMENT_OPERATOR(&=, &)
ALIA_DEFINE_COMPOUND_ASSIGNMENT_OPERATOR(|=, |)

#undef ALIA_DEFINE_COMPOUND_ASSIGNMENT_OPERATOR

// The increment and decrement operators work similarly.

#define ALIA_DEFINE_BY_ONE_OPERATOR(assignment_form, normal_form)              \
    template<                                                                  \
        class A,                                                               \
        std::enable_if_t<is_bidirectional_signal_type<A>::value, int> = 0>     \
    auto operator assignment_form(A const& a)                                  \
    {                                                                          \
        return a <<= (a normal_form value(typename A::value_type(1)));         \
    }                                                                          \
    template<                                                                  \
        class A,                                                               \
        std::enable_if_t<is_bidirectional_signal_type<A>::value, int> = 0>     \
    auto operator assignment_form(A const& a, int)                             \
    {                                                                          \
        return a <<= (a normal_form value(typename A::value_type(1)));         \
    }

ALIA_DEFINE_BY_ONE_OPERATOR(++, +)
ALIA_DEFINE_BY_ONE_OPERATOR(--, -)

#undef ALIA_DEFINE_BY_ONE_OPERATOR

// make_toggle_action(flag), where :flag is a signal to a boolean, creates an
// action that will toggle the value of :flag between true and false.
//
// Note that this could also be used with other value types as long as the !
// operator provides a reasonable "toggle" function.
//
template<class Flag>
auto
make_toggle_action(Flag const& flag)
{
    return flag <<= !flag;
}

// make_push_back_action(collection, item), where both :collection and :item are
// signals, creates an action that will push the value of :item onto the back
// of :collection.

template<class Collection, class Item>
struct push_back_action : action_interface<>
{
    push_back_action(Collection const& collection, Item const& item)
        : collection_(collection), item_(item)
    {
    }

    bool
    is_ready() const
    {
        return collection_.is_readable() && collection_.is_writable()
               && item_.is_readable();
    }

    void
    perform() const
    {
        auto new_collection = collection_.read();
        new_collection.push_back(item_.read());
        collection_.write(new_collection);
    }

 private:
    Collection collection_;
    Item item_;
};

template<class Collection, class Item>
auto
make_push_back_action(Collection const& collection, Item const& item)
{
    return push_back_action<Collection, Item>(collection, item);
}

// lambda_action(is_ready, perform) creates an action whose behavior is defined
// by two function objects.
//
// :is_ready takes no parameters and simply returns true or false to show if the
// action is ready to be performed.
//
// :perform can take any number/type of arguments and this defines the signature
// of the action.

template<class Function>
struct call_operator_action_signature
{
};

template<class T, class R, class... Args>
struct call_operator_action_signature<R (T::*)(Args...) const>
{
    typedef action_interface<Args...> type;
};

template<class Lambda>
struct lambda_action_signature
    : call_operator_action_signature<decltype(&Lambda::operator())>
{
};

template<class IsReady, class Perform, class Interface>
struct lambda_action_object;

template<class IsReady, class Perform, class... Args>
struct lambda_action_object<IsReady, Perform, action_interface<Args...>>
    : action_interface<Args...>
{
    lambda_action_object(IsReady is_ready, Perform perform)
        : is_ready_(is_ready), perform_(perform)
    {
    }

    bool
    is_ready() const
    {
        return is_ready_();
    }

    void
    perform(Args... args) const
    {
        perform_(args...);
    }

 private:
    IsReady is_ready_;
    Perform perform_;
};

template<class IsReady, class Perform>
auto
lambda_action(IsReady is_ready, Perform perform)
{
    return lambda_action_object<
        IsReady,
        Perform,
        typename lambda_action_signature<Perform>::type>(is_ready, perform);
}

// This is just a clear and concise way of indicating that a lambda action is
// always ready.
inline bool
always_ready()
{
    return true;
}

} // namespace alia




namespace alia {

// The following are utilities that are used to implement the control flow
// macros. They shouldn't be used directly by applications.

struct if_block : noncopyable
{
    if_block(data_traversal& traversal, bool condition);

 private:
    scoped_data_block scoped_data_block_;
};

struct switch_block : noncopyable
{
    template<class Context>
    switch_block(Context& ctx)
    {
        nc_.begin(ctx);
    }
    template<class Id>
    void
    activate_case(Id id)
    {
        active_case_.end();
        active_case_.begin(nc_, make_id(id), manual_delete(true));
    }

 private:
    naming_context nc_;
    named_block active_case_;
};

struct loop_block : noncopyable
{
    loop_block(data_traversal& traversal);
    ~loop_block();
    data_block&
    block() const
    {
        return *block_;
    }
    data_traversal&
    traversal() const
    {
        return *traversal_;
    }
    void
    next();

 private:
    data_traversal* traversal_;
    data_block* block_;
};

// The following are macros used to annotate control flow.
// They are used exactly like their C equivalents, but all require an alia_end
// after the end of their scope.
// Also note that all come in two forms. One form ends in an underscore and
// takes the context as its first argument. The other form has no trailing
// underscore and assumes that the context is a variable named 'ctx'.

// condition_is_true(x), where x is a signal whose value is testable in a
// boolean context, returns true iff x is readable and its value is true.
template<class Signal>
std::enable_if_t<is_readable_signal_type<Signal>::value, bool>
condition_is_true(Signal const& x)
{
    return signal_is_readable(x) && (read_signal(x) ? true : false);
}

// condition_is_false(x), where x is a signal whose value is testable in a
// boolean context, returns true iff x is readable and its value is false.
template<class Signal>
std::enable_if_t<is_readable_signal_type<Signal>::value, bool>
condition_is_false(Signal const& x)
{
    return signal_is_readable(x) && (read_signal(x) ? false : true);
}

// condition_is_readable(x), where x is a readable signal type, calls
// signal_is_readable.
template<class Signal>
std::enable_if_t<is_readable_signal_type<Signal>::value, bool>
condition_is_readable(Signal const& x)
{
    return signal_is_readable(x);
}

// read_condition(x), where x is a readable signal type, calls read_signal.
template<class Signal>
std::enable_if_t<
    is_readable_signal_type<Signal>::value,
    typename Signal::value_type>
read_condition(Signal const& x)
{
    return read_signal(x);
}

// ALIA_STRICT_CONDITIONALS disables the definitions that allow non-signals to
// be used in if/else/switch macros.
#ifndef ALIA_STRICT_CONDITIONALS

// condition_is_true(x) evaluates x in a boolean context.
template<class T>
std::enable_if_t<!is_signal_type<T>::value, bool>
condition_is_true(T x)
{
    return x ? true : false;
}

// condition_is_false(x) evaluates x in a boolean context and inverts it.
template<class T>
std::enable_if_t<!is_signal_type<T>::value, bool>
condition_is_false(T x)
{
    return x ? false : true;
}

// condition_is_readable(x), where x is NOT a signal type, always returns true.
template<class T>
std::enable_if_t<!is_signal_type<T>::value, bool>
condition_is_readable(T const& x)
{
    return true;
}

// read_condition(x), where x is NOT a signal type, simply returns x.
template<class T>
std::enable_if_t<!is_signal_type<T>::value, T const&>
read_condition(T const& x)
{
    return x;
}

#endif

// if, else_if, else

#define ALIA_IF_(ctx, condition)                                               \
    {                                                                          \
        bool _alia_else_condition ALIA_UNUSED;                                 \
        {                                                                      \
            auto const& _alia_condition = (condition);                         \
            bool _alia_if_condition                                            \
                = ::alia::condition_is_true(_alia_condition);                  \
            _alia_else_condition                                               \
                = ::alia::condition_is_false(_alia_condition);                 \
            ::alia::if_block _alia_if_block(                                   \
                get_data_traversal(ctx), _alia_if_condition);                  \
            if (_alia_if_condition)                                            \
            {

#define ALIA_IF(condition) ALIA_IF_(ctx, condition)

#define ALIA_ELSE_IF_(ctx, condition)                                          \
    }                                                                          \
    }                                                                          \
    {                                                                          \
        auto const& _alia_condition = (condition);                             \
        bool _alia_else_if_condition                                           \
            = _alia_else_condition                                             \
              && ::alia::condition_is_true(_alia_condition);                   \
        _alia_else_condition = _alia_else_condition                            \
                               && ::alia::condition_is_false(_alia_condition); \
        ::alia::if_block _alia_if_block(                                       \
            get_data_traversal(ctx), _alia_else_if_condition);                 \
        if (_alia_else_if_condition)                                           \
        {

#define ALIA_ELSE_IF(condition) ALIA_ELSE_IF_(ctx, condition)

#define ALIA_ELSE_(ctx)                                                        \
    }                                                                          \
    }                                                                          \
    {                                                                          \
        ::alia::if_block _alia_if_block(                                       \
            get_data_traversal(ctx), _alia_else_condition);                    \
        if (_alia_else_condition)                                              \
        {

#define ALIA_ELSE ALIA_ELSE_(ctx)

#ifdef ALIA_LOWERCASE_MACROS
#define alia_if_(ctx, condition) ALIA_IF_(ctx, condition)
#define alia_if(condition) ALIA_IF(condition)
#define alia_else_if_(ctx, condition) ALIA_ELSE_IF_(ctx, condition)
#define alia_else_if(condition) ALIA_ELSE_IF(condition)
#define alia_else_(ctx) ALIA_ELSE(ctx)
#define alia_else ALIA_ELSE
#endif

// switch

#define ALIA_SWITCH_(ctx, x)                                                   \
    {                                                                          \
        ::alia::switch_block _alia_switch_block(ctx);                          \
        if (::alia::condition_is_readable(x))                                  \
        {                                                                      \
            switch (::alia::read_condition(x))                                 \
            {

#define ALIA_SWITCH(x) ALIA_SWITCH_(ctx, x)

#define ALIA_CONCATENATE_HELPER(a, b) a##b
#define ALIA_CONCATENATE(a, b) ALIA_CONCATENATE_HELPER(a, b)

#define ALIA_CASE_(ctx, c)                                                     \
    case c:                                                                    \
        _alia_switch_block.activate_case(c);                                   \
        goto ALIA_CONCATENATE(_alia_dummy_label_, __LINE__);                   \
        ALIA_CONCATENATE(_alia_dummy_label_, __LINE__)

#define ALIA_CASE(c) ALIA_CASE_(ctx, c)

#define ALIA_DEFAULT_(ctx)                                                     \
    default:                                                                   \
        _alia_switch_block.activate_case("_alia_default_case");                \
        goto ALIA_CONCATENATE(_alia_dummy_label_, __LINE__);                   \
        ALIA_CONCATENATE(_alia_dummy_label_, __LINE__)

#define ALIA_DEFAULT ALIA_DEFAULT_(ctx)

#ifdef ALIA_LOWERCASE_MACROS
#define alia_switch_(ctx, x) ALIA_SWITCH_(ctx, x)
#define alia_switch(x) ALIA_SWITCH(x)
#define alia_case_(ctx, c) ALIA_CASE(ctx, c)
#define alia_case(c) ALIA_CASE(c)
#define alia_default_(ctx) ALIA_DEFAULT_(ctx)
#define alia_default ALIA_DEFAULT
#endif

// for

#define ALIA_FOR_(ctx, x)                                                      \
    {                                                                          \
        {                                                                      \
            ::alia::loop_block _alia_looper(get_data_traversal(ctx));          \
            for (x)                                                            \
            {                                                                  \
                ::alia::scoped_data_block _alia_scope;                         \
                _alia_scope.begin(                                             \
                    _alia_looper.traversal(), _alia_looper.block());           \
                _alia_looper.next();

#define ALIA_FOR(x) ALIA_FOR_(ctx, x)

#ifdef ALIA_LOWERCASE_MACROS
#define alia_for_(ctx, x) ALIA_FOR_(ctx, x)
#define alia_for(x) ALIA_FOR(x)
#endif

// while

#define ALIA_WHILE_(ctx, x)                                                    \
    {                                                                          \
        {                                                                      \
            ::alia::loop_block _alia_looper(get_data_traversal(ctx));          \
            while (x)                                                          \
            {                                                                  \
                ::alia::scoped_data_block _alia_scope;                         \
                _alia_scope.begin(                                             \
                    _alia_looper.traversal(), _alia_looper.block());           \
                _alia_looper.next();

#define ALIA_WHILE(x) ALIA_WHILE_(ctx, x)

#ifdef ALIA_LOWERCASE_MACROS
#define alia_while_(ctx, x) ALIA_WHILE_(ctx, x)
#define alia_while(x) ALIA_WHILE(x)
#endif

// end

#define ALIA_END                                                               \
    }                                                                          \
    }                                                                          \
    }

#ifdef ALIA_LOWERCASE_MACROS
#define alia_end ALIA_END
#endif

// The following macros are substitutes for normal C++ control flow statements.
// Unlike alia_if and company, they do NOT track control flow. Instead, they
// convert your context variable to a dataless_context within the block.
// This means that any attempt to retrieve data within the block will result
// in an error (as it should).

#define ALIA_REMOVE_DATA_TRACKING(ctx)                                         \
    typename decltype(ctx)::storage_type _alia_storage;                        \
    auto _alia_ctx                                                             \
        = alia::remove_component<data_traversal_tag>(&_alia_storage, ctx);     \
    auto ctx = _alia_ctx;

#define ALIA_UNTRACKED_IF_(ctx, condition)                                     \
    if (alia::condition_is_true(condition))                                    \
    {                                                                          \
        ALIA_REMOVE_DATA_TRACKING(ctx)                                         \
        {                                                                      \
            {

#define ALIA_UNTRACKED_IF(condition) ALIA_UNTRACKED_IF_(ctx, condition)

#define ALIA_UNTRACKED_ELSE_IF_(ctx, condition)                                \
    }                                                                          \
    }                                                                          \
    }                                                                          \
    else if (alia::condition_is_true(condition))                               \
    {                                                                          \
        ALIA_REMOVE_DATA_TRACKING(ctx)                                         \
        {                                                                      \
            {

#define ALIA_UNTRACKED_ELSE_IF(condition)                                      \
    ALIA_UNTRACKED_ELSE_IF_(ctx, condition)

#define ALIA_UNTRACKED_ELSE_(ctx)                                              \
    }                                                                          \
    }                                                                          \
    }                                                                          \
    else                                                                       \
    {                                                                          \
        ALIA_REMOVE_DATA_TRACKING(ctx)                                         \
        {                                                                      \
            {

#define ALIA_UNTRACKED_ELSE ALIA_UNTRACKED_ELSE_(ctx)

#ifdef ALIA_LOWERCASE_MACROS

#define alia_untracked_if_(ctx, condition) ALIA_UNTRACKED_IF_(ctx, condition)
#define alia_untracked_if(condition) ALIA_UNTRACKED_IF(condition)
#define alia_untracked_else_if_(ctx, condition)                                \
    ALIA_UNTRACKED_ELSE_IF_(ctx, condition)
#define alia_untracked_else_if(condition) ALIA_UNTRACKED_ELSE_IF(condition)
#define alia_untracked_else_(ctx) ALIA_UNTRACKED_ELSE(ctx)
#define alia_untracked_else ALIA_UNTRACKED_ELSE

#endif

#define ALIA_UNTRACKED_SWITCH_(ctx, expression)                                \
    {                                                                          \
        {                                                                      \
            auto _alia_value = (expression);                                   \
            ALIA_REMOVE_DATA_TRACKING(ctx)                                     \
            switch (_alia_value)                                               \
            {

#define ALIA_UNTRACKED_SWITCH(expression)                                      \
    ALIA_UNTRACKED_SWITCH_(ctx, expression)

#ifdef ALIA_LOWERCASE_MACROS

#define alia_untracked_switch_(ctx, x) ALIA_UNTRACKED_SWITCH_(ctx, x)
#define alia_untracked_switch(x) ALIA_UNTRACKED_SWITCH(x)

#endif

} // namespace alia


// This file implements utilities for routing events through an alia content
// traversal function.
//
// In alia, the application defines the contents of the scene dynamically by
// iterating through the current objects in the scene and calling a
// library-provided function to specify the existence of each of them.
//
// This traversal function serves as a way to dispatch and handle events.
// However, in cases where the event only needs to go to a single object in the
// scene, the overhead of having the traversal function visit every other object
// can be problematic. The overhead can be reduced by having the traversal
// function skip over subregions of the scene when they're not relevant to the
// event.
//
// This file provides a system for defining a hierarchy of such subregions in
// the scene, identifying which subregion an event is targeted at, and culling
// out irrelevant subregions.

namespace alia {

struct system;
struct routing_region;

typedef std::shared_ptr<routing_region> routing_region_ptr;

struct routing_region
{
    routing_region_ptr parent;
};

struct event_routing_path
{
    routing_region* node;
    event_routing_path* rest;
};

struct event_traversal
{
    routing_region_ptr* active_region;
    bool targeted;
    event_routing_path* path_to_target;
    std::type_info const* event_type;
    void* event;
};

template<class Context>
routing_region_ptr
get_active_routing_region(Context ctx)
{
    event_traversal& traversal = get_event_traversal(ctx);
    return traversal.active_region ? *traversal.active_region
                                   : routing_region_ptr();
}

// Set up the event traversal so that it will route the control flow to the
// given target. (And also invoke the traversal.)
// :target can be null, in which case no (further) routing will be done.
void
route_event(system& sys, event_traversal& traversal, routing_region* target);

template<class Event>
void
dispatch_targeted_event(
    system& sys, Event& event, routing_region_ptr const& target)
{
    event_traversal traversal;
    traversal.active_region = 0;
    traversal.targeted = true;
    traversal.path_to_target = 0;
    traversal.event_type = &typeid(Event);
    traversal.event = &event;
    route_event(sys, traversal, target.get());
}

template<class Event>
void
dispatch_event(system& sys, Event& event)
{
    event_traversal traversal;
    traversal.active_region = 0;
    traversal.targeted = false;
    traversal.path_to_target = 0;
    traversal.event_type = &typeid(Event);
    traversal.event = &event;
    route_event(sys, traversal, 0);
}

struct scoped_routing_region
{
    scoped_routing_region() : traversal_(0)
    {
    }
    scoped_routing_region(context ctx)
    {
        begin(ctx);
    }
    ~scoped_routing_region()
    {
        end();
    }

    void
    begin(context ctx);

    void
    end();

    bool
    is_relevant() const
    {
        return is_relevant_;
    }

 private:
    event_traversal* traversal_;
    routing_region_ptr* parent_;
    bool is_relevant_;
};

bool
detect_event(context ctx, std::type_info const& type);

template<class Event>
bool
detect_event(context ctx, Event** event)
{
    event_traversal& traversal = get_event_traversal(ctx);
    if (*traversal.event_type == typeid(Event))
    {
        *event = reinterpret_cast<Event*>(traversal.event);
        return true;
    }
    return false;
}

template<class Event, class Context, class Handler>
void
handle_event(Context ctx, Handler const& handler)
{
    Event* e;
    ALIA_UNTRACKED_IF(detect_event(ctx, &e))
    {
        handler(ctx, *e);
    }
    ALIA_END
}

} // namespace alia




namespace alia {

// fake_readability(s), where :s is a signal, yields a wrapper for :s that
// pretends to have read capabilities. It will never actually be readable, but
// it will type-check as a readable signal.
template<class Wrapped>
struct readability_faker : signal<
                               readability_faker<Wrapped>,
                               typename Wrapped::value_type,
                               typename signal_direction_union<
                                   read_only_signal,
                                   typename Wrapped::direction_tag>::type>
{
    readability_faker(Wrapped wrapped) : wrapped_(wrapped)
    {
    }
    id_interface const&
    value_id() const
    {
        return no_id;
    }
    bool
    is_readable() const
    {
        return false;
    }
    // Since this is only faking readability, read() should never be called.
    // LCOV_EXCL_START
    typename Wrapped::value_type const&
    read() const
    {
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnull-dereference"
#endif
        return *(typename Wrapped::value_type const*) nullptr;
#ifdef __clang__
#pragma clang diagnostic pop
#endif
    }
    // LCOV_EXCL_STOP
    bool
    is_writable() const
    {
        return wrapped_.is_writable();
    }
    void
    write(typename Wrapped::value_type const& value) const
    {
        return wrapped_.write(value);
    }

 private:
    Wrapped wrapped_;
};
template<class Wrapped>
readability_faker<Wrapped>
fake_readability(Wrapped const& wrapped)
{
    return readability_faker<Wrapped>(wrapped);
}

// fake_writability(s), where :s is a signal, yields a wrapper for :s that
// pretends to have write capabilities. It will never actually be writable, but
// it will type-check as a writable signal.
template<class Wrapped>
struct writability_faker : signal<
                               writability_faker<Wrapped>,
                               typename Wrapped::value_type,
                               typename signal_direction_union<
                                   write_only_signal,
                                   typename Wrapped::direction_tag>::type>
{
    writability_faker(Wrapped wrapped) : wrapped_(wrapped)
    {
    }
    id_interface const&
    value_id() const
    {
        return wrapped_.value_id();
    }
    bool
    is_readable() const
    {
        return wrapped_.is_readable();
    }
    typename Wrapped::value_type const&
    read() const
    {
        return wrapped_.read();
    }
    bool
    is_writable() const
    {
        return false;
    }
    // Since this is only faking writability, write() should never be called.
    // LCOV_EXCL_START
    void
    write(typename Wrapped::value_type const& value) const
    {
    }
    // LCOV_EXCL_STOP

 private:
    Wrapped wrapped_;
};
template<class Wrapped>
writability_faker<Wrapped>
fake_writability(Wrapped const& wrapped)
{
    return writability_faker<Wrapped>(wrapped);
}

// signal_cast<Value>(x), where :x is a signal, yields a proxy for :x with
// the value type :Value. The proxy will apply static_casts to convert its
// own values to and from :x's value type.
template<class Wrapped, class To>
struct signal_caster : regular_signal<
                           signal_caster<Wrapped, To>,
                           To,
                           typename Wrapped::direction_tag>
{
    signal_caster(Wrapped wrapped) : wrapped_(wrapped)
    {
    }
    bool
    is_readable() const
    {
        return wrapped_.is_readable();
    }
    To const&
    read() const
    {
        return lazy_reader_.read(
            [&] { return static_cast<To>(wrapped_.read()); });
    }
    bool
    is_writable() const
    {
        return wrapped_.is_writable();
    }
    void
    write(To const& value) const
    {
        return wrapped_.write(static_cast<typename Wrapped::value_type>(value));
    }

 private:
    Wrapped wrapped_;
    lazy_reader<To> lazy_reader_;
};
template<class To, class Wrapped>
signal_caster<Wrapped, To>
signal_cast(Wrapped const& wrapped)
{
    return signal_caster<Wrapped, To>(wrapped);
}

// is_readable(x) yields a signal to a boolean which indicates whether or
// not x is readable. (The returned signal is always readable itself.)
template<class Wrapped>
struct readability_signal
    : regular_signal<readability_signal<Wrapped>, bool, read_only_signal>
{
    readability_signal(Wrapped const& wrapped) : wrapped_(wrapped)
    {
    }
    bool
    is_readable() const
    {
        return true;
    }
    bool const&
    read() const
    {
        value_ = wrapped_.is_readable();
        return value_;
    }

 private:
    Wrapped wrapped_;
    mutable bool value_;
};
template<class Wrapped>
auto
is_readable(Wrapped const& wrapped)
{
    return readability_signal<Wrapped>(wrapped);
}

// is_writable(x) yields a signal to a boolean which indicates whether or
// not x is writable. (The returned signal is always readable.)
template<class Wrapped>
struct writability_signal
    : regular_signal<writability_signal<Wrapped>, bool, read_only_signal>
{
    writability_signal(Wrapped const& wrapped) : wrapped_(wrapped)
    {
    }
    bool
    is_readable() const
    {
        return true;
    }
    bool const&
    read() const
    {
        value_ = wrapped_.is_writable();
        return value_;
    }

 private:
    Wrapped wrapped_;
    mutable bool value_;
};
template<class Wrapped>
auto
is_writable(Wrapped const& wrapped)
{
    return writability_signal<Wrapped>(wrapped);
}

// add_fallback(primary, fallback), where :primary and :fallback are both
// signals, yields another signal whose value is that of :primary if it's
// readable and that of :fallback otherwise.
// All writes go directly to :primary.
template<class Primary, class Fallback>
struct fallback_signal : signal<
                             fallback_signal<Primary, Fallback>,
                             typename Primary::value_type,
                             typename Primary::direction_tag>
{
    fallback_signal()
    {
    }
    fallback_signal(Primary primary, Fallback fallback)
        : primary_(primary), fallback_(fallback)
    {
    }
    bool
    is_readable() const
    {
        return primary_.is_readable() || fallback_.is_readable();
    }
    typename Primary::value_type const&
    read() const
    {
        return primary_.is_readable() ? primary_.read() : fallback_.read();
    }
    id_interface const&
    value_id() const
    {
        id_ = combine_ids(
            make_id(primary_.is_readable()),
            primary_.is_readable() ? ref(primary_.value_id())
                                   : ref(fallback_.value_id()));
        return id_;
    }
    bool
    is_writable() const
    {
        return primary_.is_writable();
    }
    void
    write(typename Primary::value_type const& value) const
    {
        primary_.write(value);
    }

 private:
    mutable id_pair<simple_id<bool>, id_ref> id_;
    Primary primary_;
    Fallback fallback_;
};
template<class Primary, class Fallback>
fallback_signal<Primary, Fallback>
add_fallback(Primary const& primary, Fallback const& fallback)
{
    return fallback_signal<Primary, Fallback>(primary, fallback);
}

// simplify_id(s), where :s is a signal, yields a wrapper for :s with the exact
// same read/write behavior but whose value ID is a simple_id (i.e., it is
// simply the value of the signal).
//
// The main utility of this is in cases where you have a signal carrying a
// small value with a complicated value ID (because it was picked from the
// signal of a larger data structure, for example). The more complicated ID
// might change superfluously.
//
template<class Wrapped>
struct simplified_id_wrapper : regular_signal<
                                   simplified_id_wrapper<Wrapped>,
                                   typename Wrapped::value_type,
                                   typename Wrapped::direction_tag>
{
    simplified_id_wrapper(Wrapped wrapped) : wrapped_(wrapped)
    {
    }
    bool
    is_readable() const
    {
        return wrapped_.is_readable();
    }
    typename Wrapped::value_type const&
    read() const
    {
        return wrapped_.read();
    }
    bool
    is_writable() const
    {
        return wrapped_.is_writable();
    }
    void
    write(typename Wrapped::value_type const& value) const
    {
        return wrapped_.write(value);
    }

 private:
    Wrapped wrapped_;
};
template<class Wrapped>
simplified_id_wrapper<Wrapped>
simplify_id(Wrapped const& wrapped)
{
    return simplified_id_wrapper<Wrapped>(wrapped);
}

} // namespace alia


namespace alia {

// is_map_like<Container>::value yields a compile-time boolean indicating
// whether or not Container behaves like a map for the purposes of alia
// iteration and indexing. (This is determined by checking whether or not
// Container has both a key_type and a mapped_type member.)
template<class T, class = void_t<>>
struct is_map_like : std::false_type
{
};
template<class T>
struct is_map_like<T, void_t<typename T::key_type, typename T::mapped_type>>
    : std::true_type
{
};

// is_vector_like<Container>::value yields a compile-time boolean indicating
// whether or not Container behaves like a vector for the purposes of alia
// iteration and indexing. (This is determined by checking whether or not
// Container can be subscripted with a size_t. This is sufficient because the
// main purpose is to distinguish vector-like containers from list-like ones.)
template<class Container, class = void_t<>>
struct is_vector_like : std::false_type
{
};
template<class Container>
struct is_vector_like<
    Container,
    void_t<decltype(std::declval<Container>()[size_t(0)])>> : std::true_type
{
};

template<class Item>
auto
get_alia_id(Item const& item)
{
    return no_id;
}

// for_each for map-like containers
template<
    class Context,
    class ContainerSignal,
    class Fn,
    std::enable_if_t<
        is_map_like<typename ContainerSignal::value_type>::value,
        int> = 0>
void
for_each(Context ctx, ContainerSignal const& container_signal, Fn const& fn)
{
    ALIA_IF(is_readable(container_signal))
    {
        naming_context nc(ctx);
        auto const& container = read_signal(container_signal);
        for (auto const& item : container)
        {
            named_block nb;
            auto iteration_id = get_alia_id(item.first);
            if (iteration_id != no_id)
                nb.begin(nc, iteration_id);
            else
                nb.begin(nc, make_id(&item));
            auto key = direct(item.first);
            auto value = container_signal[key];
            fn(ctx, key, value);
        }
    }
    ALIA_END
}

// for_each for vector-like containers
template<
    class Context,
    class ContainerSignal,
    class Fn,
    std::enable_if_t<
        !is_map_like<typename ContainerSignal::value_type>::value
            && is_vector_like<typename ContainerSignal::value_type>::value,
        int> = 0>
void
for_each(Context ctx, ContainerSignal const& container_signal, Fn const& fn)
{
    ALIA_IF(is_readable(container_signal))
    {
        naming_context nc(ctx);
        auto const& container = read_signal(container_signal);
        size_t const item_count = container.size();
        for (size_t index = 0; index != item_count; ++index)
        {
            named_block nb;
            auto iteration_id = get_alia_id(container[index]);
            if (iteration_id != no_id)
                nb.begin(nc, iteration_id);
            else
                nb.begin(nc, make_id(index));
            fn(ctx, container_signal[value(index)]);
        }
    }
    ALIA_END
}

// signal type for accessing items within a list
template<class ListSignal, class Item>
struct list_item_signal : signal<
                              list_item_signal<ListSignal, Item>,
                              Item,
                              typename ListSignal::direction_tag>
{
    list_item_signal(ListSignal const& list_signal, size_t index, Item* item)
        : list_signal_(list_signal), index_(index), item_(item)
    {
    }
    id_interface const&
    value_id() const
    {
        id_ = combine_ids(ref(list_signal_.value_id()), make_id(index_));
        return id_;
    }
    bool
    is_readable() const
    {
        return list_signal_.is_readable();
    }
    Item const&
    read() const
    {
        return *item_;
    }
    bool
    is_writable() const
    {
        return list_signal_.is_writable();
    }
    void
    write(Item const& value) const
    {
        *item_ = value;
    }

 private:
    ListSignal list_signal_;
    size_t index_;
    Item* item_;
    mutable id_pair<id_ref, simple_id<size_t>> id_;
};
template<class ListSignal, class Item>
list_item_signal<ListSignal, Item>
make_list_item_signal(ListSignal const& signal, size_t index, Item const* item)
{
    return list_item_signal<ListSignal, Item>(
        signal, index, const_cast<Item*>(item));
}

// for_each for list-like containers
template<
    class Context,
    class ContainerSignal,
    class Fn,
    std::enable_if_t<
        !is_map_like<typename ContainerSignal::value_type>::value
            && !is_vector_like<typename ContainerSignal::value_type>::value,
        int> = 0>
void
for_each(Context ctx, ContainerSignal const& container_signal, Fn const& fn)
{
    ALIA_IF(is_readable(container_signal))
    {
        naming_context nc(ctx);
        auto const& container = read_signal(container_signal);
        size_t index = 0;
        for (auto const& item : container)
        {
            named_block nb;
            auto iteration_id = get_alia_id(item);
            if (iteration_id != no_id)
                nb.begin(nc, iteration_id);
            else
                nb.begin(nc, make_id(&item));
            fn(ctx, make_list_item_signal(container_signal, index, &item));
            ++index;
        }
    }
    ALIA_END
}

} // namespace alia


#include <utility>
#include <vector>


namespace alia {

// This is the signals-based version of a functional transform (i.e., map). It
// takes a sequence container and a function mapping a single container item to
// another type. The mapping function is itself an alia traversal function, so
// its first argument is a context (and the second is the item to map, as a
// signal).
//
// The return value of transform() is a signal carrying a vector with the mapped
// items. This signal is readable when all items are successfully transformed
// (i.e., when the mapping function is returning a readable signal for every
// item).
//
// Note that this follows proper dataflow semantics in that the mapped values
// are allowed to change over time. Even after the result is readable (and all
// items are successfully transformed), the mapping function will continue to be
// invoked whenever necessary to allow events to be delivered and changes to
// propogate.
//

template<class MappedItem>
struct mapped_sequence_data
{
    captured_id input_id;
    std::vector<MappedItem> mapped_items;
    std::vector<captured_id> item_ids;
    counter_type output_version = 0;
};

template<class MappedItem>
struct mapped_sequence_signal : signal<
                                    mapped_sequence_signal<MappedItem>,
                                    std::vector<MappedItem>,
                                    read_only_signal>
{
    mapped_sequence_signal(
        mapped_sequence_data<MappedItem>& data, bool all_items_readable)
        : data_(&data), all_items_readable_(all_items_readable)
    {
    }
    id_interface const&
    value_id() const
    {
        id_ = make_id(data_->output_version);
        return id_;
    }
    bool
    is_readable() const
    {
        return all_items_readable_;
    }
    std::vector<MappedItem> const&
    read() const
    {
        return data_->mapped_items;
    }

 private:
    mapped_sequence_data<MappedItem>* data_;
    bool all_items_readable_;
    mutable simple_id<counter_type> id_;
};

template<class Context, class Container, class Function>
auto
transform(Context ctx, Container const& container, Function const& f)
{
    typedef typename decltype(f(
        ctx,
        std::declval<readable<typename Container::value_type::value_type>>()))::
        value_type mapped_value_type;

    mapped_sequence_data<mapped_value_type>* data;
    get_cached_data(ctx, &data);

    bool all_items_readable = false;

    ALIA_IF(is_readable(container))
    {
        size_t container_size = read_signal(container).size();

        if (!data->input_id.matches(container.value_id()))
        {
            data->mapped_items.resize(container_size);
            data->item_ids.resize(container_size);
            ++data->output_version;
            data->input_id.capture(container.value_id());
        }

        size_t readable_item_count = 0;
        auto captured_item = data->mapped_items.begin();
        auto captured_id = data->item_ids.begin();
        for_each(ctx, container, [&](context ctx, auto item) {
            auto mapped_item = f(ctx, item);
            if (signal_is_readable(mapped_item))
            {
                if (!captured_id->matches(mapped_item.value_id()))
                {
                    *captured_item = read_signal(mapped_item);
                    captured_id->capture(mapped_item.value_id());
                    ++data->output_version;
                }
                ++readable_item_count;
            }
            ++captured_item;
            ++captured_id;
        });
        assert(captured_item == data->mapped_items.end());
        assert(captured_id == data->item_ids.end());

        all_items_readable = (readable_item_count == container_size);
    }
    ALIA_END

    return mapped_sequence_signal<mapped_value_type>(*data, all_items_readable);
}

} // namespace alia



// This file defines utilities for constructing custom signals via lambda
// functions.

namespace alia {

// lambda_reader(is_readable, read) creates a read-only signal whose value is
// determined by calling :is_readable and :read.
template<class Value, class IsReadable, class Read>
struct lambda_reader_signal : regular_signal<
                                  lambda_reader_signal<Value, IsReadable, Read>,
                                  Value,
                                  read_only_signal>
{
    lambda_reader_signal(IsReadable is_readable, Read read)
        : is_readable_(is_readable), read_(read)
    {
    }
    bool
    is_readable() const
    {
        return is_readable_();
    }
    Value const&
    read() const
    {
        value_ = read_();
        return value_;
    }

 private:
    IsReadable is_readable_;
    Read read_;
    mutable decltype(read_()) value_;
};
template<class IsReadable, class Read>
auto
lambda_reader(IsReadable is_readable, Read read)
{
    return lambda_reader_signal<
        std::decay_t<decltype(read())>,
        IsReadable,
        Read>(is_readable, read);
}

// lambda_reader(is_readable, read, generate_id) creates a read-only signal
// whose value is determined by calling :is_readable and :read and whose ID is
// determined by calling :generate_id.
template<class Value, class IsReadable, class Read, class GenerateId>
struct lambda_reader_signal_with_id
    : signal<
          lambda_reader_signal_with_id<Value, IsReadable, Read, GenerateId>,
          Value,
          read_only_signal>
{
    lambda_reader_signal_with_id(
        IsReadable is_readable, Read read, GenerateId generate_id)
        : is_readable_(is_readable), read_(read), generate_id_(generate_id)
    {
    }
    id_interface const&
    value_id() const
    {
        id_ = generate_id_();
        return id_;
    }
    bool
    is_readable() const
    {
        return is_readable_();
    }
    Value const&
    read() const
    {
        value_ = read_();
        return value_;
    }

 private:
    IsReadable is_readable_;
    Read read_;
    mutable decltype(read_()) value_;
    GenerateId generate_id_;
    mutable decltype(generate_id_()) id_;
};
template<class IsReadable, class Read, class GenerateId>
auto
lambda_reader(IsReadable is_readable, Read read, GenerateId generate_id)
{
    return lambda_reader_signal_with_id<
        std::decay_t<decltype(read())>,
        IsReadable,
        Read,
        GenerateId>(is_readable, read, generate_id);
}

// lambda_bidirectional(is_readable, read, is_writable, write) creates a
// bidirectional signal whose value is read by calling :is_readable and :read
// and written by calling :is_writable and :write.
template<
    class Value,
    class IsReadable,
    class Read,
    class IsWritable,
    class Write>
struct lambda_bidirectional_signal : regular_signal<
                                         lambda_bidirectional_signal<
                                             Value,
                                             IsReadable,
                                             Read,
                                             IsWritable,
                                             Write>,
                                         Value,
                                         bidirectional_signal>
{
    lambda_bidirectional_signal(
        IsReadable is_readable, Read read, IsWritable is_writable, Write write)
        : is_readable_(is_readable),
          read_(read),
          is_writable_(is_writable),
          write_(write)
    {
    }
    bool
    is_readable() const
    {
        return is_readable_();
    }
    Value const&
    read() const
    {
        value_ = read_();
        return value_;
    }
    bool
    is_writable() const
    {
        return is_writable_();
    }
    void
    write(Value const& value) const
    {
        write_(value);
    }

 private:
    IsReadable is_readable_;
    Read read_;
    mutable decltype(read_()) value_;
    IsWritable is_writable_;
    Write write_;
};
template<class IsReadable, class Read, class IsWritable, class Write>
auto
lambda_bidirectional(
    IsReadable is_readable, Read read, IsWritable is_writable, Write write)
{
    return lambda_bidirectional_signal<
        std::decay_t<decltype(read())>,
        IsReadable,
        Read,
        IsWritable,
        Write>(is_readable, read, is_writable, write);
}

// lambda_bidirectional(is_readable, read, is_writable, write, generate_id)
// creates a bidirectional signal whose value is read by calling :is_readable
// and :read and written by calling :is_writable and :write. Its ID is
// determined by calling :generate_id.
template<
    class Value,
    class IsReadable,
    class Read,
    class IsWritable,
    class Write,
    class GenerateId>
struct lambda_bidirectional_signal_with_id
    : signal<
          lambda_bidirectional_signal_with_id<
              Value,
              IsReadable,
              Read,
              IsWritable,
              Write,
              GenerateId>,
          Value,
          bidirectional_signal>
{
    lambda_bidirectional_signal_with_id(
        IsReadable is_readable,
        Read read,
        IsWritable is_writable,
        Write write,
        GenerateId generate_id)
        : is_readable_(is_readable),
          read_(read),
          is_writable_(is_writable),
          write_(write),
          generate_id_(generate_id)
    {
    }
    id_interface const&
    value_id() const
    {
        id_ = generate_id_();
        return id_;
    }
    bool
    is_readable() const
    {
        return is_readable_();
    }
    Value const&
    read() const
    {
        value_ = read_();
        return value_;
    }
    bool
    is_writable() const
    {
        return is_writable_();
    }
    void
    write(Value const& value) const
    {
        write_(value);
    }

 private:
    IsReadable is_readable_;
    Read read_;
    mutable decltype(read_()) value_;
    IsWritable is_writable_;
    Write write_;
    GenerateId generate_id_;
    mutable decltype(generate_id_()) id_;
};
template<
    class IsReadable,
    class Read,
    class IsWritable,
    class Write,
    class GenerateId>
auto
lambda_bidirectional(
    IsReadable is_readable,
    Read read,
    IsWritable is_writable,
    Write write,
    GenerateId generate_id)
{
    return lambda_bidirectional_signal_with_id<
        std::decay_t<decltype(read())>,
        IsReadable,
        Read,
        IsWritable,
        Write,
        GenerateId>(is_readable, read, is_writable, write, generate_id);
}

// This is just a clear and concise way of indicating that a lambda signal is
// always readable.
inline bool
always_readable()
{
    return true;
}

// This is just a clear and concise way of indicating that a lambda signal is
// always writable.
inline bool
always_writable()
{
    return true;
}

} // namespace alia



#include <cmath>

// This file defines some numerical adaptors for signals.

namespace alia {

// scale(n, factor) creates a new signal that presents a scaled view of :n,
// where :n and :factor are both numeric signals.
template<class N, class Factor>
struct scaled_signal : regular_signal<
                           scaled_signal<N, Factor>,
                           typename N::value_type,
                           typename N::direction_tag>
{
    scaled_signal(N n, Factor scale_factor) : n_(n), scale_factor_(scale_factor)
    {
    }
    bool
    is_readable() const
    {
        return n_.is_readable() && scale_factor_.is_readable();
    }
    typename N::value_type const&
    read() const
    {
        return lazy_reader_.read(
            [&] { return n_.read() * scale_factor_.read(); });
    }
    bool
    is_writable() const
    {
        return n_.is_writable() && scale_factor_.is_readable();
    }
    void
    write(typename N::value_type const& value) const
    {
        n_.write(value / scale_factor_.read());
    }

 private:
    N n_;
    Factor scale_factor_;
    lazy_reader<typename N::value_type> lazy_reader_;
};
template<class N, class Factor>
scaled_signal<N, Factor>
scale(N const& n, Factor const& scale_factor)
{
    return scaled_signal<N, Factor>(n, scale_factor);
}

// offset(n, offset) presents an offset view of :n.
template<class N, class Offset>
struct offset_signal : regular_signal<
                           offset_signal<N, Offset>,
                           typename N::value_type,
                           typename N::direction_tag>
{
    offset_signal(N n, Offset offset) : n_(n), offset_(offset)
    {
    }
    bool
    is_readable() const
    {
        return n_.is_readable() && offset_.is_readable();
    }
    typename N::value_type const&
    read() const
    {
        return lazy_reader_.read([&] { return n_.read() + offset_.read(); });
    }
    bool
    is_writable() const
    {
        return n_.is_writable() && offset_.is_readable();
    }
    void
    write(typename N::value_type const& value) const
    {
        n_.write(value - offset_.read());
    }

 private:
    N n_;
    Offset offset_;
    lazy_reader<typename N::value_type> lazy_reader_;
};
template<class N, class Offset>
offset_signal<N, Offset>
offset(N const& n, Offset const& offset)
{
    return offset_signal<N, Offset>(n, offset);
}

// round_signal_writes(n, step) yields a wrapper which rounds any writes to
// :n so that values are always a multiple of :step.
template<class N, class Step>
struct rounding_signal_wrapper : regular_signal<
                                     rounding_signal_wrapper<N, Step>,
                                     typename N::value_type,
                                     typename N::direction_tag>
{
    rounding_signal_wrapper(N n, Step step) : n_(n), step_(step)
    {
    }
    bool
    is_readable() const
    {
        return n_.is_readable();
    }
    typename N::value_type const&
    read() const
    {
        return n_.read();
    }
    bool
    is_writable() const
    {
        return n_.is_writable() && step_.is_readable();
    }
    void
    write(typename N::value_type const& value) const
    {
        typename N::value_type step = step_.read();
        n_.write(std::floor(value / step + typename N::value_type(0.5)) * step);
    }

 private:
    N n_;
    Step step_;
};
template<class N, class Step>
rounding_signal_wrapper<N, Step>
round_signal_writes(N const& n, Step const& step)
{
    return rounding_signal_wrapper<N, Step>(n, step);
}

} // namespace alia



namespace alia {

// state_holder<Value> is designed to be stored persistently as actual
// application state. Signals for it will track changes in it and report its ID
// based on that.
template<class Value>
struct state_holder
{
    state_holder() : version_(0)
    {
    }

    explicit state_holder(Value value) : value_(std::move(value)), version_(1)
    {
    }

    bool
    is_initialized() const
    {
        return version_ != 0;
    }

    Value const&
    get() const
    {
        return value_;
    }

    unsigned
    version() const
    {
        return version_;
    }

    void
    set(Value value)
    {
        value_ = std::move(value);
        ++version_;
    }

    // If you REALLY need direct, non-const access to the underlying state,
    // you can use this. It returns a non-const reference to the value and
    // increments the version number.
    //
    // Note that you should be careful to use this atomically. In other words,
    // call this to get a reference, do your update, and then discard the
    // reference before anyone else observes the state. If you hold onto the
    // reference and continue making changes while other alia code is accessing
    // it, you'll cause them to go out-of-sync.
    //
    // Also note that if you call this on an uninitialized state, you're
    // expected to initialize it.
    //
    Value&
    nonconst_get()
    {
        ++version_;
        return value_;
    }

 private:
    Value value_;
    // version_ is incremented for each change in the value of the state.
    // If this is 0, the state is considered uninitialized.
    unsigned version_;
};

template<class Value>
struct state_signal : signal<state_signal<Value>, Value, bidirectional_signal>
{
    explicit state_signal(state_holder<Value>* s) : state_(s)
    {
    }

    bool
    is_readable() const
    {
        return state_->is_initialized();
    }

    Value const&
    read() const
    {
        return state_->get();
    }

    simple_id<unsigned> const&
    value_id() const
    {
        id_ = make_id(state_->version());
        return id_;
    }

    bool
    is_writable() const
    {
        return true;
    }

    void
    write(Value const& value) const
    {
        state_->set(value);
    }

 private:
    state_holder<Value>* state_;
    mutable simple_id<unsigned> id_;
};

template<class Value>
state_signal<Value>
make_state_signal(state_holder<Value>& state)
{
    return state_signal<Value>(&state);
}

// get_state(ctx, initial_value) returns a signal carrying some persistent local
// state whose initial value is determined by the signal :initial_value. The
// returned signal will not be readable until :initial_value is readable (or
// a value is explicitly written to the state signal).
template<class InitialValue>
auto
get_state(context ctx, InitialValue const& initial_value)
{
    state_holder<typename InitialValue::value_type>* state;
    get_data(ctx, &state);

    if (!state->is_initialized() && signal_is_readable(initial_value))
        state->set(read_signal(initial_value));

    return make_state_signal(*state);
}

} // namespace alia


namespace alia {
}



#include <cstdio>

namespace alia {

// The following implements a very minimal C++-friendly version of printf that
// works with signals.

template<class Value>
Value
make_printf_friendly(Value x)
{
    return x;
}

static inline char const*
make_printf_friendly(std::string const& x)
{
    return x.c_str();
}

template<class... Args>
std::string
invoke_snprintf(std::string const& format, Args const&... args)
{
    int length
        = std::snprintf(0, 0, format.c_str(), make_printf_friendly(args)...);
    if (length < 0)
        throw "printf format error";
    std::string s;
    if (length > 0)
    {
        s.resize(length);
        std::snprintf(
            &s[0], length + 1, format.c_str(), make_printf_friendly(args)...);
    }
    return s;
}

template<class... Args>
auto
printf(context ctx, readable<std::string> format, Args const&... args)
{
    return apply(ctx, ALIA_LAMBDIFY(invoke_snprintf), format, args...);
}

template<class... Args>
auto
printf(context ctx, char const* format, Args const&... args)
{
    return apply(ctx, ALIA_LAMBDIFY(invoke_snprintf), value(format), args...);
}

} // namespace alia

#ifdef ALIA_IMPLEMENTATION

namespace alia {

bool
is_refresh_pass(context ctx)
{
    return get_data_traversal(ctx).gc_enabled;
}

} // namespace alia
#include <typeinfo>

namespace alia {

inline bool
types_match(id_interface const& a, id_interface const& b)
{
    return typeid(a).name() == typeid(b).name() || typeid(a) == typeid(b);
}

bool
operator<(id_interface const& a, id_interface const& b)
{
    return typeid(a).before(typeid(b)) || (types_match(a, b) && a.less_than(b));
}

void
clone_into(id_interface*& storage, id_interface const* id)
{
    if (!id)
    {
        delete storage;
        storage = 0;
    }
    else if (storage && types_match(*storage, *id))
    {
        id->deep_copy(storage);
    }
    else
    {
        delete storage;
        storage = id->clone();
    }
}

void
clone_into(std::shared_ptr<id_interface>& storage, id_interface const* id)
{
    if (!id)
    {
        storage.reset();
    }
    else if (storage && types_match(*storage, *id))
    {
        id->deep_copy(&*storage);
    }
    else
    {
        storage.reset(id->clone());
    }
}

bool
operator==(captured_id const& a, captured_id const& b)
{
    return a.is_initialized() == b.is_initialized()
           && (!a.is_initialized() || a.get() == b.get());
}
bool
operator!=(captured_id const& a, captured_id const& b)
{
    return !(a == b);
}
bool
operator<(captured_id const& a, captured_id const& b)
{
    return b.is_initialized() && (!a.is_initialized() || a.get() < b.get());
}

} // namespace alia
#include <map>
#include <vector>

namespace alia {

struct named_block_node;

struct naming_map
{
    typedef std::map<
        id_interface const*,
        named_block_node*,
        id_interface_pointer_less_than_test>
        map_type;
    map_type blocks;
};

struct named_block_node : noncopyable
{
    named_block_node()
        : reference_count(0), active_count(0), manual_delete(false), map(0)
    {
    }

    // the actual data block
    data_block block;

    // the ID of the block
    captured_id id;

    // count of references to this block by data_blocks
    int reference_count;

    // count of active data blocks that are currently referencing this block
    int active_count;

    // If this is set to true, the block is also owned by its map and will
    // persist until it's manually deleted (or the map is destroyed).
    bool manual_delete;

    // the naming map that this block belongs to
    // Since the block may not be owned by the map that contains it, it needs
    // to know where that map is so it can remove itself if it's no longer
    // needed.
    naming_map* map;
};

// naming_maps are always created via a naming_map_node, which takes care of
// associating them with the data_graph.
struct naming_map_node : noncopyable
{
    naming_map_node() : next(0), prev(0)
    {
    }
    ~naming_map_node();

    naming_map map;

    // the graph that this node belongs to
    data_graph* graph;

    // doubly linked list pointers
    naming_map_node* next;
    naming_map_node* prev;
};
naming_map_node::~naming_map_node()
{
    // Remove the association between any named_blocks left in the map and
    // the map itself.
    for (naming_map::map_type::const_iterator i = map.blocks.begin();
         i != map.blocks.end();
         ++i)
    {
        named_block_node* node = i->second;
        if (node->reference_count == 0)
            delete node;
        else
            node->map = 0;
    }

    // Remove this node from graph's map list.
    if (next)
        next->prev = prev;
    if (prev)
        prev->next = next;
    else
        graph->map_list = next;
}

static void
deactivate(named_block_ref_node& ref);

// named_block_ref_nodes are stored as lists within data_blocks to hold
// references to the named_block_nodes that occur within that block.
// A named_block_ref_node provides ownership of the referenced node.
struct named_block_ref_node : noncopyable
{
    ~named_block_ref_node()
    {
        if (node)
        {
            deactivate(*this);

            --node->reference_count;
            if (!node->reference_count)
            {
                if (node->map)
                {
                    if (!node->manual_delete)
                    {
                        node->map->blocks.erase(&node->id.get());
                        delete node;
                    }
                    else
                        clear_cached_data(node->block);
                }
                else
                    delete node;
            }
        }
    }

    // referenced node
    named_block_node* node = nullptr;

    // is this reference contributing to the active count in the node?
    bool active = false;

    // next node in linked list
    named_block_ref_node* next = nullptr;
};

static void
activate(named_block_ref_node& ref)
{
    if (!ref.active)
    {
        ++ref.node->active_count;
        ref.active = true;
    }
}
static void
deactivate(named_block_ref_node& ref)
{
    if (ref.active)
    {
        --ref.node->active_count;
        if (ref.node->active_count == 0)
            clear_cached_data(ref.node->block);
        ref.active = false;
    }
}

static void
delete_named_block_ref_list(named_block_ref_node* head)
{
    while (head)
    {
        named_block_ref_node* next = head->next;
        delete head;
        head = next;
    }
}

// Clear all cached data stored in the subgraph referenced from the given node
// list.
static void
clear_cached_data(data_node* nodes)
{
    for (data_node* i = nodes; i; i = i->next)
    {
        // If this node is cached data, clear it.
        typed_data_node<cached_data_holder>* caching_node
            = dynamic_cast<typed_data_node<cached_data_holder>*>(i);
        if (caching_node)
        {
            cached_data_holder& holder = caching_node->value;
            delete holder.data;
            holder.data = 0;
        }

        // If this node is a data block, clear cached data from it.
        typed_data_node<data_block>* block
            = dynamic_cast<typed_data_node<data_block>*>(i);
        if (block)
            clear_cached_data(block->value);
    }
}

void
clear_cached_data(data_block& block)
{
    if (!block.cache_clear)
    {
        clear_cached_data(block.nodes);
        for (named_block_ref_node* i = block.named_blocks; i; i = i->next)
            deactivate(*i);
        block.cache_clear = true;
    }
}

data_block::~data_block()
{
    clear_data_block(*this);
}

void
clear_data_block(data_block& block)
{
    data_node* node = block.nodes;
    while (node)
    {
        data_node* next = node->next;
        delete node;
        node = next;
    }
    block.nodes = 0;

    delete_named_block_ref_list(block.named_blocks);
    block.named_blocks = 0;

    block.cache_clear = true;
}

void
scoped_data_block::begin(data_traversal& traversal, data_block& block)
{
    traversal_ = &traversal;

    old_active_block_ = traversal.active_block;
    old_predicted_named_block_ = traversal.predicted_named_block;
    old_used_named_blocks_ = traversal.used_named_blocks;
    old_named_block_next_ptr_ = traversal.named_block_next_ptr;
    old_next_data_ptr_ = traversal.next_data_ptr;

    traversal.active_block = &block;
    traversal.predicted_named_block = block.named_blocks;
    traversal.used_named_blocks = 0;
    traversal.named_block_next_ptr = &traversal.used_named_blocks;
    traversal.next_data_ptr = &block.nodes;

    block.cache_clear = false;
}
void
scoped_data_block::end()
{
    if (traversal_)
    {
        data_traversal& traversal = *traversal_;

        // If GC is enabled, record which named blocks were used and clear out
        // the unused ones.
        if (traversal.gc_enabled && !std::uncaught_exception())
        {
            traversal.active_block->named_blocks = traversal.used_named_blocks;
            delete_named_block_ref_list(traversal.predicted_named_block);
        }

        traversal.active_block = old_active_block_;
        traversal.predicted_named_block = old_predicted_named_block_;
        traversal.used_named_blocks = old_used_named_blocks_;
        traversal.named_block_next_ptr = old_named_block_next_ptr_;
        traversal.next_data_ptr = old_next_data_ptr_;

        traversal_ = 0;
    }
}

naming_map*
retrieve_naming_map(data_traversal& traversal)
{
    naming_map_node* map_node;
    if (get_data(traversal, &map_node))
    {
        data_graph& graph = *traversal.graph;
        map_node->graph = &graph;
        map_node->next = graph.map_list;
        if (graph.map_list)
            graph.map_list->prev = map_node;
        map_node->prev = 0;
        graph.map_list = map_node;
    }
    return &map_node->map;
}

void
naming_context::begin(data_traversal& traversal)
{
    traversal_ = &traversal;
    map_ = retrieve_naming_map(traversal);
}

static void
record_usage(data_traversal& traversal, named_block_ref_node* ref)
{
    *traversal.named_block_next_ptr = ref;
    traversal.named_block_next_ptr = &ref->next;
    ref->next = 0;
    activate(*ref);
}

static named_block_node*
find_named_block(
    data_traversal& traversal,
    naming_map& map,
    id_interface const& id,
    manual_delete manual)
{
    // If the sequence of data requests is the same as last pass (which it
    // generally is), then the block we're looking for is the predicted one.
    named_block_ref_node* predicted = traversal.predicted_named_block;
    if (predicted && predicted->node->id.get() == id
        && predicted->node->map == &map)
    {
        traversal.predicted_named_block = predicted->next;
        if (traversal.gc_enabled)
            record_usage(traversal, predicted);
        return predicted->node;
    }

    if (!traversal.gc_enabled)
        throw named_block_out_of_order();

    // Otherwise, look it up in the map.
    naming_map::map_type::const_iterator i = map.blocks.find(&id);

    // If it's not already in the map, create it and insert it.
    if (i == map.blocks.end())
    {
        named_block_node* new_node = new named_block_node;
        new_node->id.capture(id);
        new_node->map = &map;
        new_node->manual_delete = manual.value;
        i = map.blocks
                .insert(naming_map::map_type::value_type(
                    &new_node->id.get(), new_node))
                .first;
    }

    named_block_node* node = i->second;
    assert(node && node->map == &map);

    // Create a new reference node to record the node's usage within this
    // data_block.
    named_block_ref_node* ref = new named_block_ref_node;
    ref->node = node;
    ref->active = false;
    ++node->reference_count;
    record_usage(traversal, ref);

    return node;
}

void
named_block::begin(
    data_traversal& traversal,
    naming_map& map,
    id_interface const& id,
    manual_delete manual)
{
    scoped_data_block_.begin(
        traversal, find_named_block(traversal, map, id, manual)->block);
}
void
named_block::end()
{
    scoped_data_block_.end();
}

void
delete_named_block(data_graph& graph, id_interface const& id)
{
    for (naming_map_node* i = graph.map_list; i; i = i->next)
    {
        naming_map::map_type::const_iterator j = i->map.blocks.find(&id);
        if (j != i->map.blocks.end())
        {
            named_block_node* node = j->second;
            // If the reference count is nonzero, the block is still active,
            // so we don't want to delete it. We just want to clear the
            // manual_delete flag.
            if (node->reference_count != 0)
            {
                node->manual_delete = false;
            }
            else
            {
                i->map.blocks.erase(&id);
                node->map = 0;
                delete node;
            }
        }
    }
}

void
disable_gc(data_traversal& traversal)
{
    traversal.gc_enabled = false;
}

void
scoped_cache_clearing_disabler::begin(data_traversal& traversal)
{
    traversal_ = &traversal;
    old_cache_clearing_state_ = traversal.cache_clearing_enabled;
    traversal.cache_clearing_enabled = false;
}
void
scoped_cache_clearing_disabler::end()
{
    if (traversal_)
    {
        traversal_->cache_clearing_enabled = old_cache_clearing_state_;
        traversal_ = 0;
    }
}

void
scoped_data_traversal::begin(data_graph& graph, data_traversal& traversal)
{
    traversal.graph = &graph;
    traversal.gc_enabled = true;
    traversal.cache_clearing_enabled = true;
    root_block_.begin(traversal, graph.root_block);
    root_map_.begin(traversal);
}
void
scoped_data_traversal::end()
{
    root_block_.end();
    root_map_.end();
}

} // namespace alia

namespace alia {

void
scoped_routing_region::begin(context ctx)
{
    event_traversal& traversal = get_event_traversal(ctx);

    routing_region_ptr* region;
    if (get_data(ctx, &region))
        region->reset(new routing_region);

    if (traversal.active_region)
    {
        if ((*region)->parent != *traversal.active_region)
            (*region)->parent = *traversal.active_region;
    }
    else
        (*region)->parent.reset();

    parent_ = traversal.active_region;
    traversal.active_region = region;

    if (traversal.targeted)
    {
        if (traversal.path_to_target
            && traversal.path_to_target->node == region->get())
        {
            traversal.path_to_target = traversal.path_to_target->rest;
            is_relevant_ = true;
        }
        else
            is_relevant_ = false;
    }
    else
        is_relevant_ = true;

    traversal_ = &traversal;
}

void
scoped_routing_region::end()
{
    if (traversal_)
    {
        traversal_->active_region = parent_;
        traversal_ = 0;
    }
}

static void
invoke_controller(system& sys, event_traversal& events)
{
    data_traversal data;
    scoped_data_traversal sdt(sys.data, data);

    component_storage storage;
    add_component<data_traversal_tag>(storage, &data);
    add_component<event_traversal_tag>(storage, &events);

    context ctx(&storage);
    sys.controller(ctx);
}

void
route_event(system& sys, event_traversal& traversal, routing_region* target)
{
    // In order to construct the path to the target, we start at the target and
    // follow the 'parent' pointers until we reach the root.
    // We do this via recursion so that the path can be constructed entirely
    // on the stack.
    if (target)
    {
        event_routing_path path_node;
        path_node.rest = traversal.path_to_target;
        path_node.node = target;
        traversal.path_to_target = &path_node;
        route_event(sys, traversal, target->parent.get());
    }
    else
    {
        invoke_controller(sys, traversal);
    }
}

} // namespace alia

namespace alia {

if_block::if_block(data_traversal& traversal, bool condition)
{
    data_block* block;
    get_data(traversal, &block);
    if (condition)
    {
        scoped_data_block_.begin(traversal, *block);
    }
    else
    {
        if (traversal.cache_clearing_enabled)
            clear_cached_data(*block);
    }
}

loop_block::loop_block(data_traversal& traversal)
{
    traversal_ = &traversal;
    get_data(traversal, &block_);
}
loop_block::~loop_block()
{
    // The current block is the one we were expecting to use for the next
    // iteration, but since the destructor is being invoked, there won't be a
    // next iteration, which means we should clear out that block.
    if (!std::uncaught_exception())
        clear_data_block(*block_);
}
void
loop_block::next()
{
    get_data(*traversal_, &block_);
}

} // namespace alia
#endif
#endif
