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

// alia.hpp - (local) - generated 2020-06-08T18:20:59-04:00

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

    ~exception() noexcept(true)
    {
    }

    virtual char const*
    what() const noexcept(true)
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
#ifndef ALIA_STRICT_MACROS
#define alia_lambdify(f) ALIA_LAMBDIFY(f)
#endif

// function_view is the non-owning equivalent of std::function.
template<class Signature>
class function_view;
template<class Return, class... Args>
class function_view<Return(Args...)> final
{
 private:
    using signature_type = Return(void*, Args...);

    void* _ptr;
    Return (*_erased_fn)(void*, Args...);

 public:
    template<typename T>
    function_view(T&& x) noexcept : _ptr{(void*) std::addressof(x)}
    {
        _erased_fn = [](void* ptr, Args... xs) -> Return {
            return (*reinterpret_cast<std::add_pointer_t<T>>(ptr))(
                std::forward<Args>(xs)...);
        };
    }

    decltype(auto)
    operator()(Args... xs) const
        noexcept(noexcept(_erased_fn(_ptr, std::forward<Args>(xs)...)))
    {
        return _erased_fn(_ptr, std::forward<Args>(xs)...);
    }
};

} // namespace alia


#include <functional>
#include <sstream>

// This file implements the concept of IDs in alia.

namespace alia {

// id_interface defines the interface required of all ID types.
struct id_interface
{
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
// it. The type Value must be copyable and comparable for equality and ordering
// (i.e., supply == and < operators).
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

// null_id can be used when you have nothing to identify.
struct null_id_type
{
};
static simple_id<null_id_type*> const null_id(nullptr);

// unit_id can be used when there is only possible identify.
struct unit_id_type
{
};
static simple_id<unit_id_type*> const unit_id(nullptr);

} // namespace alia



#include <typeindex>
#include <unordered_map>




// This file provides the underlying type mechanics that allow for defining the
// context of an application as an arbitrary collection of data from disparate
// sources (the alia core, a UI library adaptor, services that the application
// is dependent on, higher-level parts of the application, etc.). This set of
// data obviously varies across applications (and even across modules within a
// complex application). The goal here is to allow applications to freely mix
// together data/objects from multiple sources (which may not know about one
// another).
//
// Some additional design considerations follow.
//
// 1. An application should be able to easily define its own data types and mix
//    those into its context. This includes application-level state. If there is
//    state that is essentially global to the application (e.g., the active
//    user), application code should be able to retrieve this from the
//    application context. Similarly, a component of the application should be
//    able to extend the application's context with state that is specific to
//    that component (but ubiquitous within it).
//
// 2. Functions that take contexts as arguments should be able to define the set
//    of context elements that they require as part of the type signature of the
//    context. (Context elements would be identified by compile-time type tags.)
//    Any caller whose context includes a superset of those tags should be able
//    to call the function with an implicit conversion of the context parameter.
//    This should all be possible without needing to define functions as
//    templates (otherwise alia-based applications would end up being entirely
//    header-based) and with minimal (ideally zero) runtime overhead in
//    converting the caller's context to the type expected by the function.
//
// 3. Retrieving frames/capabilities from a context should require minimal
//    (ideally zero) runtime overhead.
//
// 4. Static type checking of context conversions/retrievals should be possible
//    but optional. Developers should not have to pay these compile-time costs
//    unless it's desired. It should be possible to use a mixed workflow where
//    these checks are replaced by runtime checks for iterative development but
//    enabled for CI/release builds.
//
// In order to satisfy #4, this file looks for a #define called
// ALIA_DYNAMIC_CONTEXT_CHECKS. If this is set, code related to statically
// checking context contents is omitted and dynamic checks are substituted where
// appropriate. Note that when ALIA_DYNAMIC_CONTEXT_CHECKS is NOT set,
// ALIA_STATIC_CONTEXT_CHECKS is set and static checks are included.
//
// The statically typed structural_collection object is a simple wrapper around
// the dynamically typed storage object. It adds a compile-time type list
// indicating what's actually supposed to be in the collection. This allows
// collections to be converted to other collection types without any run-time
// overhead. This does imply some run-time overhead for retrieving data from the
// collection, but that can be mitigated by providing zero-cost retrieval for
// select (core) data. This also implies that the collection object must be
// passed by value (or const& - though there's no real point in that) whereas
// passing by reference would be more obvious, but that seems unavoidable given
// the requirements.

#ifndef ALIA_DYNAMIC_CONTEXT_CHECKS
#define ALIA_STATIC_CONTEXT_CHECKS
#endif

namespace alia {

namespace impl {

#define ALIA_DEFINE_TAGGED_TYPE(tag, data)                                     \
    struct tag                                                                 \
    {                                                                          \
        typedef data data_type;                                                \
    };

template<class Tags, class Storage>
struct structural_collection;

#ifdef ALIA_STATIC_CONTEXT_CHECKS

// tag_list<Tags...> defines a simple compile-time list of tags. This is held by
// a structural_collection to provide compile-time tracking of its contents.
template<class... Tags>
struct tag_list
{
};

// list_contains_tag<List,Tag>::value yields a compile-time boolean indicating
// whether or not the tag_list :List contains :Tag.
template<class List, class Tag>
struct list_contains_tag
{
};
// base case - The list is empty, so the tag isn't in there.
template<class Tag>
struct list_contains_tag<tag_list<>, Tag> : std::false_type
{
};
// case where tag matches
template<class Tag, class... Rest>
struct list_contains_tag<tag_list<Tag, Rest...>, Tag> : std::true_type
{
};
// non-matching (recursive) case
template<class Tag, class OtherTag, class... Rest>
struct list_contains_tag<tag_list<OtherTag, Rest...>, Tag>
    : list_contains_tag<tag_list<Rest...>, Tag>
{
};

// structural_collection_contains_tag<Collection,Tag>::value yields a
// compile-time boolean indicating whether or not :Collection contains an item
// with the tag :Tag.
template<class Collection, class Tag>
struct structural_collection_contains_tag
{
};
template<class Tags, class Storage, class Tag>
struct structural_collection_contains_tag<
    structural_collection<Tags, Storage>,
    Tag> : list_contains_tag<Tags, Tag>
{
};

// add_tag_to_list<List,Tag>::type yields the list that results from adding :Tag
// to the head of :List.
//
// Note that this doesn't perform any checks for duplicates.
//
template<class List, class Tag>
struct add_tag_to_list
{
};
template<class Tag, class... Tags>
struct add_tag_to_list<tag_list<Tags...>, Tag>
{
    typedef tag_list<Tag, Tags...> type;
};

// remove_tag_from_list<List,Tag>::type yields the list that results from
// removing the tag matching :Tag from :List.
//
// Note that removing a tag that's not actually in the list is not considered an
// error.
//
template<class List, class Tag>
struct remove_tag_from_list
{
};
// base case (list is empty)
template<class Tag>
struct remove_tag_from_list<tag_list<>, Tag>
{
    typedef tag_list<> type;
};
// case where an item matches
template<class Tag, class... Rest>
struct remove_tag_from_list<tag_list<Tag, Rest...>, Tag>
    : remove_tag_from_list<tag_list<Rest...>, Tag>
{
};
// non-matching case
template<class Tag, class OtherTag, class... Rest>
struct remove_tag_from_list<tag_list<OtherTag, Rest...>, Tag>
    : add_tag_to_list<
          typename remove_tag_from_list<tag_list<Rest...>, Tag>::type,
          OtherTag>
{
};

// collection_contains_all_tags<Collection,Tags...>::value yields a compile-time
// boolean indicating whether or not :Collection contains all tags in :Tags.
template<class Collection, class... Tags>
struct collection_contains_all_tags
{
};
// base case - The list of tags to search for is empty, so this is trivially
// true.
template<class Collection>
struct collection_contains_all_tags<Collection> : std::true_type
{
};
// recursive case
template<class Collection, class Tag, class... Rest>
struct collection_contains_all_tags<Collection, Tag, Rest...>
    : std::conditional_t<
          structural_collection_contains_tag<Collection, Tag>::value,
          collection_contains_all_tags<Collection, Rest...>,
          std::false_type>
{
};

// merge_tag_lists<A,B>::type yields a list of tags that includes all tags from
// :A and :B (with no duplicates).
template<class A, class B>
struct merge_tag_lists
{
};
// base case (:A is empty)
template<class B>
struct merge_tag_lists<tag_list<>, B>
{
    typedef B type;
};
// recursive case
template<class B, class AHead, class... ARest>
struct merge_tag_lists<tag_list<AHead, ARest...>, B>
    : add_tag_to_list<
          // Ensure that :AHead isn't duplicated. (This may be a noop.)
          typename remove_tag_from_list<
              typename merge_tag_lists<tag_list<ARest...>, B>::type,
              AHead>::type,
          AHead>
{
};

// structural_collection_is_convertible<From,To>::value yields a
// compile-time boolean indicating whether or not the type :From can be
// converted to the type :To (both must be structural_collections).
// The requirements for this are that a) the storage types are the same and b)
// the tags of :From are a superset of those of :To.
template<class From, class To>
struct structural_collection_is_convertible
{
};
// case where storage types differ
template<class FromTags, class FromStorage, class ToTags, class ToStorage>
struct structural_collection_is_convertible<
    structural_collection<FromTags, FromStorage>,
    structural_collection<ToTags, ToStorage>> : std::false_type
{
};
// case where storage types are the same, so tags must be checked
template<class Storage, class FromTags, class... ToTags>
struct structural_collection_is_convertible<
    structural_collection<FromTags, Storage>,
    structural_collection<tag_list<ToTags...>, Storage>>
    : collection_contains_all_tags<
          structural_collection<FromTags, Storage>,
          ToTags...>
{
};

template<class Tags, class Storage>
struct structural_collection
{
    typedef Tags tags;
    typedef Storage storage_type;

    structural_collection(Storage* storage) : storage(storage)
    {
    }

    // copy constructor (from convertible collections)
    template<class Other>
    structural_collection(
        Other other,
        std::enable_if_t<structural_collection_is_convertible<
            Other,
            structural_collection>::value>* = 0)
        : storage(other.storage)
    {
    }

    // assignment operator (from convertible collections)
    template<class Other>
    std::enable_if_t<
        structural_collection_is_convertible<Other, structural_collection>::
            value,
        structural_collection&>
    operator=(Other other)
    {
        storage = other.storage;
        return *this;
    }

    Storage* storage;
};

// empty_structural_collection<Storage> yields a structural collection with
// no data and :Storage as its storage type.
template<class Storage>
using empty_structural_collection = structural_collection<tag_list<>, Storage>;

// add_tagged_data_type<Collection,Tag>::type gives the type that results
// from extending :Collection with the data type defined by :Tag.
template<class Collection, class Tag>
struct add_tagged_data_type
{
};
template<class Tag, class Storage, class... Tags>
struct add_tagged_data_type<
    structural_collection<tag_list<Tags...>, Storage>,
    Tag>
{
    static_assert(
        !list_contains_tag<tag_list<Tags...>, Tag>::value,
        "duplicate context tag");
    typedef structural_collection<tag_list<Tag, Tags...>, Storage> type;
};
template<class Collection, class Tag>
using add_tagged_data_type_t =
    typename add_tagged_data_type<Collection, Tag>::type;

// add_tagged_data_types<Collection,Tag...>::type gives the type that results
// from extending :Collection with the data types defined by the given list of
// tags.
template<class Collection, class... Tag>
struct add_tagged_data_types
{
};
template<class Collection>
struct add_tagged_data_types<Collection>
{
    typedef Collection type;
};
template<class Collection, class Tag, class... Rest>
struct add_tagged_data_types<Collection, Tag, Rest...>
    : add_tagged_data_types<
          typename add_tagged_data_type<Collection, Tag>::type,
          Rest...>
{
};
template<class Collection, class... Tag>
using add_tagged_data_types_t =
    typename add_tagged_data_types<Collection, Tag...>::type;

// remove_tagged_data_type<Collection,Tag>::type yields the type that results
// from removing the data type associated with :Tag from :Collection.
template<class Collection, class Tag>
struct remove_tagged_data_type
{
};
template<class Tag, class Storage, class Tags>
struct remove_tagged_data_type<structural_collection<Tags, Storage>, Tag>
{
    // Note that it's considered OK to remove a tag that's not actually in the
    // collection.
    typedef structural_collection<
        typename remove_tag_from_list<Tags, Tag>::type,
        Storage>
        type;
};
template<class Collection, class Tag>
using remove_tagged_data_type_t =
    typename remove_tagged_data_type<Collection, Tag>::type;

// merge_structural_collections<A,B>::type yields a structural collection type
// that contains all the tags from :A and :B (but no duplicates). Note that the
// resulting collection inherits the storage type of :A.
template<class A, class B>
struct merge_structural_collections
{
    typedef structural_collection<
        typename merge_tag_lists<typename A::tags, typename B::tags>::type,
        typename A::storage_type>
        type;
};
template<class A, class B>
using merge_structural_collections_t =
    typename merge_structural_collections<A, B>::type;

#else

struct dynamic_tag_list
{
};

template<class Tags, class Storage>
struct structural_collection
{
    typedef Tags tags;
    typedef Storage storage_type;

    structural_collection(Storage* storage) : storage(storage)
    {
    }

    Storage* storage;
};

// structural_collection_is_convertible<From,To>::value yields a
// compile-time boolean indicating whether or not the type :From can be
// converted to the type :To (both must be structural_collections).
// Since this is the dynamic version, the requirements for this are simply that
// the storage types are the same.
template<class From, class To>
struct structural_collection_is_convertible
{
};
// case where storage types differ
template<class FromTags, class FromStorage, class ToTags, class ToStorage>
struct structural_collection_is_convertible<
    structural_collection<FromTags, FromStorage>,
    structural_collection<ToTags, ToStorage>> : std::false_type
{
};
// case where storage types are the same
template<class Storage, class FromTags, class ToTags>
struct structural_collection_is_convertible<
    structural_collection<FromTags, Storage>,
    structural_collection<ToTags, Storage>> : std::true_type
{
};

// empty_structural_collection<Storage> yields a structural collection with no
// item and :Storage as its storage type.
template<class Storage>
using empty_structural_collection
    = structural_collection<dynamic_tag_list, Storage>;

// add_tagged_data_type<Collection,Tag>::type gives the type that results from
// extending :Collection with the data type defined by :Tag and :Data.
template<class Collection, class Tag>
struct add_tagged_data_type
{
    typedef Collection type;
};
template<class Collection, class Tag>
using add_tagged_data_type_t =
    typename add_tagged_data_type<Collection, Tag>::type;

// add_tagged_data_types<Collection,Tag...>::type gives the type that results
// from extending :Collection with the data types defined by the given list of
// tags.
template<class Collection, class... Tag>
struct add_tagged_data_types
{
    typedef Collection type;
};
template<class Collection, class... Tag>
using add_tagged_data_types_t =
    typename add_tagged_data_types<Collection, Tag...>::type;

// remove_tagged_data_type<Collection,Tag>::type yields the type that results
// from removing the data type associated with :Tag from :Collection.
template<class Collection, class Tag>
struct remove_tagged_data_type
{
    typedef Collection type;
};
template<class Collection, class Tag>
using remove_tagged_data_type_t =
    typename remove_tagged_data_type<Collection, Tag>::type;

// merge_structural_collections<A,B>::type yields a structural collection type
// that contains all the tags from :A and :B (but no duplicates). Note that the
// resulting tags collection inherits the storage type of :A.
template<class A, class B>
struct merge_structural_collections
{
    typedef A type;
};
template<class A, class B>
using merge_structural_collections_t =
    typename merge_structural_collections<A, B>::type;

#endif

// Make an empty structural collection for the given storage object.
template<class Storage>
empty_structural_collection<Storage>
make_empty_structural_collection(Storage* storage)
{
    return empty_structural_collection<Storage>(storage);
}

// Extend a collection by adding a new data object.
// :Tag is the tag of the new data.
// :data is the data.
//
// Note that although this returns a new collection (with the correct type), the
// new collection shares the storage of the original, so this should be used
// with caution.
//
template<class Tag, class Collection, class Data>
add_tagged_data_type_t<Collection, Tag>
add_tagged_data(Collection collection, Data&& data)
{
    auto* storage = collection.storage;
    // Add the new data to the storage object.
    storage->template add<Tag>(std::forward<Data&&>(data));
    // Create a collection with the proper type to reference the storage.
    return add_tagged_data_type_t<Collection, Tag>(storage);
}

// Remove an item from a collection.
// :Tag is the tag of the item.
//
// As with add_tagged_data(), although this returns a new collection for typing
// purposes, the new collection shares the storage of the original, so use with
// caution.
//
template<class Tag, class Collection>
remove_tagged_data_type_t<Collection, Tag>
remove_tagged_data(Collection collection)
{
    typename Collection::storage_type* storage = collection.storage;
    // We only actually have to remove the item if we're using dynamic context
    // checking. With static checking, it doesn't matter if the runtime storage
    // includes an extra item. Static checks will prevent its use.
#ifdef ALIA_DYNAMIC_CONTEXT_CHECKS
    // Remove the item from the storage object.
    storage->template remove<Tag>();
#endif
    return remove_tagged_data_type_t<Collection, Tag>(storage);
}

// Remove an item from a collection.
//
// With this version, you supply a new storage object, and the function uses it
// if needed to ensure that the original collection's storage is left untouched.
//
#ifdef ALIA_STATIC_CONTEXT_CHECKS
template<class Tag, class Collection, class Storage>
remove_tagged_data_type_t<Collection, Tag>
remove_tagged_data(Collection collection, Storage*)
{
    return remove_tagged_data_type_t<Collection, Tag>(collection.storage);
}
#else
template<class Tag, class Collection, class Storage>
remove_tagged_data_type_t<Collection, Tag>
remove_tagged_data(Collection collection, Storage* new_storage)
{
    *new_storage = *collection.storage;
    new_storage->template remove<Tag>();
    return remove_tagged_data_type_t<Collection, Tag>(new_storage);
}
#endif

// Determine if a tag is in a collection.
#ifdef ALIA_STATIC_CONTEXT_CHECKS
template<class Tag, class Collection>
bool has_tagged_data(Collection)
{
    return structural_collection_contains_tag<Collection, Tag>::value;
}
#else
template<class Tag, class Collection>
bool
has_tagged_data(Collection collection)
{
    return collection.storage->template has<Tag>();
}
#endif

#ifdef ALIA_DYNAMIC_CONTEXT_CHECKS

// When using dynamic context checking, this error is thrown when trying to
// retrieve a tag that's not actually present in a collection.
template<class Tag>
struct tagged_data_not_found : exception
{
    tagged_data_not_found()
        : exception(
            std::string("tag not found in context:\n") + typeid(Tag).name())
    {
    }
};

#endif

// tagged_data_caster should be specialized so that it properly casts from
// stored data to the expected types.

template<class Stored, class Expected>
struct tagged_data_caster
{
};

// If we're stored what's expected, then the cast is trivial.
template<class T>
struct tagged_data_caster<T, T>
{
    static T
    apply(T stored)
    {
        return stored;
    }
};
template<class T>
struct tagged_data_caster<T&, T>
{
    static T&
    apply(T& stored)
    {
        return stored;
    }
};

// Get a reference to the data associated with a tag in a collection. If static
// checking is enabled, this generates a compile-time error if :Tag isn't
// contained in :collection.
template<class Tag, class Collection>
decltype(auto)
get_tagged_data(Collection collection)
{
#ifdef ALIA_STATIC_CONTEXT_CHECKS
    static_assert(
        structural_collection_contains_tag<Collection, Tag>::value,
        "tag not found in context");
#else
    if (!has_tagged_data<Tag>(collection))
        throw tagged_data_not_found<Tag>();
#endif
    return tagged_data_caster<
        decltype(collection.storage->template get<Tag>()),
        typename Tag::data_type>::apply(collection.storage
                                            ->template get<Tag>());
}

} // namespace impl

} // namespace alia


namespace alia {

namespace impl {

// generic_tagged_storage is one possible implementation of the underlying
// container for storing the actual data associated with a tag.
// :Data is the type used to the store data.
template<class Data>
struct generic_tagged_storage
{
    std::unordered_map<std::type_index, Data> objects;

    template<class Tag>
    bool
    has() const
    {
        return this->objects.find(std::type_index(typeid(Tag)))
               != this->objects.end();
    }

    template<class Tag, class ObjectData>
    void
    add(ObjectData&& data)
    {
        this->objects[std::type_index(typeid(Tag))]
            = std::forward<ObjectData&&>(data);
    }

    template<class Tag>
    void
    remove()
    {
        this->objects.erase(std::type_index(typeid(Tag)));
    }

    template<class Tag>
    Data&
    get()
    {
        return this->objects.at(std::type_index(typeid(Tag)));
    }
};

// any_ref is a simple way to store references to any type in a
// generic_tagged_storage object.
struct any_ref
{
    any_ref()
    {
    }

    template<class T>
    any_ref(std::reference_wrapper<T> ref) : ptr(&ref.get())
    {
    }

    void* ptr;
};

template<class T>
struct tagged_data_caster<any_ref&, T&>
{
    static T&
    apply(any_ref stored)
    {
        return *reinterpret_cast<T*>(stored.ptr);
    }
};
template<class T>
struct tagged_data_caster<any_ref, T&>
{
    static T&
    apply(any_ref stored)
    {
        return *reinterpret_cast<T*>(stored.ptr);
    }
};

// The following provides a small framework for defining more specialized
// storage structures with direct storage of frequently used objects. See
// context.hpp for an example of how it's used.

template<class Storage, class Tag>
struct tagged_data_accessor
{
    static bool
    has(Storage const& storage)
    {
        return storage.generic.template has<Tag>();
    }
    static void
    add(Storage& storage, any_ref data)
    {
        storage.generic.template add<Tag>(data);
    }
    static void
    remove(Storage& storage)
    {
        storage.generic.template remove<Tag>();
    }
    static any_ref
    get(Storage& storage)
    {
        return storage.generic.template get<Tag>();
    }
};

#define ALIA_IMPLEMENT_STORAGE_OBJECT_ACCESSORS(Storage)                       \
    template<class Tag>                                                        \
    bool has() const                                                           \
    {                                                                          \
        return impl::tagged_data_accessor<Storage, Tag>::has(*this);           \
    }                                                                          \
                                                                               \
    template<class Tag, class Data>                                            \
    void add(Data&& data)                                                      \
    {                                                                          \
        impl::tagged_data_accessor<Storage, Tag>::add(                         \
            *this, std::forward<Data&&>(data));                                \
    }                                                                          \
                                                                               \
    template<class Tag>                                                        \
    void remove()                                                              \
    {                                                                          \
        impl::tagged_data_accessor<Storage, Tag>::remove(*this);               \
    }                                                                          \
                                                                               \
    template<class Tag>                                                        \
    decltype(auto) get()                                                       \
    {                                                                          \
        return impl::tagged_data_accessor<Storage, Tag>::get(*this);           \
    }

#define ALIA_ADD_DIRECT_TAGGED_DATA_ACCESS(Storage, Tag, name)                 \
    namespace impl {                                                           \
    template<>                                                                 \
    struct tagged_data_accessor<Storage, Tag>                                  \
    {                                                                          \
        static bool                                                            \
        has(Storage const& storage)                                            \
        {                                                                      \
            return storage.name != nullptr;                                    \
        }                                                                      \
        static void                                                            \
        add(Storage& storage, Tag::data_type data)                             \
        {                                                                      \
            storage.name = &data;                                              \
        }                                                                      \
        static void                                                            \
        remove(Storage& storage)                                               \
        {                                                                      \
            storage.name = nullptr;                                            \
        }                                                                      \
        static Tag::data_type                                                  \
        get(Storage& storage)                                                  \
        {                                                                      \
            return *storage.name;                                              \
        }                                                                      \
    };                                                                         \
    }

} // namespace impl

} // namespace alia




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
struct duplex_signal
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
// A duplex signal can work as anything.
template<class Expected>
struct signal_direction_is_compatible<Expected, duplex_signal> : std::true_type
{
};
// Resolve ambiguity.
template<>
struct signal_direction_is_compatible<duplex_signal, duplex_signal>
    : std::true_type
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
// A duplex signal has both capabilities.
template<class A>
struct signal_direction_intersection<A, duplex_signal>
{
    typedef A type;
};
template<class B>
struct signal_direction_intersection<duplex_signal, B>
{
    typedef B type;
};
// Resolve ambiguity.
template<>
struct signal_direction_intersection<duplex_signal, duplex_signal>
{
    typedef duplex_signal type;
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
    // All other combinations yield duplex signals.
    typedef duplex_signal type;
};

// untyped_signal_base defines functionality common to all signals, irrespective
// of the type of the value that the signal carries.
struct untyped_signal_base
{
    // Does the signal currently have a value?
    virtual bool
    has_value() const = 0;

    // A signal must supply an ID that uniquely identifies its value.
    //
    // The ID is required to be valid if has_value() returns true.
    // (It may be valid even if has_value() returns false, which would mean
    // that the signal can identify its value but doesn't know it yet.)
    //
    // The returned ID reference is only guaranteed to be valid as long as the
    // signal itself is valid.
    //
    virtual id_interface const&
    value_id() const = 0;

    // Is the signal currently ready to write?
    virtual bool
    ready_to_write() const = 0;
};

template<class Value>
struct signal_interface : untyped_signal_base
{
    typedef Value value_type;

    // Read the signal's value. The reference returned here is only guaranteed
    // to be valid as long as the signal object itself is valid.
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
    auto operator[](Index index) const;
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
    ready_to_write() const
    {
        return false;
    }
    void
    write(Value const&) const
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
        return null_id;
    }
    bool
    has_value() const
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
    has_value() const
    {
        return ref_->has_value();
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
    ready_to_write() const
    {
        return ref_->ready_to_write();
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

// duplex<Value> denotes a reference to a duplex signal carrying values of type
// :Value.
template<class Value>
using duplex = signal_ref<Value, duplex_signal>;

// is_signal_type<T>::value yields a compile-time boolean indicating whether or
// not T is an alia signal type.
template<class T>
struct is_signal_type : std::is_base_of<untyped_signal_base, T>
{
};

// signal_is_readable<Signal>::value yields a compile-time boolean indicating
// whether or not the given signal type supports reading.
template<class Signal>
struct signal_is_readable : signal_direction_is_compatible<
                                read_only_signal,
                                typename Signal::direction_tag>
{
};

// is_readable_signal_type<T>::value yields a compile-time boolean indicating
// whether or not T is an alia signal that supports reading.
template<class T>
struct is_readable_signal_type : std::conditional_t<
                                     is_signal_type<T>::value,
                                     signal_is_readable<T>,
                                     std::false_type>
{
};

// Does :signal currently have a value?
// Unlike calling signal.has_value() directly, this will generate a
// compile-time error if the signal's type doesn't support reading.
template<class Signal>
std::enable_if_t<signal_is_readable<Signal>::value, bool>
signal_has_value(Signal const& signal)
{
    return signal.has_value();
}

// Read a signal's value.
// Unlike calling signal.read() directly, this will generate a compile-time
// error if the signal's type doesn't support reading and a run-time error if
// the signal doesn't currently have a value.
template<class Signal>
std::enable_if_t<
    signal_is_readable<Signal>::value,
    typename Signal::value_type const&>
read_signal(Signal const& signal)
{
    assert(signal.has_value());
    return signal.read();
}

// signal_is_writable<Signal>::value yields a compile-time boolean indicating
// whether or not the given signal type supports writing.
template<class Signal>
struct signal_is_writable : signal_direction_is_compatible<
                                write_only_signal,
                                typename Signal::direction_tag>
{
};

// is_writable_signal_type<T>::value yields a compile-time boolean indicating
// whether or not T is an alia signal that supports writing.
template<class T>
struct is_writable_signal_type : std::conditional_t<
                                     is_signal_type<T>::value,
                                     signal_is_writable<T>,
                                     std::false_type>
{
};

// Is :signal ready to write?
// Unlike calling signal.ready_to_write() directly, this will generate a
// compile-time error if the signal's type doesn't support writing.
template<class Signal>
std::enable_if_t<signal_is_writable<Signal>::value, bool>
signal_ready_to_write(Signal const& signal)
{
    return signal.ready_to_write();
}

// Write a signal's value.
// Unlike calling signal.write() directly, this will generate a compile-time
// error if the signal's type doesn't support writing.
// Note that if the signal isn't ready to write, this is a no op.
template<class Signal, class Value>
std::enable_if_t<signal_is_writable<Signal>::value>
write_signal(Signal const& signal, Value const& value)
{
    if (signal.ready_to_write())
        signal.write(value);
}

// signal_is_duplex<Signal>::value yields a compile-time boolean indicating
// whether or not the given signal type supports both reading and writing.
template<class Signal>
struct signal_is_duplex : signal_direction_is_compatible<
                              duplex_signal,
                              typename Signal::direction_tag>
{
};

// is_duplex_signal_type<T>::value yields a compile-time boolean indicating
// whether or not T is an alia signal that supports both reading and writing.
template<class T>
struct is_duplex_signal_type : std::conditional_t<
                                   is_signal_type<T>::value,
                                   signal_is_duplex<T>,
                                   std::false_type>
{
};

// When a value is written to a signal, the signal is allowed to throw a
// validation_error if the value isn't acceptable.
struct validation_error : exception
{
    validation_error(std::string const& message) : exception(message)
    {
    }
    ~validation_error() noexcept(true)
    {
    }
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
        if (this->has_value())
        {
            id_ = make_id_by_reference(this->read());
            return id_;
        }
        return null_id;
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

// signals_all_have_values(signal_a, signal_b, ...) is a variadic function that
// returns true iff all its input signals have values.
inline bool
signals_all_have_values()
{
    return true;
}
template<class Signal, class... Rest>
bool
signals_all_have_values(Signal const& signal, Rest const&... rest)
{
    return signal_has_value(signal) && signals_all_have_values(rest...);
}

// When assigning value IDs for signals with value type Value, should we prefer
// to use a simple ID?
template<class Value>
struct type_prefers_simple_id : std::is_fundamental<Value>
{
};

// preferred_id_signal is used to decide whether to assign a 'simple' value ID
// (one that is simply the value itself) when a more complex form is available.
// The simple form will of course eliminate spurious ID changes, but for large
// values, it might be unreasonably expensive, so this tries to use the simple
// form only for value types where it would be appropriate.

template<
    class Derived,
    class Value,
    class Direction,
    class ComplexId,
    class = void>
struct preferred_id_signal
{
};

template<class Derived, class Value, class Direction, class ComplexId>
struct preferred_id_signal<
    Derived,
    Value,
    Direction,
    ComplexId,
    std::enable_if_t<type_prefers_simple_id<Value>::value>>
    : signal<Derived, Value, Direction>
{
    id_interface const&
    value_id() const
    {
        if (this->has_value())
        {
            id_ = make_id_by_reference(this->read());
            return id_;
        }
        return null_id;
    }

 private:
    mutable simple_id_by_reference<Value> id_;
};

template<class Derived, class Value, class Direction, class ComplexId>
struct preferred_id_signal<
    Derived,
    Value,
    Direction,
    ComplexId,
    std::enable_if_t<!type_prefers_simple_id<Value>::value>>
    : signal<Derived, Value, Direction>
{
    id_interface const&
    value_id() const
    {
        id_ = static_cast<Derived const*>(this)->complex_value_id();
        return id_;
    }

 private:
    mutable ComplexId id_;
};

// refresh_signal_shadow is useful to monitoring a signal and responding to
// changes in its value.
template<class Signal, class OnNewValue, class OnLostValue>
void
refresh_signal_shadow(
    captured_id& id,
    Signal signal,
    OnNewValue&& on_new_value,
    OnLostValue&& on_lost_value)
{
    if (signal.has_value())
    {
        if (!id.matches(signal.value_id()))
        {
            on_new_value(signal.read());
            id.capture(signal.value_id());
        }
    }
    else
    {
        if (!id.matches(null_id))
        {
            on_lost_value();
            id.capture(null_id);
        }
    }
}

} // namespace alia


// This file defines various utilities for constructing basic signals.

namespace alia {

// empty<Value>() gives a signal that never has a value.
template<class Value>
struct empty_signal : signal<empty_signal<Value>, Value, duplex_signal>
{
    empty_signal()
    {
    }
    id_interface const&
    value_id() const
    {
        return null_id;
    }
    bool
    has_value() const
    {
        return false;
    }
    // Since this never has a value, read() should never be called.
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
    ready_to_write() const
    {
        return false;
    }
    // Since this is never ready to write, write() should never be called.
    // LCOV_EXCL_START
    void
    write(Value const&) const
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
    has_value() const
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
        id_ = make_id(text_);
        return id_;
    }
    bool
    has_value() const
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
    mutable simple_id<char const*> id_;
};
inline string_literal_signal
value(char const* text)
{
    return string_literal_signal(text);
}

// literal operators
namespace literals {
inline string_literal_signal operator"" _a(char const* s, size_t)
{
    return string_literal_signal(s);
}
} // namespace literals

// direct(x), where x is a non-const reference, creates a duplex signal that
// directly exposes the value of x.
template<class Value>
struct direct_signal
    : regular_signal<direct_signal<Value>, Value, duplex_signal>
{
    explicit direct_signal(Value* v) : v_(v)
    {
    }
    bool
    has_value() const
    {
        return true;
    }
    Value const&
    read() const
    {
        return *v_;
    }
    bool
    ready_to_write() const
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
    has_value() const
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

struct data_traversal;
ALIA_DEFINE_TAGGED_TYPE(data_traversal_tag, data_traversal&)

struct event_traversal;
ALIA_DEFINE_TAGGED_TYPE(event_traversal_tag, event_traversal&)

struct system;
ALIA_DEFINE_TAGGED_TYPE(system_tag, system&)

struct timing_subsystem;
ALIA_DEFINE_TAGGED_TYPE(timing_tag, timing_subsystem&)

// the structure we use to store context objects - It provides direct storage of
// the commonly-used objects in the core of alia.

struct context_storage
{
    // directly-stored objects
    system* sys = nullptr;
    event_traversal* event = nullptr;
    data_traversal* data = nullptr;
    timing_subsystem* timing = nullptr;

    // generic storage for other objects
    impl::generic_tagged_storage<impl::any_ref> generic;

    ALIA_IMPLEMENT_STORAGE_OBJECT_ACCESSORS(context_storage)
};

ALIA_ADD_DIRECT_TAGGED_DATA_ACCESS(context_storage, system_tag, sys)
ALIA_ADD_DIRECT_TAGGED_DATA_ACCESS(context_storage, event_traversal_tag, event)
ALIA_ADD_DIRECT_TAGGED_DATA_ACCESS(context_storage, data_traversal_tag, data)
ALIA_ADD_DIRECT_TAGGED_DATA_ACCESS(context_storage, timing_tag, timing)

// the context interface wrapper
template<class Contents>
struct context_interface
{
    context_interface(Contents contents) : contents_(std::move(contents))
    {
    }

    // copy constructor (from convertible collections)
    template<class OtherContents>
    context_interface(
        context_interface<OtherContents> other,
        std::enable_if_t<impl::structural_collection_is_convertible<
            OtherContents,
            Contents>::value>* = 0)
        : contents_(other.contents_)
    {
    }

    // assignment operator (from convertible collections)
    template<class OtherContents>
    std::enable_if_t<
        impl::structural_collection_is_convertible<OtherContents, Contents>::
            value,
        context_interface&>
    operator=(context_interface<OtherContents> other)
    {
        contents_ = other.contents_;
        return *this;
    }

    typedef Contents contents_type;

    template<class Tag>
    bool
    has() const
    {
        return impl::has_tagged_data<Tag>(contents_);
    }

    template<class Tag, class Data>
    auto
    add(Data& data)
    {
        auto new_contents
            = impl::add_tagged_data<Tag>(contents_, std::ref(data));
        return context_interface<decltype(new_contents)>(
            std::move(new_contents));
    }

    template<class Tag>
    auto
    remove()
    {
        auto new_contents = impl::remove_tagged_data<Tag>(contents_);
        return context_interface<decltype(new_contents)>(
            std::move(new_contents));
    }

    template<class Tag>
    decltype(auto)
    get()
    {
        return impl::get_tagged_data<Tag>(contents_);
    }

    Contents contents_;
};

template<class Context, class... Tag>
struct extend_context_type
{
    typedef context_interface<
        impl::add_tagged_data_types_t<typename Context::contents_type, Tag...>>
        type;
};

template<class Context, class... Tag>
using extend_context_type_t =
    typename extend_context_type<Context, Tag...>::type;

template<class Context, class Tag>
struct remove_context_tag
{
    typedef context_interface<
        impl::remove_tagged_data_type_t<typename Context::contents_type, Tag>>
        type;
};

template<class Context, class Tag>
using remove_context_tag_t = typename remove_context_tag<Context, Tag>::type;

// the typedefs for the context - There are two because we want to be able to
// represent the context with and without data capabilities.

typedef context_interface<impl::add_tagged_data_types_t<
    impl::empty_structural_collection<context_storage>,
    system_tag,
    event_traversal_tag,
    timing_tag>>
    dataless_context;

typedef extend_context_type_t<dataless_context, data_traversal_tag> context;

// And some convenience functions...

template<class Contents>
auto
make_context(Contents contents)
{
    return context_interface<Contents>(std::move(contents));
}

context
make_context(
    context_storage* storage,
    system& sys,
    event_traversal& event,
    data_traversal& data,
    timing_subsystem& timing);

template<class Context>
Context
copy_context(Context ctx)
{
    context_storage* new_storage;
    get_data(ctx, &new_storage);

    *new_storage = *ctx.contents_.storage;

    return Context(typename Context::contents_type(new_storage));
}

template<class Tag, class Contents>
decltype(auto)
get(context_interface<Contents> context)
{
    return context.template get<Tag>();
}

template<class Context>
event_traversal&
get_event_traversal(Context ctx)
{
    return ctx.template get<event_traversal_tag>();
}

template<class Context>
data_traversal&
get_data_traversal(Context ctx)
{
    return ctx.template get<data_traversal_tag>();
}

template<class Context>
struct optional_context
{
    optional_context() : ctx_(typename Context::contents_type(nullptr))
    {
    }

    bool
    has_value() const
    {
        return ctx_.contents_.storage != nullptr;
    }

    explicit operator bool() const
    {
        return has_value();
    }

    Context const operator*() const
    {
        assert(has_value());
        return ctx_;
    }
    Context operator*()
    {
        assert(has_value());
        return ctx_;
    }

    Context const* operator->() const
    {
        assert(has_value());
        return &ctx_;
    }
    Context* operator->()
    {
        assert(has_value());
        return &ctx_;
    }

    void
    reset(Context ctx)
    {
        ctx_ = ctx;
    }

    void
    reset()
    {
        ctx_ = Context(typename Context::contents_type(nullptr));
    }

 private:
    Context ctx_;
};

} // namespace alia



// This file defines the alia action interface, some common implementations of
// it, and some utilities for working with it.
//
// An action is essentially a response to an event that's dispatched by alia.
// When specifying a component that can generate events, the application
// supplies the action that should be performed when the corresponding event is
// generated. Using this style allows event handling to be written in a safer
// and more declarative manner.
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
    //
    // :intermediary is used to implement the latch-like semantics of actions.
    // It should be invoked AFTER reading any signals you need to read but
    // BEFORE invoking any side effects.
    //
    virtual void
    perform(function_view<void()> const& intermediary, Args... args) const = 0;
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
    if (action.is_ready())
        action.perform([]() {}, args...);
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
    perform(function_view<void()> const& intermediary, Args... args) const
    {
        action_->perform(intermediary, args...);
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

template<class First, class Second, class Interface>
struct action_pair;

template<class First, class Second, class... Args>
struct action_pair<First, Second, action_interface<Args...>>
    : action_interface<Args...>
{
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
    perform(function_view<void()> const& intermediary, Args... args) const
    {
        second_.perform(
            [&]() { first_.perform(intermediary, args...); }, args...);
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
    return action_pair<First, Second, typename First::action_interface>(
        first, second);
}

// operator <<
//
// (a << s), where a is an action and s is a readable signal, returns another
// action that is like :a but with the value of :s bound to its first argument.
//
template<class Action, class Signal, class Interface>
struct bound_action;
template<class Action, class Signal, class BoundArg, class... Args>
struct bound_action<Action, Signal, action_interface<BoundArg, Args...>>
    : action_interface<Args...>
{
    bound_action(Action const& action, Signal const& signal)
        : action_(action), signal_(signal)
    {
    }

    bool
    is_ready() const
    {
        return action_.is_ready() && signal_.has_value();
    }

    void
    perform(function_view<void()> const& intermediary, Args... args) const
    {
        action_.perform(intermediary, signal_.read(), args...);
    }

 private:
    Action action_;
    Signal signal_;
};
template<
    class Action,
    class Signal,
    std::enable_if_t<
        is_action_type<Action>::value && is_readable_signal_type<Signal>::value,
        int> = 0>
auto
operator<<(Action const& action, Signal const& signal)
{
    return bound_action<Action, Signal, typename Action::action_interface>(
        action, signal);
}
template<
    class Action,
    class Value,
    std::enable_if_t<
        is_action_type<Action>::value && !is_signal_type<Value>::value,
        int> = 0>
auto
operator<<(Action const& action, Value const& v)
{
    return action << value(v);
}

// operator <<=
//
// sink <<= source, where :sink and :source are both signals, creates an
// action that will set the value of :sink to the value held in :source. In
// order for the action to be considered ready, :source must have a value and
// :sink must be ready to write.

template<class Sink, class Source>
struct copy_action : action_interface<>
{
    copy_action(Sink sink, Source source) : sink_(sink), source_(source)
    {
    }

    bool
    is_ready() const
    {
        return source_.has_value() && sink_.ready_to_write();
    }

    void
    perform(function_view<void()> const& intermediary) const
    {
        auto value = source_.read();
        intermediary();
        sink_.write(value);
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
operator<<=(Sink sink, Source source)
{
    return copy_action<Sink, Source>(sink, source);
}

template<
    class Sink,
    class Source,
    std::enable_if_t<
        is_writable_signal_type<Sink>::value && !is_signal_type<Source>::value,
        int> = 0>
auto
operator<<=(Sink sink, Source source)
{
    return sink <<= value(source);
}

// toggle(flag), where :flag is a signal to a boolean, creates an action
// that will toggle the value of :flag between true and false.
//
// Note that this could also be used with other value types as long as the !
// operator provides a reasonable "toggle" function.
//
template<class Flag>
auto
toggle(Flag flag)
{
    return flag <<= !flag;
}

// push_back(container), where :container is a signal, creates an action that
// takes an item as a parameter and pushes it onto the back of :container.

template<class Container, class Item>
struct push_back_action : action_interface<Item>
{
    push_back_action(Container container) : container_(container)
    {
    }

    bool
    is_ready() const
    {
        return container_.has_value() && container_.ready_to_write();
    }

    void
    perform(function_view<void()> const& intermediary, Item item) const
    {
        auto new_container = container_.read();
        new_container.push_back(item);
        intermediary();
        container_.write(new_container);
    }

 private:
    Container container_;
};

template<class Container>
auto
push_back(Container container)
{
    return push_back_action<
        Container,
        typename Container::value_type::value_type>(container);
}

// lambda_action(is_ready, perform) creates an action whose behavior is
// defined by two function objects.
//
// :is_ready takes no arguments and simply returns true or false to indicate if
// the action is ready to be performed.
//
// :perform can take any number/type of arguments and defines the signature
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
    perform(function_view<void()> const& intermediary, Args... args) const
    {
        intermediary();
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

// The single-argument version of lambda_action creates an action that's always
// ready to perform.
template<class Perform>
auto
lambda_action(Perform perform)
{
    return lambda_action([]() { return true; }, perform);
}

} // namespace alia



namespace alia {

struct component_container;

typedef std::shared_ptr<component_container> component_container_ptr;

struct component_container
{
    component_container_ptr parent;
    // The component is dirty and needs to be refreshed immediately.
    bool dirty = false;
    // The component is animating and would like to be refreshed soon.
    bool animating = false;
};

void
mark_dirty_component(component_container_ptr const& container);

void
mark_dirty_component(dataless_context ctx);

void
mark_animating_component(component_container_ptr const& container);

void
mark_animating_component(dataless_context ctx);

struct scoped_component_container
{
    scoped_component_container()
    {
    }
    scoped_component_container(context ctx)
    {
        begin(ctx);
    }
    scoped_component_container(context ctx, component_container_ptr* container)
    {
        begin(ctx, container);
    }
    ~scoped_component_container()
    {
        end();
    }

    void
    begin(dataless_context ctx, component_container_ptr* container);

    void
    begin(context ctx);

    void
    end();

    bool
    is_on_route() const
    {
        return is_on_route_;
    }

    bool
    is_dirty() const
    {
        return is_dirty_;
    }

    bool
    is_animating() const
    {
        return is_animating_;
    }

 private:
    optional_context<dataless_context> ctx_;
    component_container_ptr* container_;
    component_container_ptr* parent_;
    bool is_on_route_;
    bool is_dirty_;
    bool is_animating_;
};

} // namespace alia



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

// data_node represents a node in the data graph that stores data.
struct data_node : noncopyable
{
    virtual ~data_node()
    {
    }

    virtual void
    clear_cache()
    {
    }

    data_node* next = nullptr;
};

struct named_block_ref_node;

// A data_block represents a block of execution. During a single traversal of
// the data graph, either all nodes in the block are executed or all nodes are
// bypassed, and, if executed, they are always executed in the same order. (It's
// conceptually similar to a 'basic block' except that other nodes may be
// executed in between nodes in a data_block.)
struct data_block : data_node
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

    // Clear all cached data stored within a data block.
    // Note that this recursively processes child blocks.
    void
    clear_cache();

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

// get_data_node(ctx, &ptr) represents a data node in the data graph.
//
// The call retrieves data from the graph at the current point in the
// traversal, assigns its address to *ptr, and advances the traversal to the
// next node.
//
// The return value is true if the data at the node was just constructed and
// false if it already existed.
//
template<class Context, class Node>
bool
get_data_node(Context& ctx, Node** ptr)
{
    data_traversal& traversal = get_data_traversal(ctx);
    data_node* node = *traversal.next_data_ptr;
    if (node)
    {
        assert(dynamic_cast<Node*>(node));
        traversal.next_data_ptr = &node->next;
        *ptr = static_cast<Node*>(node);
        return false;
    }
    else
    {
        Node* new_node = new Node;
        *traversal.next_data_ptr = new_node;
        traversal.next_data_ptr = &new_node->next;
        *ptr = new_node;
        return true;
    }
}

template<class T>
struct persistent_data_node : data_node
{
    T value;
};

template<class Context, class T>
bool
get_data(Context& ctx, T** ptr)
{
    persistent_data_node<T>* node;
    bool is_new = get_data_node(ctx, &node);
    *ptr = &node->value;
    return is_new;
}

// This is a slightly more convenient form for when you don't care about
// initializing the data at the call site.
template<class Data, class Context>
Data&
get_data(Context& ctx)
{
    Data* data;
    get_data(ctx, &data);
    return *data;
}

// get_cached_data(ctx, &ptr) is identical to get_data(ctx, &ptr), but the
// data stored in the node is understood to be a cached value of data that's
// generated by the application. The system assumes that the data can be
// regenerated if it's lost.

template<class T>
struct cached_data_node : data_node
{
    // TODO: Use optional instead.
    std::unique_ptr<T> value;

    void
    clear_cache()
    {
        value.reset();
    }
};

template<class Context, class T>
bool
get_cached_data(Context& ctx, T** ptr)
{
    cached_data_node<T>* node;
    get_data_node(ctx, &node);
    bool is_new = !node->value;
    if (is_new)
        node->value.reset(new T);
    *ptr = node->value.get();
    return is_new;
}

// This is a slightly more convenient form for when you don't care about
// initializing the data at the call site.
template<class Data, class Context>
Data&
get_cached_data(Context& ctx)
{
    Data* data;
    get_cached_data(ctx, &data);
    return *data;
}

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
struct keyed_data_signal : signal<keyed_data_signal<Data>, Data, duplex_signal>
{
    keyed_data_signal()
    {
    }
    keyed_data_signal(keyed_data<Data>* data) : data_(data)
    {
    }
    bool
    has_value() const
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
        return data_->key.is_initialized() ? data_->key.get() : null_id;
    }
    bool
    ready_to_write() const
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
    Context ctx, id_interface const& key, keyed_data_signal<Data>* signal)
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
get_keyed_data(Context ctx, id_interface const& key, Data** data)
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

struct event_dependent_if_block : noncopyable
{
    event_dependent_if_block(data_traversal& traversal, bool condition);

 private:
    scoped_data_block scoped_data_block_;
};

// The following are macros used to annotate control flow.
// They are used exactly like their C equivalents, but all require an alia_end
// after the end of their scope.
// Also note that all come in two forms. One form ends in an underscore and
// takes the context as its first argument. The other form has no trailing
// underscore and assumes that the context is a variable named 'ctx'.

// condition_is_true(x), where x is a signal whose value is testable in a
// boolean context, returns true iff x has a value and that value is true.
template<class Signal>
std::enable_if_t<is_readable_signal_type<Signal>::value, bool>
condition_is_true(Signal const& x)
{
    return signal_has_value(x) && (read_signal(x) ? true : false);
}

// condition_is_false(x), where x is a signal whose value is testable in a
// boolean context, returns true iff x has a value and that value is false.
template<class Signal>
std::enable_if_t<is_readable_signal_type<Signal>::value, bool>
condition_is_false(Signal const& x)
{
    return signal_has_value(x) && (read_signal(x) ? false : true);
}

// condition_has_value(x), where x is a readable signal type, calls
// signal_has_value.
template<class Signal>
std::enable_if_t<is_readable_signal_type<Signal>::value, bool>
condition_has_value(Signal const& x)
{
    return signal_has_value(x);
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

// condition_has_value(x), where x is NOT a signal type, always returns true.
template<class T>
std::enable_if_t<!is_signal_type<T>::value, bool>
condition_has_value(T const&)
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

// MSVC likes to warn about shadowed local variables and function parameters,
// but there's really no way to implement these macros without potentially
// shadowing things, so we need to disable those warning within the macros.
#ifdef _MSC_VER
#define ALIA_DISABLE_MACRO_WARNINGS                                            \
    __pragma(warning(push)) __pragma(warning(disable : 4456))                  \
        __pragma(warning(disable : 4457))
#define ALIA_UNDISABLE_MACRO_WARNINGS __pragma(warning(pop))
#else
#define ALIA_DISABLE_MACRO_WARNINGS
#define ALIA_UNDISABLE_MACRO_WARNINGS
#endif

// if, else_if, else

#define ALIA_IF_(ctx, condition)                                               \
    ALIA_DISABLE_MACRO_WARNINGS                                                \
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
            {                                                                  \
                ALIA_UNDISABLE_MACRO_WARNINGS

#define ALIA_IF(condition) ALIA_IF_(ctx, condition)

#define ALIA_ELSE_IF_(ctx, condition)                                          \
    ALIA_DISABLE_MACRO_WARNINGS                                                \
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
        {                                                                      \
            ALIA_UNDISABLE_MACRO_WARNINGS

#define ALIA_ELSE_IF(condition) ALIA_ELSE_IF_(ctx, condition)

#define ALIA_ELSE_(ctx)                                                        \
    ALIA_DISABLE_MACRO_WARNINGS                                                \
    }                                                                          \
    }                                                                          \
    {                                                                          \
        ::alia::if_block _alia_if_block(                                       \
            get_data_traversal(ctx), _alia_else_condition);                    \
        if (_alia_else_condition)                                              \
        {                                                                      \
            ALIA_UNDISABLE_MACRO_WARNINGS

#define ALIA_ELSE ALIA_ELSE_(ctx)

#ifndef ALIA_STRICT_MACROS
#define alia_if_(ctx, condition) ALIA_IF_(ctx, condition)
#define alia_if(condition) ALIA_IF(condition)
#define alia_else_if_(ctx, condition) ALIA_ELSE_IF_(ctx, condition)
#define alia_else_if(condition) ALIA_ELSE_IF(condition)
#define alia_else_(ctx) ALIA_ELSE(ctx)
#define alia_else ALIA_ELSE
#endif

// switch

#define ALIA_SWITCH_(ctx, x)                                                   \
    ALIA_DISABLE_MACRO_WARNINGS                                                \
    {                                                                          \
        ::alia::switch_block _alia_switch_block(ctx);                          \
        auto const& _alia_switch_value = (x);                                  \
        if (::alia::condition_has_value(_alia_switch_value))                   \
        {                                                                      \
            switch (::alia::read_condition(_alia_switch_value))                \
            {                                                                  \
                ALIA_UNDISABLE_MACRO_WARNINGS

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

#ifndef ALIA_STRICT_MACROS
#define alia_switch_(ctx, x) ALIA_SWITCH_(ctx, x)
#define alia_switch(x) ALIA_SWITCH(x)
#define alia_case_(ctx, c) ALIA_CASE(ctx, c)
#define alia_case(c) ALIA_CASE(c)
#define alia_default_(ctx) ALIA_DEFAULT_(ctx)
#define alia_default ALIA_DEFAULT
#endif

// for

#define ALIA_FOR_(ctx, x)                                                      \
    ALIA_DISABLE_MACRO_WARNINGS                                                \
    {                                                                          \
        {                                                                      \
            ::alia::loop_block _alia_looper(get_data_traversal(ctx));          \
            for (x)                                                            \
            {                                                                  \
                ::alia::scoped_data_block _alia_scope;                         \
                _alia_scope.begin(                                             \
                    _alia_looper.traversal(), _alia_looper.block());           \
                _alia_looper.next();                                           \
                ALIA_UNDISABLE_MACRO_WARNINGS

#define ALIA_FOR(x) ALIA_FOR_(ctx, x)

#ifndef ALIA_STRICT_MACROS
#define alia_for_(ctx, x) ALIA_FOR_(ctx, x)
#define alia_for(x) ALIA_FOR(x)
#endif

// while

#define ALIA_WHILE_(ctx, x)                                                    \
    ALIA_DISABLE_MACRO_WARNINGS                                                \
    {                                                                          \
        {                                                                      \
            ::alia::loop_block _alia_looper(get_data_traversal(ctx));          \
            while (x)                                                          \
            {                                                                  \
                ::alia::scoped_data_block _alia_scope;                         \
                _alia_scope.begin(                                             \
                    _alia_looper.traversal(), _alia_looper.block());           \
                _alia_looper.next();                                           \
                ALIA_UNDISABLE_MACRO_WARNINGS

#define ALIA_WHILE(x) ALIA_WHILE_(ctx, x)

#ifndef ALIA_STRICT_MACROS
#define alia_while_(ctx, x) ALIA_WHILE_(ctx, x)
#define alia_while(x) ALIA_WHILE(x)
#endif

// end

#define ALIA_END                                                               \
    }                                                                          \
    }                                                                          \
    }

#ifndef ALIA_STRICT_MACROS
#define alia_end ALIA_END
#endif

// The following macros are substitutes for normal C++ control flow statements.
// Unlike alia_if and company, they do NOT track control flow. Instead, they
// convert your context variable to a dataless_context within the block.
// This means that any attempt to retrieve data within the block will result
// in an error (as it should).

#define ALIA_REMOVE_DATA_TRACKING(ctx)                                         \
    ALIA_DISABLE_MACRO_WARNINGS                                                \
    typename decltype(ctx)::contents_type::storage_type _alia_storage;         \
    auto _alia_ctx = alia::make_context(                                       \
        alia::impl::remove_tagged_data<data_traversal_tag>(                    \
            ctx.contents_, &_alia_storage));                                   \
    auto ctx = _alia_ctx;                                                      \
    ALIA_UNDISABLE_MACRO_WARNINGS

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

#ifndef ALIA_STRICT_MACROS

#define alia_untracked_if_(ctx, condition) ALIA_UNTRACKED_IF_(ctx, condition)
#define alia_untracked_if(condition) ALIA_UNTRACKED_IF(condition)
#define alia_untracked_else_if_(ctx, condition)                                \
    ALIA_UNTRACKED_ELSE_IF_(ctx, condition)
#define alia_untracked_else_if(condition) ALIA_UNTRACKED_ELSE_IF(condition)
#define alia_untracked_else_(ctx) ALIA_UNTRACKED_ELSE(ctx)
#define alia_untracked_else ALIA_UNTRACKED_ELSE

#endif

#define ALIA_UNTRACKED_SWITCH_(ctx, expression)                                \
    ALIA_DISABLE_MACRO_WARNINGS                                                \
    {                                                                          \
        {                                                                      \
            auto const& _alia_switch_value = (expression);                     \
            ALIA_REMOVE_DATA_TRACKING(ctx)                                     \
            switch (_alia_switch_value)                                        \
            {                                                                  \
                ALIA_UNDISABLE_MACRO_WARNINGS

#define ALIA_UNTRACKED_SWITCH(expression)                                      \
    ALIA_UNTRACKED_SWITCH_(ctx, expression)

#ifndef ALIA_STRICT_MACROS

#define alia_untracked_switch_(ctx, x) ALIA_UNTRACKED_SWITCH_(ctx, x)
#define alia_untracked_switch(x) ALIA_UNTRACKED_SWITCH(x)

#endif

// event_dependent_if - This is used for tests that involve conditions that
// change from event pass to another. It does not clear out cached data within
// the block if it's skipped.

#define ALIA_EVENT_DEPENDENT_IF_(ctx, condition)                               \
    {                                                                          \
        {                                                                      \
            bool _alia_condition = alia::condition_is_true(condition);         \
            ::alia::event_dependent_if_block _alia_if_block(                   \
                get_data_traversal(ctx), _alia_condition);                     \
            if (_alia_condition)                                               \
            {

#define ALIA_EVENT_DEPENDENT_IF(condition)                                     \
    ALIA_EVENT_DEPENDENT_IF_(ctx, condition)

#ifndef ALIA_STRICT_MACROS
#define alia_event_dependent_if_(ctx, condition)                               \
    ALIA_EVENT_DEPENDENT_IF_(ctx, condition)
#define alia_event_dependent_if(condition) ALIA_EVENT_DEPENDENT_IF(condition)
#endif

} // namespace alia


namespace alia {

struct system;

bool
system_needs_refresh(system const& sys);

void
refresh_system(system& sys);

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

struct event_routing_path
{
    component_container* node;
    event_routing_path* rest;
};

struct event_traversal
{
    component_container_ptr* active_container = nullptr;
    bool targeted;
    event_routing_path* path_to_target = nullptr;
    std::type_info const* event_type;
    void* event;
    bool aborted = false;
};

template<class Context>
component_container_ptr const&
get_active_component_container(Context ctx)
{
    return *get_event_traversal(ctx).active_container;
}

namespace impl {

// Set up the event traversal so that it will route the control flow to the
// given target. (And also invoke the traversal.)
//
// :target can be null, in which case the event will be routed through the
// entire component tree.
//
void
route_event(
    system& sys, event_traversal& traversal, component_container* target);

template<class Event>
void
dispatch_targeted_event(
    system& sys, Event& event, component_container_ptr const& target)
{
    event_traversal traversal;
    traversal.targeted = true;
    traversal.event_type = &typeid(Event);
    traversal.event = &event;
    route_event(sys, traversal, target.get());
}

template<class Event>
void
dispatch_event(system& sys, Event& event)
{
    event_traversal traversal;
    traversal.targeted = false;
    traversal.event_type = &typeid(Event);
    traversal.event = &event;
    route_event(sys, traversal, nullptr);
}

} // namespace impl

template<class Event>
void
dispatch_event(system& sys, Event& event)
{
    impl::dispatch_event(sys, event);
    refresh_system(sys);
}

struct traversal_abortion
{
};

void
abort_traversal(dataless_context ctx);

inline bool
traversal_aborted(dataless_context ctx)
{
    return get_event_traversal(ctx).aborted;
}

template<class Event>
bool
detect_event(dataless_context ctx, Event** event)
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
on_event(Context ctx, Handler&& handler)
{
    Event* e;
    ALIA_UNTRACKED_IF(detect_event(ctx, &e))
    {
        handler(ctx, *e);
    }
    ALIA_END
}

typedef component_container_ptr component_identity;

typedef component_identity* component_id;

void
refresh_component_identity(dataless_context ctx, component_identity& identity);

component_id
get_component_id(context ctx);

// external_component_id identifies a component in a form that can be safely
// stored outside of the alia data graph.
struct external_component_id
{
    component_id id = nullptr;
    component_identity identity;
};

static external_component_id const null_component_id;

// Is the given external_component_id valid?
// (Only the null_component_id is invalid.)
inline bool
is_valid(external_component_id const& id)
{
    return id.id != nullptr;
}

inline external_component_id
externalize(component_id id)
{
    external_component_id external;
    external.id = id;
    external.identity = *id;
    return external;
}

struct targeted_event
{
    component_id target_id;
};

template<class Event>
void
dispatch_targeted_event(
    system& sys, Event& event, external_component_id component)
{
    event.target_id = component.id;
    impl::dispatch_targeted_event(sys, event, component.identity);
    refresh_system(sys);
}

template<class Event>
bool
detect_targeted_event(dataless_context ctx, component_id id, Event** event)
{
    return detect_event(ctx, event) && (*event)->target_id == id;
}

template<class Event, class Context, class Handler>
void
on_targeted_event(Context ctx, component_id id, Handler&& handler)
{
    Event* e;
    ALIA_UNTRACKED_IF(detect_targeted_event(ctx, id, &e))
    {
        handler(ctx, *e);
        abort_traversal(ctx);
    }
    ALIA_END
}

// the refresh event...

struct refresh_event
{
};

inline bool
is_refresh_event(dataless_context ctx)
{
    refresh_event* e;
    return detect_event(ctx, &e);
}

template<class Context, class Handler>
void
on_refresh(Context ctx, Handler handler)
{
    ALIA_UNTRACKED_IF(is_refresh_event(ctx))
    {
        handler(ctx);
    }
    ALIA_END
}

void
on_init(context ctx, action<> on_init);

void
on_activate(context ctx, action<> on_activate);

} // namespace alia




namespace alia {

// signalize(x) turns x into a signal if it isn't already one.
// Or, in other words...
// signalize(s), where s is a signal, returns s.
// signalize(v), where v is a raw value, returns a value signal carrying v.
template<class Signal>
std::enable_if_t<is_readable_signal_type<Signal>::value, Signal>
signalize(Signal s)
{
    return s;
}
template<class Value, std::enable_if_t<!is_signal_type<Value>::value, int> = 0>
auto
signalize(Value v)
{
    return value(std::move(v));
}

// fake_readability(s), where :s is a signal, yields a wrapper for :s that
// pretends to have read capabilities. It will never actually have a value, but
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
        return null_id;
    }
    bool
    has_value() const
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
    ready_to_write() const
    {
        return wrapped_.ready_to_write();
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
// pretends to have write capabilities. It will never actually be ready to
// write, but it will type-check as a writable signal.
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
    has_value() const
    {
        return wrapped_.has_value();
    }
    typename Wrapped::value_type const&
    read() const
    {
        return wrapped_.read();
    }
    bool
    ready_to_write() const
    {
        return false;
    }
    // Since this is only faking writability, write() should never be called.
    // LCOV_EXCL_START
    void
    write(typename Wrapped::value_type const&) const
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
struct casting_signal : regular_signal<
                            casting_signal<Wrapped, To>,
                            To,
                            typename Wrapped::direction_tag>
{
    casting_signal(Wrapped wrapped) : wrapped_(wrapped)
    {
    }
    bool
    has_value() const
    {
        return wrapped_.has_value();
    }
    To const&
    read() const
    {
        return lazy_reader_.read(
            [&] { return static_cast<To>(wrapped_.read()); });
    }
    bool
    ready_to_write() const
    {
        return wrapped_.ready_to_write();
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
// Don't use a casting_signal if the value type is the same.
template<class Wrapped, class To>
struct signal_caster
{
    typedef casting_signal<Wrapped, To> type;
    static type
    apply(Wrapped wrapped)
    {
        return type(wrapped);
    }
};
template<class Wrapped>
struct signal_caster<Wrapped, typename Wrapped::value_type>
{
    typedef Wrapped type;
    static type
    apply(Wrapped wrapped)
    {
        return wrapped;
    }
};
template<class To, class Wrapped>
typename signal_caster<Wrapped, To>::type
signal_cast(Wrapped wrapped)
{
    return signal_caster<Wrapped, To>::apply(wrapped);
}

// has_value(x) yields a signal to a boolean which indicates whether or not :x
// has a value. (The returned signal itself always has a value.)
template<class Wrapped>
struct value_presence_signal
    : regular_signal<value_presence_signal<Wrapped>, bool, read_only_signal>
{
    value_presence_signal(Wrapped const& wrapped) : wrapped_(wrapped)
    {
    }
    bool
    has_value() const
    {
        return true;
    }
    bool const&
    read() const
    {
        value_ = wrapped_.has_value();
        return value_;
    }

 private:
    Wrapped wrapped_;
    mutable bool value_;
};
template<class Wrapped>
auto
has_value(Wrapped const& wrapped)
{
    return value_presence_signal<Wrapped>(wrapped);
}

// ready_to_write(x) yields a signal to a boolean that indicates whether or not
// :x is ready to write. (The returned signal always has a value.)
template<class Wrapped>
struct write_readiness_signal
    : regular_signal<write_readiness_signal<Wrapped>, bool, read_only_signal>
{
    write_readiness_signal(Wrapped const& wrapped) : wrapped_(wrapped)
    {
    }
    bool
    has_value() const
    {
        return true;
    }
    bool const&
    read() const
    {
        value_ = wrapped_.ready_to_write();
        return value_;
    }

 private:
    Wrapped wrapped_;
    mutable bool value_;
};
template<class Wrapped>
auto
ready_to_write(Wrapped const& wrapped)
{
    return write_readiness_signal<Wrapped>(wrapped);
}

// add_fallback(primary, fallback), where :primary and :fallback are both
// signals, yields another signal whose value is that of :primary if it has one
// and that of :fallback otherwise.
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
    has_value() const
    {
        return primary_.has_value() || fallback_.has_value();
    }
    typename Primary::value_type const&
    read() const
    {
        return primary_.has_value() ? primary_.read() : fallback_.read();
    }
    id_interface const&
    value_id() const
    {
        id_ = combine_ids(
            make_id(primary_.has_value()),
            primary_.has_value() ? ref(primary_.value_id())
                                 : ref(fallback_.value_id()));
        return id_;
    }
    bool
    ready_to_write() const
    {
        return primary_.ready_to_write();
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
make_fallback_signal(Primary primary, Fallback fallback)
{
    return fallback_signal<Primary, Fallback>(primary, fallback);
}
template<class Primary, class Fallback>
auto
add_fallback(Primary primary, Fallback fallback)
{
    return make_fallback_signal(signalize(primary), signalize(fallback));
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
    has_value() const
    {
        return wrapped_.has_value();
    }
    typename Wrapped::value_type const&
    read() const
    {
        return wrapped_.read();
    }
    bool
    ready_to_write() const
    {
        return wrapped_.ready_to_write();
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
simplify_id(Wrapped wrapped)
{
    return simplified_id_wrapper<Wrapped>(wrapped);
}

// mask(signal, availibility_flag) does the equivalent of bit masking on
// individual signals. If :availibility_flag evaluates to true, the mask
// evaluates to signal equivalent to :signal. Otherwise, it evaluates to an
// empty signal of the same type.
template<class Primary, class Mask>
struct masking_signal : signal<
                            masking_signal<Primary, Mask>,
                            typename Primary::value_type,
                            typename Primary::direction_tag>
{
    masking_signal()
    {
    }
    masking_signal(Primary primary, Mask mask) : primary_(primary), mask_(mask)
    {
    }
    bool
    has_value() const
    {
        return mask_.has_value() && mask_.read() && primary_.has_value();
    }
    typename Primary::value_type const&
    read() const
    {
        return primary_.read();
    }
    id_interface const&
    value_id() const
    {
        if (mask_.has_value() && mask_.read())
            return primary_.value_id();
        else
            return null_id;
    }
    bool
    ready_to_write() const
    {
        return mask_.has_value() && mask_.read() && primary_.ready_to_write();
    }
    void
    write(typename Primary::value_type const& value) const
    {
        primary_.write(value);
    }

 private:
    Primary primary_;
    Mask mask_;
};
template<class Signal, class AvailabilityFlag>
auto
make_masking_signal(Signal signal, AvailabilityFlag availability_flag)
{
    return masking_signal<Signal, AvailabilityFlag>(signal, availability_flag);
}
template<class Signal, class AvailabilityFlag>
auto
mask(Signal signal, AvailabilityFlag availability_flag)
{
    return make_masking_signal(signalize(signal), signalize(availability_flag));
}

// mask_writes(signal, writability_flag) masks writes to :signal according to
// the value of :writability_flag.
//
// :writability_flag can be either a signal or a raw value. If it evaluates to
// true (in a boolean context), the mask evaluates to a signal equivalent to
// :signal. Otherwise, it evaluates to one with equivalent reading behavior but
// with writing disabled.
//
// Note that in either case, the masked version has the same directionality as
// :signal.
//
template<class Primary, class Mask>
struct write_masking_signal : signal<
                                  write_masking_signal<Primary, Mask>,
                                  typename Primary::value_type,
                                  typename Primary::direction_tag>
{
    write_masking_signal()
    {
    }
    write_masking_signal(Primary primary, Mask mask)
        : primary_(primary), mask_(mask)
    {
    }
    bool
    has_value() const
    {
        return primary_.has_value();
    }
    typename Primary::value_type const&
    read() const
    {
        return primary_.read();
    }
    id_interface const&
    value_id() const
    {
        return primary_.value_id();
    }
    bool
    ready_to_write() const
    {
        return mask_.has_value() && mask_.read() && primary_.ready_to_write();
    }
    void
    write(typename Primary::value_type const& value) const
    {
        primary_.write(value);
    }

 private:
    Primary primary_;
    Mask mask_;
};
template<class Signal, class WritabilityFlag>
auto
make_write_masking_signal(Signal signal, WritabilityFlag writability_flag)
{
    return write_masking_signal<Signal, WritabilityFlag>(
        signal, writability_flag);
}
template<class Signal, class WritabilityFlag>
auto
mask_writes(Signal signal, WritabilityFlag writability_flag)
{
    return make_write_masking_signal(signal, signalize(writability_flag));
}

// disable_writes(s), where :s is a signal, yields a wrapper for :s where writes
// are disabled. Like mask_signal, this doesn't change the directionality of :s.
template<class Signal>
auto
disable_writes(Signal s)
{
    return mask_writes(s, false);
}

// unwrap(signal), where :signal is a signal carrying a std::optional value,
// yields a signal that directly carries the value wrapped inside the optional.
template<class Wrapped>
struct unwrapper_signal : signal<
                              unwrapper_signal<Wrapped>,
                              typename Wrapped::value_type::value_type,
                              typename Wrapped::direction_tag>
{
    unwrapper_signal()
    {
    }
    unwrapper_signal(Wrapped wrapped) : wrapped_(wrapped)
    {
    }
    bool
    has_value() const
    {
        return wrapped_.has_value() && wrapped_.read().has_value();
    }
    typename Wrapped::value_type::value_type const&
    read() const
    {
        return wrapped_.read().value();
    }
    id_interface const&
    value_id() const
    {
        if (this->has_value())
            return wrapped_.value_id();
        else
            return null_id;
    }
    bool
    ready_to_write() const
    {
        return wrapped_.ready_to_write();
    }
    void
    write(typename Wrapped::value_type::value_type const& value) const
    {
        wrapped_.write(value);
    }

 private:
    Wrapped wrapped_;
};
template<class Signal>
auto
unwrap(Signal signal)
{
    return unwrapper_signal<Signal>(signal);
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
    lazy_apply1_signal(Function f, Arg arg) : f_(f), arg_(arg)
    {
    }
    id_interface const&
    value_id() const
    {
        return arg_.value_id();
    }
    bool
    has_value() const
    {
        return arg_.has_value();
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
lazy_apply(Function f, Arg arg)
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
    lazy_apply2_signal(Function f, Arg0 arg0, Arg1 arg1)
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
    has_value() const
    {
        return arg0_.has_value() && arg1_.has_value();
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
lazy_apply(Function f, Arg0 arg0, Arg1 arg1)
{
    return lazy_apply2_signal<
        decltype(f(read_signal(arg0), read_signal(arg1))),
        Function,
        Arg0,
        Arg1>(f, arg0, arg1);
}

template<class Function>
auto
lazy_lift(Function f)
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
    has_value() const
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
process_apply_args(context, apply_result_data<Result>&, bool&)
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
    if (is_refresh_event(ctx))
    {
        if (!signal_has_value(arg))
        {
            reset(data);
            args_ready = false;
        }
        else if (!cached_id->matches(arg.value_id()))
        {
            reset(data);
            cached_id->capture(arg.value_id());
        }
    }
    process_apply_args(ctx, data, args_ready, rest...);
}

template<class Function, class... Args>
auto
apply(context ctx, Function f, Args const&... args)
{
    apply_result_data<decltype(f(read_signal(args)...))>* data_ptr;
    get_cached_data(ctx, &data_ptr);
    auto& data = *data_ptr;
    bool args_ready = true;
    process_apply_args(ctx, data, args_ready, args...);
    if (is_refresh_event(ctx))
    {
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
lift(Function f)
{
    return [=](context ctx, auto&&... args) { return apply(ctx, f, args...); };
}

// alia_mem_fn(m) wraps a member function name in a lambda so that it can be
// passed as a function object. (It's the equivalent of std::mem_fn, but there's
// no need to provide the type name.)
#define ALIA_MEM_FN(m) [](auto&& x, auto&&... args) { return x.m(args...); }
#ifndef ALIA_STRICT_MACROS
#define alia_mem_fn(m) ALIA_MEM_FN(m)
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

#define ALIA_DEFINE_LIBERAL_BINARY_SIGNAL_OPERATOR(op)                         \
    template<                                                                  \
        class A,                                                               \
        class B,                                                               \
        std::enable_if_t<                                                      \
            is_signal_type<A>::value && !is_signal_type<B>::value,             \
            int> = 0>                                                          \
    auto operator op(A const& a, B const& b)                                   \
    {                                                                          \
        return lazy_apply([](auto a, auto b) { return a op b; }, a, value(b)); \
    }                                                                          \
    template<                                                                  \
        class A,                                                               \
        class B,                                                               \
        std::enable_if_t<                                                      \
            !is_signal_type<A>::value && !is_action_type<A>::value             \
                && is_signal_type<B>::value,                                   \
            int> = 0>                                                          \
    auto operator op(A const& a, B const& b)                                   \
    {                                                                          \
        return lazy_apply([](auto a, auto b) { return a op b; }, value(a), b); \
    }

ALIA_DEFINE_LIBERAL_BINARY_SIGNAL_OPERATOR(+)
ALIA_DEFINE_LIBERAL_BINARY_SIGNAL_OPERATOR(-)
ALIA_DEFINE_LIBERAL_BINARY_SIGNAL_OPERATOR(*)
ALIA_DEFINE_LIBERAL_BINARY_SIGNAL_OPERATOR(/)
ALIA_DEFINE_LIBERAL_BINARY_SIGNAL_OPERATOR (^)
ALIA_DEFINE_LIBERAL_BINARY_SIGNAL_OPERATOR(%)
ALIA_DEFINE_LIBERAL_BINARY_SIGNAL_OPERATOR(&)
ALIA_DEFINE_LIBERAL_BINARY_SIGNAL_OPERATOR(|)
ALIA_DEFINE_LIBERAL_BINARY_SIGNAL_OPERATOR(<<)
ALIA_DEFINE_LIBERAL_BINARY_SIGNAL_OPERATOR(>>)
ALIA_DEFINE_LIBERAL_BINARY_SIGNAL_OPERATOR(==)
ALIA_DEFINE_LIBERAL_BINARY_SIGNAL_OPERATOR(!=)
ALIA_DEFINE_LIBERAL_BINARY_SIGNAL_OPERATOR(<)
ALIA_DEFINE_LIBERAL_BINARY_SIGNAL_OPERATOR(<=)
ALIA_DEFINE_LIBERAL_BINARY_SIGNAL_OPERATOR(>)
ALIA_DEFINE_LIBERAL_BINARY_SIGNAL_OPERATOR(>=)

#define ALIA_DEFINE_UNARY_SIGNAL_OPERATOR(op)                                  \
    template<class A, std::enable_if_t<is_signal_type<A>::value, int> = 0>     \
    auto operator op(A const& a)                                               \
    {                                                                          \
        return lazy_apply([](auto a) { return op a; }, a);                     \
    }

ALIA_DEFINE_UNARY_SIGNAL_OPERATOR(-)
ALIA_DEFINE_UNARY_SIGNAL_OPERATOR(!)

#undef ALIA_DEFINE_UNARY_SIGNAL_OPERATOR

// For most compound assignment operators (e.g., +=), a += b, where :a and
// :b are signals, creates an action that sets :a equal to :a + :b.

#define ALIA_DEFINE_COMPOUND_ASSIGNMENT_OPERATOR(assignment_form, normal_form) \
    template<                                                                  \
        class A,                                                               \
        class B,                                                               \
        std::enable_if_t<                                                      \
            is_duplex_signal_type<A>::value                                    \
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

#define ALIA_DEFINE_LIBERAL_COMPOUND_ASSIGNMENT_OPERATOR(                      \
    assignment_form, normal_form)                                              \
    template<                                                                  \
        class A,                                                               \
        class B,                                                               \
        std::enable_if_t<                                                      \
            is_duplex_signal_type<A>::value && !is_signal_type<B>::value,      \
            int> = 0>                                                          \
    auto operator assignment_form(A const& a, B const& b)                      \
    {                                                                          \
        return a <<= (a normal_form value(b));                                 \
    }

ALIA_DEFINE_LIBERAL_COMPOUND_ASSIGNMENT_OPERATOR(+=, +)
ALIA_DEFINE_LIBERAL_COMPOUND_ASSIGNMENT_OPERATOR(-=, -)
ALIA_DEFINE_LIBERAL_COMPOUND_ASSIGNMENT_OPERATOR(*=, *)
ALIA_DEFINE_LIBERAL_COMPOUND_ASSIGNMENT_OPERATOR(/=, /)
ALIA_DEFINE_LIBERAL_COMPOUND_ASSIGNMENT_OPERATOR(^=, ^)
ALIA_DEFINE_LIBERAL_COMPOUND_ASSIGNMENT_OPERATOR(%=, %)
ALIA_DEFINE_LIBERAL_COMPOUND_ASSIGNMENT_OPERATOR(&=, &)
ALIA_DEFINE_LIBERAL_COMPOUND_ASSIGNMENT_OPERATOR(|=, |)

// The increment and decrement operators work similarly.

#define ALIA_DEFINE_BY_ONE_OPERATOR(assignment_form, normal_form)              \
    template<                                                                  \
        class A,                                                               \
        std::enable_if_t<is_duplex_signal_type<A>::value, int> = 0>            \
    auto operator assignment_form(A const& a)                                  \
    {                                                                          \
        return a <<= (a normal_form value(typename A::value_type(1)));         \
    }                                                                          \
    template<                                                                  \
        class A,                                                               \
        std::enable_if_t<is_duplex_signal_type<A>::value, int> = 0>            \
    auto operator assignment_form(A const& a, int)                             \
    {                                                                          \
        return a <<= (a normal_form value(typename A::value_type(1)));         \
    }

ALIA_DEFINE_BY_ONE_OPERATOR(++, +)
ALIA_DEFINE_BY_ONE_OPERATOR(--, -)

#undef ALIA_DEFINE_BY_ONE_OPERATOR

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
    has_value() const
    {
        // Obviously, this has a value if both of its arguments have values.
        // However, we can also determine a value if only one input has a value
        // but that value is true.
        return (arg0_.has_value() && arg1_.has_value())
               || (arg0_.has_value() && arg0_.read())
               || (arg1_.has_value() && arg1_.read());
    }
    bool const&
    read() const
    {
        value_ = (arg0_.has_value() && arg0_.read())
                 || (arg1_.has_value() && arg1_.read());
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
        is_signal_type<A>::value && is_signal_type<B>::value,
        int> = 0>
auto
operator||(A const& a, B const& b)
{
    return logical_or_signal<A, B>(a, b);
}

template<
    class A,
    class B,
    std::enable_if_t<
        is_signal_type<A>::value && !is_signal_type<B>::value,
        int> = 0>
auto
operator||(A const& a, B const& b)
{
    return a || value(b);
}
template<
    class A,
    class B,
    std::enable_if_t<
        !is_signal_type<A>::value && is_signal_type<B>::value,
        int> = 0>
auto
operator||(A const& a, B const& b)
{
    return value(a) || b;
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
    has_value() const
    {
        // Obviously, this has a value if both of its arguments have values.
        // However, we can also determine a value if only one input has a value
        // but that value is false.
        return (arg0_.has_value() && arg1_.has_value())
               || (arg0_.has_value() && !arg0_.read())
               || (arg1_.has_value() && !arg1_.read());
    }
    bool const&
    read() const
    {
        value_
            = !((arg0_.has_value() && !arg0_.read())
                || (arg1_.has_value() && !arg1_.read()));
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
        is_signal_type<A>::value && is_signal_type<B>::value,
        int> = 0>
auto
operator&&(A const& a, B const& b)
{
    return logical_and_signal<A, B>(a, b);
}

template<
    class A,
    class B,
    std::enable_if_t<
        is_signal_type<A>::value && !is_signal_type<B>::value,
        int> = 0>
auto
operator&&(A const& a, B const& b)
{
    return a && value(b);
}
template<
    class A,
    class B,
    std::enable_if_t<
        !is_signal_type<A>::value && is_signal_type<B>::value,
        int> = 0>
auto
operator&&(A const& a, B const& b)
{
    return value(a) && b;
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
    has_value() const
    {
        return condition_.has_value()
               && (condition_.read() ? t_.has_value() : f_.has_value());
    }
    typename T::value_type const&
    read() const
    {
        return condition_.read() ? t_.read() : f_.read();
    }
    id_interface const&
    value_id() const
    {
        if (!condition_.has_value())
            return null_id;
        id_ = combine_ids(
            make_id(condition_.read()),
            condition_.read() ? ref(t_.value_id()) : ref(f_.value_id()));
        return id_;
    }
    bool
    ready_to_write() const
    {
        return condition_.has_value()
               && (condition_.read() ? t_.ready_to_write()
                                     : f_.ready_to_write());
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
make_signal_mux(Condition condition, T t, F f)
{
    return signal_mux<Condition, T, F>(condition, t, f);
}

template<class Condition, class T, class F>
auto
conditional(Condition condition, T t, F f)
{
    return make_signal_mux(signalize(condition), signalize(t), signalize(f));
}

// Given a signal to a structure, signal->*field_ptr returns a signal to the
// specified field within the structure.
template<class StructureSignal, class Field>
struct field_signal : preferred_id_signal<
                          field_signal<StructureSignal, Field>,
                          Field,
                          typename StructureSignal::direction_tag,
                          id_pair<id_ref, simple_id<Field*>>>
{
    typedef typename StructureSignal::value_type structure_type;
    typedef Field structure_type::*field_ptr;
    field_signal(StructureSignal structure, field_ptr field)
        : structure_(structure), field_(field)
    {
    }
    bool
    has_value() const
    {
        return structure_.has_value();
    }
    Field const&
    read() const
    {
        structure_type const& structure = structure_.read();
        return structure.*field_;
    }
    auto
    complex_value_id() const
    {
        // Apparently pointers-to-members aren't comparable for order, so
        // instead we use the address of the field if it were in a structure
        // that started at address 0.
        return combine_ids(
            ref(structure_.value_id()),
            make_id(&(((structure_type*) 0)->*field_)));
    }
    bool
    ready_to_write() const
    {
        return structure_.has_value() && structure_.ready_to_write();
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
#ifndef ALIA_STRICT_MACROS
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
// indicating whether or not Container has an 'at' member function that takes an
// Index.
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
// compile-time boolean indicating whether or not invoke_const_subscript returns
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
std::enable_if_t<signal_is_writable<ContainerSignal>::value>
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
std::enable_if_t<!signal_is_writable<ContainerSignal>::value>
write_subscript(ContainerSignal const&, IndexSignal const&, Value const&)
{
}

template<class ContainerSignal, class IndexSignal>
struct subscript_signal : preferred_id_signal<
                              subscript_signal<ContainerSignal, IndexSignal>,
                              typename subscript_result_type<
                                  typename ContainerSignal::value_type,
                                  typename IndexSignal::value_type>::type,
                              typename ContainerSignal::direction_tag,
                              id_pair<alia::id_ref, alia::id_ref>>
{
    subscript_signal()
    {
    }
    subscript_signal(ContainerSignal array, IndexSignal index)
        : container_(array), index_(index)
    {
    }
    bool
    has_value() const
    {
        return container_.has_value() && index_.has_value();
    }
    typename subscript_signal::value_type const&
    read() const
    {
        return invoker_(container_.read(), index_.read());
    }
    auto
    complex_value_id() const
    {
        return combine_ids(ref(container_.value_id()), ref(index_.value_id()));
    }
    bool
    ready_to_write() const
    {
        return container_.has_value() && index_.has_value()
               && container_.ready_to_write();
    }
    void
    write(typename subscript_signal::value_type const& x) const
    {
        write_subscript(container_, index_, x);
    }

 private:
    ContainerSignal container_;
    IndexSignal index_;
    const_subscript_invoker<
        typename ContainerSignal::value_type,
        typename IndexSignal::value_type>
        invoker_;
};
template<class ContainerSignal, class IndexSignal>
subscript_signal<ContainerSignal, IndexSignal>
make_subscript_signal(ContainerSignal container, IndexSignal index)
{
    return subscript_signal<ContainerSignal, IndexSignal>(container, index);
}

template<class Derived, class Value, class Direction>
template<class Index>
auto signal_base<Derived, Value, Direction>::operator[](Index index) const
{
    return make_subscript_signal(
        static_cast<Derived const&>(*this), signalize(index));
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
get_alia_id(Item const&)
{
    return null_id;
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
for_each(Context ctx, ContainerSignal const& container_signal, Fn&& fn)
{
    ALIA_IF(has_value(container_signal))
    {
        naming_context nc(ctx);
        auto const& container = read_signal(container_signal);
        for (auto const& item : container)
        {
            named_block nb;
            auto iteration_id = get_alia_id(item.first);
            if (iteration_id != null_id)
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
for_each(Context ctx, ContainerSignal const& container_signal, Fn&& fn)
{
    ALIA_IF(has_value(container_signal))
    {
        naming_context nc(ctx);
        auto const& container = read_signal(container_signal);
        size_t const item_count = container.size();
        for (size_t index = 0; index != item_count; ++index)
        {
            named_block nb;
            auto iteration_id = get_alia_id(container[index]);
            if (iteration_id != null_id)
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
    has_value() const
    {
        return list_signal_.has_value();
    }
    Item const&
    read() const
    {
        return *item_;
    }
    bool
    ready_to_write() const
    {
        return list_signal_.ready_to_write();
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
for_each(Context ctx, ContainerSignal const& container_signal, Fn&& fn)
{
    ALIA_IF(has_value(container_signal))
    {
        naming_context nc(ctx);
        auto const& container = read_signal(container_signal);
        size_t index = 0;
        for (auto const& item : container)
        {
            named_block nb;
            auto iteration_id = get_alia_id(item);
            if (iteration_id != null_id)
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



namespace alia {

template<class Object>
struct tree_node : noncopyable
{
    Object object;

    ~tree_node()
    {
        if (prev_)
        {
            this->remove_from_list();
            object.remove();
        }
        if (children_)
        {
            // The parent is being destructed before the children, so just
            // wipe all their tracking pointers.
            tree_node* child = children_;
            while (child)
            {
                tree_node* next = child->next_;
                child->prev_ = nullptr;
                child->next_ = nullptr;
                child = next;
            }
        }
    }

    void
    remove_from_list()
    {
        if (next_)
            next_->prev_ = prev_;
        if (prev_)
            *prev_ = next_;
    }

    void
    insert_into_list(tree_node** prev, tree_node* next)
    {
        prev_ = prev;
        *prev_ = this;
        next_ = next;
        if (next)
            next->prev_ = &this->next_;
    }

    tree_node** prev_ = nullptr;
    tree_node* next_ = nullptr;
    tree_node* children_ = nullptr;
};

template<class Object>
struct tree_traversal
{
    tree_node<Object>* last_sibling = nullptr;
    tree_node<Object>** next_ptr = nullptr;
    tree_node<Object>* active_parent = nullptr;
};

template<class Object>
void
check_for_movement(tree_traversal<Object>& traversal, tree_node<Object>& node)
{
    tree_node<Object>* expected_node = *traversal.next_ptr;
    if (expected_node != &node)
    {
        node.remove_from_list();
        node.object.relocate(
            traversal.active_parent->object,
            traversal.last_sibling ? &traversal.last_sibling->object : nullptr);
        node.insert_into_list(traversal.next_ptr, expected_node);
    }
}

template<class Object>
void
depart_node(tree_traversal<Object>& traversal, tree_node<Object>& object)
{
    traversal.last_sibling = &object;
    traversal.next_ptr = &object.next_;
}

template<class Object>
void
refresh_tree_node(tree_traversal<Object>& traversal, tree_node<Object>& object)
{
    check_for_movement(traversal, object);
    depart_node(traversal, object);
}

template<class Object>
void
activate_parent_node(
    tree_traversal<Object>& traversal, tree_node<Object>& object)
{
    traversal.active_parent = &object;
    traversal.last_sibling = nullptr;
    traversal.next_ptr = &object.children_;
}

template<class Object>
void
cap_sibling_list(tree_traversal<Object>& traversal)
{
    if (*traversal.next_ptr)
    {
        tree_node<Object>* expected_node = *traversal.next_ptr;
        do
        {
            expected_node->object.remove();
            expected_node->remove_from_list();
            expected_node->prev_ = nullptr;
            expected_node = expected_node->next_;
        } while (expected_node);
        *traversal.next_ptr = nullptr;
    }
}

template<class Object>
struct scoped_tree_node
{
    scoped_tree_node() : traversal_(nullptr)
    {
    }
    scoped_tree_node(tree_traversal<Object>& traversal, tree_node<Object>& node)
    {
        begin(traversal, node);
    }
    ~scoped_tree_node()
    {
        end();
    }

    void
    begin(tree_traversal<Object>& traversal, tree_node<Object>& node)
    {
        traversal_ = &traversal;
        node_ = &node;
        parent_ = traversal.active_parent;
        check_for_movement(traversal, node);
        activate_parent_node(traversal, node);
    }

    void
    end()
    {
        if (traversal_)
        {
            cap_sibling_list(*traversal_);
            traversal_->active_parent = parent_;
            depart_node(*traversal_, *node_);
            traversal_ = nullptr;
        }
    }

 private:
    tree_traversal<Object>* traversal_;
    tree_node<Object>* parent_;
    tree_node<Object>* node_;
};

template<class Object>
struct scoped_tree_children
{
    scoped_tree_children() : traversal_(nullptr)
    {
    }
    scoped_tree_children(
        tree_traversal<Object>& traversal, tree_node<Object>& parent)
    {
        begin(traversal, parent);
    }
    ~scoped_tree_children()
    {
        end();
    }

    void
    begin(tree_traversal<Object>& traversal, tree_node<Object>& parent)
    {
        traversal_ = &traversal;
        old_traversal_state_ = traversal;
        activate_parent_node(traversal, parent);
    }

    void
    end()
    {
        if (traversal_)
        {
            cap_sibling_list(*traversal_);
            *traversal_ = old_traversal_state_;
            traversal_ = nullptr;
        }
    }

 private:
    tree_traversal<Object>* traversal_;
    tree_traversal<Object> old_traversal_state_;
};

template<class Object, class Content>
void
traverse_object_tree(
    tree_traversal<Object>& traversal,
    tree_node<Object>& root,
    Content&& content)
{
    activate_parent_node(traversal, root);
    content();
    cap_sibling_list(traversal);
}

template<class Object>
struct tree_caching_data
{
    captured_id content_id;
    tree_node<Object>** predecessor = nullptr;
    tree_node<Object>* subtree_head = nullptr;
    tree_node<Object>** subtree_tail = nullptr;
    tree_node<Object>* last_sibling = nullptr;
};

template<class Object>
struct scoped_tree_cacher
{
    scoped_tree_cacher() : traversal_(nullptr)
    {
    }
    scoped_tree_cacher(
        tree_traversal<Object>& traversal,
        tree_caching_data<Object>& data,
        id_interface const& content_id,
        bool force_update)
    {
        begin(traversal, data, content_id, force_update);
    }
    ~scoped_tree_cacher()
    {
        if (!std::uncaught_exception())
            end();
    }

    void
    begin(
        tree_traversal<Object>& traversal,
        tree_caching_data<Object>& data,
        id_interface const& content_id,
        bool force_update)
    {
        traversal_ = &traversal;
        data_ = &data;
        content_traversal_required_ = force_update;

        // If the content ID changes, we know we have to refresh the contents.
        if (!data.content_id.matches(content_id))
            content_traversal_required_ = true;

        // Check to see that we're inserting this where it's expected.
        // If not, traverse the contents. (It's not technically necessary to do
        // this. We could instead move the cached contents here, but this is
        // easier and fine for now.)
        if (data.predecessor != traversal.next_ptr)
            content_traversal_required_ = true;

        if (content_traversal_required_)
        {
            // If we're updating the contents, capture the content ID now (while
            // it's still valid) so we can store it in end().
            content_id_.capture(content_id);

            // Also record the current value of the tree traversal's next_ptr.
            predecessor_ = traversal.next_ptr;
        }
        else
        {
            // And if we're not updating the contents, just splice it in.
            *traversal.next_ptr = data.subtree_head;
            traversal.next_ptr = data.subtree_tail;
            traversal.last_sibling = data.last_sibling;
        }
    }

    void
    end()
    {
        if (traversal_)
        {
            // If the subtree was traversed, record the head and tail of it so
            // we can splice it in on future passes.
            if (content_traversal_required_)
            {
                data_->predecessor = predecessor_;
                data_->subtree_head = *predecessor_;
                data_->subtree_tail = traversal_->next_ptr;
                data_->last_sibling = traversal_->last_sibling;
                data_->content_id = std::move(content_id_);
            }

            traversal_ = nullptr;
        }
    }

    bool
    content_traversal_required() const
    {
        return content_traversal_required_;
    }

 private:
    tree_traversal<Object>* traversal_;
    tree_caching_data<Object>* data_;
    bool content_traversal_required_;
    captured_id content_id_;
    tree_node<Object>** predecessor_;
};

} // namespace alia



namespace alia {

enum class async_status
{
    UNREADY,
    LAUNCHED,
    COMPLETE,
    FAILED
};

template<class Value>
struct async_operation_data
{
    counter_type version = 0;
    Value result;
    async_status status = async_status::UNREADY;
};

template<class Value>
void
reset(async_operation_data<Value>& data)
{
    if (data.status != async_status::UNREADY)
    {
        ++data.version;
        data.status = async_status::UNREADY;
    }
}

template<class Value>
struct async_signal : signal<async_signal<Value>, Value, read_only_signal>
{
    async_signal(async_operation_data<Value>& data) : data_(&data)
    {
    }
    id_interface const&
    value_id() const
    {
        id_ = make_id(data_->version);
        return id_;
    }
    bool
    has_value() const
    {
        return data_->status == async_status::COMPLETE;
    }
    Value const&
    read() const
    {
        return data_->result;
    }

 private:
    async_operation_data<Value>* data_;
    mutable simple_id<counter_type> id_;
};

template<class Value>
async_signal<Value>
make_async_signal(async_operation_data<Value>& data)
{
    return async_signal<Value>(data);
}

template<class Result>
void
process_async_args(context, async_operation_data<Result>&, bool&)
{
}
template<class Result, class Arg, class... Rest>
void
process_async_args(
    context ctx,
    async_operation_data<Result>& data,
    bool& args_ready,
    Arg const& arg,
    Rest const&... rest)
{
    captured_id* cached_id;
    get_cached_data(ctx, &cached_id);
    if (is_refresh_event(ctx))
    {
        if (!signal_has_value(arg))
        {
            reset(data);
            args_ready = false;
        }
        else if (!cached_id->matches(arg.value_id()))
        {
            reset(data);
            cached_id->capture(arg.value_id());
        }
    }
    process_async_args(ctx, data, args_ready, rest...);
}

template<class Result, class Context, class Launcher, class... Args>
auto
async(Context ctx, Launcher launcher, Args const&... args)
{
    std::shared_ptr<async_operation_data<Result>>& data_ptr
        = get_cached_data<std::shared_ptr<async_operation_data<Result>>>(ctx);
    if (!data_ptr)
        data_ptr.reset(new async_operation_data<Result>);
    auto& data = *data_ptr;

    bool args_ready = true;
    process_async_args(ctx, data, args_ready, args...);

    on_refresh(ctx, [&](auto ctx) {
        if (data.status == async_status::UNREADY && args_ready)
        {
            auto* system = &get<system_tag>(ctx);
            auto version = data.version;
            auto container = get_active_component_container(ctx);
            auto report_result
                = [system, version, container, data_ptr](Result result) {
                      auto& data = *data_ptr;
                      if (data.version == version)
                      {
                          data.result = std::move(result);
                          data.status = async_status::COMPLETE;
                          mark_dirty_component(container);
                          refresh_system(*system);
                      }
                  };
            try
            {
                launcher(ctx, report_result, read_signal(args)...);
            }
            catch (...)
            {
                data.status = async_status::FAILED;
            }
        }
    });

    return make_async_signal(data);
}

} // namespace alia


#include <map>
#include <utility>
#include <vector>


namespace alia {

// `transform()` is the component-level version of `std::transform`. See the
// docs for more details.

// the sequence version...

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
        mapped_sequence_data<MappedItem>& data, bool all_items_have_values)
        : data_(&data), all_items_have_values_(all_items_have_values)
    {
    }
    id_interface const&
    value_id() const
    {
        id_ = make_id(data_->output_version);
        return id_;
    }
    bool
    has_value() const
    {
        return all_items_have_values_;
    }
    std::vector<MappedItem> const&
    read() const
    {
        return data_->mapped_items;
    }

 private:
    mapped_sequence_data<MappedItem>* data_;
    bool all_items_have_values_;
    mutable simple_id<counter_type> id_;
};

template<
    class Context,
    class Container,
    class Function,
    std::enable_if_t<
        !is_map_like<typename Container::value_type>::value,
        int> = 0>
auto
transform(Context ctx, Container const& container, Function&& f)
{
    typedef typename decltype(f(
        ctx,
        std::declval<readable<typename Container::value_type::value_type>>()))::
        value_type mapped_value_type;

    mapped_sequence_data<mapped_value_type>* data;
    get_cached_data(ctx, &data);

    bool all_items_have_values = false;

    ALIA_IF(has_value(container))
    {
        size_t container_size = read_signal(container).size();

        if (!data->input_id.matches(container.value_id()))
        {
            data->mapped_items.resize(container_size);
            data->item_ids.resize(container_size);
            ++data->output_version;
            data->input_id.capture(container.value_id());
        }

        size_t valid_item_count = 0;
        auto captured_item = data->mapped_items.begin();
        auto captured_id = data->item_ids.begin();
        for_each(ctx, container, [&](context ctx, auto item) {
            auto mapped_item = f(ctx, item);
            if (signal_has_value(mapped_item))
            {
                if (!captured_id->matches(mapped_item.value_id()))
                {
                    *captured_item = read_signal(mapped_item);
                    captured_id->capture(mapped_item.value_id());
                    ++data->output_version;
                }
                ++valid_item_count;
            }
            ++captured_item;
            ++captured_id;
        });
        assert(captured_item == data->mapped_items.end());
        assert(captured_id == data->item_ids.end());

        all_items_have_values = (valid_item_count == container_size);
    }
    ALIA_END

    return mapped_sequence_signal<mapped_value_type>(
        *data, all_items_have_values);
}

// the map version...

template<class Key, class MappedItem>
struct mapped_map_data
{
    captured_id input_id;
    std::map<Key, MappedItem> mapped_items;
    std::vector<captured_id> item_ids;
    counter_type output_version = 0;
};

template<class Key, class MappedItem>
struct mapped_map_signal : signal<
                               mapped_map_signal<Key, MappedItem>,
                               std::map<Key, MappedItem>,
                               read_only_signal>
{
    mapped_map_signal(
        mapped_map_data<Key, MappedItem>& data, bool all_items_have_values)
        : data_(&data), all_items_have_values_(all_items_have_values)
    {
    }
    id_interface const&
    value_id() const
    {
        id_ = make_id(data_->output_version);
        return id_;
    }
    bool
    has_value() const
    {
        return all_items_have_values_;
    }
    std::map<Key, MappedItem> const&
    read() const
    {
        return data_->mapped_items;
    }

 private:
    mapped_map_data<Key, MappedItem>* data_;
    bool all_items_have_values_;
    mutable simple_id<counter_type> id_;
};

template<
    class Context,
    class Container,
    class Function,
    std::enable_if_t<
        is_map_like<typename Container::value_type>::value,
        int> = 0>
auto
transform(Context ctx, Container const& container, Function&& f)
{
    typedef typename Container::value_type::key_type key_type;
    typedef typename decltype(
        f(ctx,
          std::declval<readable<key_type>>(),
          std::declval<readable<
              typename Container::value_type::mapped_type>>()))::value_type
        mapped_value_type;

    mapped_map_data<key_type, mapped_value_type>* data;
    get_cached_data(ctx, &data);

    bool all_items_have_values = false;

    ALIA_IF(has_value(container))
    {
        size_t container_size = read_signal(container).size();

        if (!data->input_id.matches(container.value_id()))
        {
            // There's probably a less heavy-handed approach to this, but this
            // works for now.
            data->mapped_items.clear();
            data->item_ids.clear();
            data->item_ids.resize(container_size);
            ++data->output_version;
            data->input_id.capture(container.value_id());
        }

        size_t valid_item_count = 0;
        auto captured_id = data->item_ids.begin();
        for_each(ctx, container, [&](context ctx, auto key, auto value) {
            auto mapped_item = f(ctx, key, value);
            if (signal_has_value(mapped_item))
            {
                if (!captured_id->matches(mapped_item.value_id()))
                {
                    data->mapped_items[read_signal(key)]
                        = read_signal(mapped_item);
                    captured_id->capture(mapped_item.value_id());
                    ++data->output_version;
                }
                ++valid_item_count;
            }
            ++captured_id;
        });
        assert(captured_id == data->item_ids.end());

        all_items_have_values = (valid_item_count == container_size);
    }
    ALIA_END

    return mapped_map_signal<key_type, mapped_value_type>(
        *data, all_items_have_values);
}

} // namespace alia



// This file defines utilities for constructing custom signals via lambda
// functions.

namespace alia {

// lambda_constant(read) creates a read-only signal whose value is constant and
// is determined by calling :read.
template<class Value, class Read>
struct simple_lambda_constant_signal
    : signal<
          simple_lambda_constant_signal<Value, Read>,
          Value,
          read_only_signal>
{
    simple_lambda_constant_signal(Read read) : read_(read)
    {
    }
    bool
    has_value() const
    {
        return true;
    }
    id_interface const&
    value_id() const
    {
        return unit_id;
    }
    Value const&
    read() const
    {
        value_ = read_();
        return value_;
    }

 private:
    Read read_;
    mutable decltype(read_()) value_;
};
template<class Read>
auto
lambda_constant(Read read)
{
    return simple_lambda_constant_signal<std::decay_t<decltype(read())>, Read>(
        read);
}

// lambda_reader(read) creates a read-only signal whose value is determined by
// calling :read.
template<class Value, class Read>
struct simple_lambda_reader_signal
    : regular_signal<
          simple_lambda_reader_signal<Value, Read>,
          Value,
          read_only_signal>
{
    simple_lambda_reader_signal(Read read) : read_(read)
    {
    }
    bool
    has_value() const
    {
        return true;
    }
    Value const&
    read() const
    {
        value_ = read_();
        return value_;
    }

 private:
    Read read_;
    mutable decltype(read_()) value_;
};
template<class Read>
auto
lambda_reader(Read read)
{
    return simple_lambda_reader_signal<std::decay_t<decltype(read())>, Read>(
        read);
}

// lambda_reader(has_value, read) creates a read-only signal whose value is
// determined by calling :has_value and :read.
template<class Value, class HasValue, class Read>
struct lambda_reader_signal : regular_signal<
                                  lambda_reader_signal<Value, HasValue, Read>,
                                  Value,
                                  read_only_signal>
{
    lambda_reader_signal(HasValue has_value, Read read)
        : has_value_(has_value), read_(read)
    {
    }
    bool
    has_value() const
    {
        return has_value_();
    }
    Value const&
    read() const
    {
        value_ = read_();
        return value_;
    }

 private:
    HasValue has_value_;
    Read read_;
    mutable decltype(read_()) value_;
};
template<class HasValue, class Read>
auto
lambda_reader(HasValue has_value, Read read)
{
    return lambda_reader_signal<std::decay_t<decltype(read())>, HasValue, Read>(
        has_value, read);
}

// lambda_reader(has_value, read, generate_id) creates a read-only signal
// whose value is determined by calling :has_value and :read and whose ID is
// determined by calling :generate_id.
template<class Value, class HasValue, class Read, class GenerateId>
struct lambda_reader_signal_with_id
    : signal<
          lambda_reader_signal_with_id<Value, HasValue, Read, GenerateId>,
          Value,
          read_only_signal>
{
    lambda_reader_signal_with_id(
        HasValue has_value, Read read, GenerateId generate_id)
        : has_value_(has_value), read_(read), generate_id_(generate_id)
    {
    }
    id_interface const&
    value_id() const
    {
        id_ = generate_id_();
        return id_;
    }
    bool
    has_value() const
    {
        return has_value_();
    }
    Value const&
    read() const
    {
        value_ = read_();
        return value_;
    }

 private:
    HasValue has_value_;
    Read read_;
    mutable decltype(read_()) value_;
    GenerateId generate_id_;
    mutable decltype(generate_id_()) id_;
};
template<class HasValue, class Read, class GenerateId>
auto
lambda_reader(HasValue has_value, Read read, GenerateId generate_id)
{
    return lambda_reader_signal_with_id<
        std::decay_t<decltype(read())>,
        HasValue,
        Read,
        GenerateId>(has_value, read, generate_id);
}

// lambda_duplex(has_value, read, ready_to_write, write) creates a duplex signal
// whose value is read by calling :has_value and :read and written by calling
// :ready_to_write and :write.
template<
    class Value,
    class HasValue,
    class Read,
    class ReadyToWrite,
    class Write>
struct lambda_duplex_signal
    : regular_signal<
          lambda_duplex_signal<Value, HasValue, Read, ReadyToWrite, Write>,
          Value,
          duplex_signal>
{
    lambda_duplex_signal(
        HasValue has_value, Read read, ReadyToWrite ready_to_write, Write write)
        : has_value_(has_value),
          read_(read),
          ready_to_write_(ready_to_write),
          write_(write)
    {
    }
    bool
    has_value() const
    {
        return has_value_();
    }
    Value const&
    read() const
    {
        value_ = read_();
        return value_;
    }
    bool
    ready_to_write() const
    {
        return ready_to_write_();
    }
    void
    write(Value const& value) const
    {
        write_(value);
    }

 private:
    HasValue has_value_;
    Read read_;
    mutable decltype(read_()) value_;
    ReadyToWrite ready_to_write_;
    Write write_;
};
template<class HasValue, class Read, class ReadyToWrite, class Write>
auto
lambda_duplex(
    HasValue has_value, Read read, ReadyToWrite ready_to_write, Write write)
{
    return lambda_duplex_signal<
        std::decay_t<decltype(read())>,
        HasValue,
        Read,
        ReadyToWrite,
        Write>(has_value, read, ready_to_write, write);
}

// lambda_duplex(has_value, read, ready_to_write, write, generate_id) creates a
// duplex signal whose value is read by calling :has_value and :read and written
// by calling :ready_to_write and :write. Its ID is determined by calling
// :generate_id.
template<
    class Value,
    class HasValue,
    class Read,
    class ReadyToWrite,
    class Write,
    class GenerateId>
struct lambda_duplex_signal_with_id : signal<
                                          lambda_duplex_signal_with_id<
                                              Value,
                                              HasValue,
                                              Read,
                                              ReadyToWrite,
                                              Write,
                                              GenerateId>,
                                          Value,
                                          duplex_signal>
{
    lambda_duplex_signal_with_id(
        HasValue has_value,
        Read read,
        ReadyToWrite ready_to_write,
        Write write,
        GenerateId generate_id)
        : has_value_(has_value),
          read_(read),
          ready_to_write_(ready_to_write),
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
    has_value() const
    {
        return has_value_();
    }
    Value const&
    read() const
    {
        value_ = read_();
        return value_;
    }
    bool
    ready_to_write() const
    {
        return ready_to_write_();
    }
    void
    write(Value const& value) const
    {
        write_(value);
    }

 private:
    HasValue has_value_;
    Read read_;
    mutable decltype(read_()) value_;
    ReadyToWrite ready_to_write_;
    Write write_;
    GenerateId generate_id_;
    mutable decltype(generate_id_()) id_;
};
template<
    class HasValue,
    class Read,
    class ReadyToWrite,
    class Write,
    class GenerateId>
auto
lambda_duplex(
    HasValue has_value,
    Read read,
    ReadyToWrite ready_to_write,
    Write write,
    GenerateId generate_id)
{
    return lambda_duplex_signal_with_id<
        std::decay_t<decltype(read())>,
        HasValue,
        Read,
        ReadyToWrite,
        Write,
        GenerateId>(has_value, read, ready_to_write, write, generate_id);
}

// This is just a clear and concise way of indicating that a lambda signal
// always has a value.
inline bool
always_has_value()
{
    return true;
}

// This is just a clear and concise way of indicating that a lambda signal is
// always ready to write.
inline bool
always_ready()
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
    has_value() const
    {
        return n_.has_value() && scale_factor_.has_value();
    }
    typename N::value_type const&
    read() const
    {
        return lazy_reader_.read(
            [&] { return n_.read() * scale_factor_.read(); });
    }
    bool
    ready_to_write() const
    {
        return n_.ready_to_write() && scale_factor_.has_value();
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
make_scaled_signal(N n, Factor scale_factor)
{
    return scaled_signal<N, Factor>(n, scale_factor);
}
template<class N, class Factor>
auto
scale(N n, Factor scale_factor)
{
    return make_scaled_signal(n, signalize(scale_factor));
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
    has_value() const
    {
        return n_.has_value() && offset_.has_value();
    }
    typename N::value_type const&
    read() const
    {
        return lazy_reader_.read([&] { return n_.read() + offset_.read(); });
    }
    bool
    ready_to_write() const
    {
        return n_.ready_to_write() && offset_.has_value();
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
make_offset_signal(N n, Offset offset)
{
    return offset_signal<N, Offset>(n, offset);
}
template<class N, class Offset>
auto
offset(N n, Offset offset)
{
    return make_offset_signal(n, signalize(offset));
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
    has_value() const
    {
        return n_.has_value();
    }
    typename N::value_type const&
    read() const
    {
        return n_.read();
    }
    bool
    ready_to_write() const
    {
        return n_.ready_to_write() && step_.has_value();
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
make_rounding_signal_wrapper(N n, Step step)
{
    return rounding_signal_wrapper<N, Step>(n, step);
}
template<class N, class Step>
auto
round_signal_writes(N n, Step step)
{
    return make_rounding_signal_wrapper(n, signalize(step));
}

} // namespace alia



namespace alia {

// state_storage<Value> is designed to be stored persistently within the
// component tree to represent application state or other data that needs to be
// tracked similarly. It contains a 'version' number that counts changes and
// serves as a signal value ID, and it also takes care of mark the component
// tree as 'dirty' when it's updated.
template<class Value>
struct state_storage
{
    state_storage() : version_(0)
    {
    }

    explicit state_storage(Value value) : value_(std::move(value)), version_(1)
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
        handle_change();
    }

    // If you REALLY need direct, non-const access to the underlying state,
    // you can use this. It returns a non-const reference to the value and
    // increments the version number (assuming you'll make some changes).
    //
    // Note that you should be careful to use this atomically. In other words,
    // call this to get a reference, do your update, and then discard the
    // reference before anyone else observes the state. If you hold onto the
    // reference and continue making changes while other alia code is accessing
    // it, they'll end up with outdated views of the state.
    //
    // Also note that if you call this on an uninitialized state, you're
    // expected to initialize it.
    //
    Value&
    nonconst_ref()
    {
        handle_change();
        return value_;
    }

    // This is even less safe. It's like above, but any changes you make will
    // NOT be marked in the component tree, so you should only use this if you
    // know it's safe to do so.
    Value&
    untracked_nonconst_ref()
    {
        ++version_;
        return value_;
    }

    // Update the container that the state is part of.
    void
    refresh_container(component_container_ptr const& container)
    {
        container_ = container;
    }

 private:
    void
    handle_change()
    {
        ++version_;
        mark_dirty_component(container_);
    }

    Value value_;
    // version_ is incremented for each change in the value of the state.
    // If this is 0, the state is considered uninitialized.
    unsigned version_;
    component_container_ptr container_;
};

template<class Value, class Direction>
struct state_signal : signal<state_signal<Value, Direction>, Value, Direction>
{
    explicit state_signal(state_storage<Value>* data) : data_(data)
    {
    }

    bool
    has_value() const
    {
        return data_->is_initialized();
    }

    Value const&
    read() const
    {
        return data_->get();
    }

    simple_id<unsigned> const&
    value_id() const
    {
        id_ = make_id(data_->version());
        return id_;
    }

    bool
    ready_to_write() const
    {
        return true;
    }

    void
    write(Value const& value) const
    {
        data_->set(value);
    }

 private:
    state_storage<Value>* data_;
    mutable simple_id<unsigned> id_;
};

template<class Value>
state_signal<Value, duplex_signal>
make_state_signal(state_storage<Value>& data)
{
    return state_signal<Value, duplex_signal>(&data);
}

// get_state(ctx, initial_value) returns a signal carrying some persistent local
// state whose initial value is determined by the :initial_value signal. The
// returned signal will not have a value until :initial_value has one or one is
// explicitly written to the state signal.
template<class Context, class InitialValue>
auto
get_state(Context ctx, InitialValue const& initial_value)
{
    auto initial_value_signal = signalize(initial_value);

    state_storage<typename decltype(initial_value_signal)::value_type>* state;
    get_data(ctx, &state);

    on_refresh(ctx, [&](auto ctx) {
        state->refresh_container(get_active_component_container(ctx));
        if (!state->is_initialized() && signal_has_value(initial_value_signal))
            state->untracked_nonconst_ref() = read_signal(initial_value_signal);
    });

    return make_state_signal(*state);
}

} // namespace alia



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

inline char const*
make_printf_friendly(std::string const& x)
{
    return x.c_str();
}

struct printf_format_error : exception
{
    printf_format_error() : exception("printf format error")
    {
    }
};

template<class... Args>
std::string
invoke_snprintf(std::string const& format, Args const&... args)
{
    int length
        = std::snprintf(0, 0, format.c_str(), make_printf_friendly(args)...);
    if (length < 0)
        throw printf_format_error();
    std::string s;
    if (length > 0)
    {
        s.resize(length);
        std::snprintf(
            &s[0], length + 1, format.c_str(), make_printf_friendly(args)...);
    }
    return s;
}

template<class Format, class... Args>
auto
printf(context ctx, Format format, Args... args)
{
    return apply(
        ctx,
        ALIA_LAMBDIFY(invoke_snprintf),
        signalize(format),
        signalize(args)...);
}

// All conversion of values to and from text goes through the functions
// from_string and to_string. In order to use a particular value type with
// the text-based widgets and utilities provided here, that type must
// implement these functions.

#define ALIA_DECLARE_STRING_CONVERSIONS(T)                                     \
    void from_string(T* value, std::string const& s);                          \
    std::string to_string(T value);

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
update_text_conversion(keyed_data<std::string>* data, Readable x)
{
    if (signal_has_value(x))
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
keyed_data_signal<std::string>
as_text(context ctx, Readable x)
{
    keyed_data<std::string>* data;
    get_cached_data(ctx, &data);
    update_text_conversion(data, x);
    return keyed_data_signal<std::string>(data);
}

// as_duplex_text(ctx, x) is similar to as_text but it's duplex.
template<class Value>
struct duplex_text_data
{
    captured_id input_id;
    Value input_value;
    bool output_valid = false;
    std::string output_text;
    counter_type output_version = 1;
};
template<class Value, class Readable>
void
update_duplex_text(duplex_text_data<Value>* data, Readable x)
{
    if (signal_has_value(x))
    {
        auto const& input_id = x.value_id();
        if (!data->input_id.matches(input_id))
        {
            if (!data->output_valid || read_signal(x) != data->input_value)
            {
                data->input_value = read_signal(x);
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
template<class Wrapped>
struct duplex_text_signal : signal<
                                duplex_text_signal<Wrapped>,
                                std::string,
                                typename Wrapped::direction_tag>
{
    duplex_text_signal(
        Wrapped wrapped, duplex_text_data<typename Wrapped::value_type>* data)
        : wrapped_(wrapped), data_(data)
    {
    }
    bool
    has_value() const
    {
        return data_->output_valid;
    }
    std::string const&
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
    ready_to_write() const
    {
        return wrapped_.ready_to_write();
    }
    void
    write(std::string const& s) const
    {
        typename Wrapped::value_type value;
        from_string(&value, s);
        data_->input_value = value;
        wrapped_.write(value);
        data_->output_text = s;
        ++data_->output_version;
    }

 private:
    Wrapped wrapped_;
    duplex_text_data<typename Wrapped::value_type>* data_;
    mutable simple_id<counter_type> id_;
};
template<class Signal>
duplex_text_signal<Signal>
as_duplex_text(context ctx, Signal x)
{
    duplex_text_data<typename Signal::value_type>* data;
    get_cached_data(ctx, &data);
    update_duplex_text(data, x);
    return duplex_text_signal<Signal>(x, data);
}

} // namespace alia






namespace alia {

// Currently, alia's only sense of time is that of a monotonically increasing
// millisecond counter. It's understood to have an arbitrary start point and is
// allowed to wrap around, so 'unsigned' is considered sufficient.
typedef unsigned millisecond_count;

struct timing_subsystem
{
    millisecond_count tick_counter = 0;
};

// Request that the UI context refresh again quickly enough for smooth
// animation.
void
schedule_animation_refresh(dataless_context ctx);

// Get the value of the millisecond tick counter associated with the given
// UI context. This counter is updated every refresh pass, so it's consistent
// within a single frame.
// When this is called, it's assumed that something is currently animating, so
// it also requests a refresh.
millisecond_count
get_raw_animation_tick_count(dataless_context ctx);

// Same as above, but returns a signal rather than a raw integer.
value_signal<millisecond_count>
get_animation_tick_count(dataless_context ctx);

// Get the number of ticks remaining until the given end time.
// If the time has passed, this returns 0.
// This ensures that the UI context refreshes until the end time is reached.
millisecond_count
get_raw_animation_ticks_left(dataless_context ctx, millisecond_count end_tick);

struct animation_timer_state
{
    bool active = false;
    millisecond_count end_tick;
};

struct raw_animation_timer
{
    raw_animation_timer(context ctx) : ctx_(ctx)
    {
        get_cached_data(ctx, &state_);
        update();
    }
    raw_animation_timer(dataless_context ctx, animation_timer_state& state)
        : ctx_(ctx), state_(&state)
    {
        update();
    }
    bool
    is_active() const
    {
        return state_->active;
    }
    millisecond_count
    ticks_left() const
    {
        return ticks_left_;
    }
    void
    start(millisecond_count duration)
    {
        state_->active = true;
        state_->end_tick = get_raw_animation_tick_count(ctx_) + duration;
    }

 private:
    void
    update()
    {
        if (state_->active)
        {
            ticks_left_ = get_raw_animation_ticks_left(ctx_, state_->end_tick);
            if (ticks_left_ == 0)
                state_->active = false;
        }
        else
        {
            ticks_left_ = 0;
        }
    }

    dataless_context ctx_;
    animation_timer_state* state_;
    millisecond_count ticks_left_;
};

struct animation_timer
{
    animation_timer(context ctx) : raw_(ctx)
    {
    }
    animation_timer(dataless_context ctx, animation_timer_state& state)
        : raw_(ctx, state)
    {
    }
    auto
    is_active() const
    {
        return value(raw_.is_active());
    }
    auto
    ticks_left() const
    {
        return value(raw_.ticks_left());
    }
    auto
    start()
    {
        return lambda_action(
            [&](millisecond_count duration) { raw_.start(duration); });
    }

 private:
    raw_animation_timer raw_;
};

} // namespace alia



// This file defines the default implementation for tracking timer events.

namespace alia {

struct timer_event_request
{
    millisecond_count trigger_time;
    external_component_id component;
    unsigned frame_issued;
};

struct timer_event_scheduler
{
    std::vector<timer_event_request> requests;
    unsigned frame_counter = 0;
};

// Schedule an event.
void
schedule_event(
    timer_event_scheduler& scheduler,
    external_component_id component,
    millisecond_count time);

// Issue any events that are ready to be issued.
void
issue_ready_events(
    timer_event_scheduler& scheduler,
    millisecond_count now,
    function_view<void(
        external_component_id component, millisecond_count time)> const& issue);

// Are there any scheduled events?
bool
has_scheduled_events(timer_event_scheduler const& scheduler);

// Get the time until the next scheduled event.
// The behavior is undefined if there are no scheduled events.
millisecond_count
get_time_until_next_event(
    timer_event_scheduler& scheduler, millisecond_count now);

} // namespace alia


namespace alia {

struct system;

struct external_interface
{
    virtual ~external_interface()
    {
    }

    // Get the current value of the system's millisecond tick counter.
    // (The default implementation uses std::chrono::steady_clock.)
    virtual millisecond_count
    get_tick_count() const = 0;

    // alia calls this every frame when an animation is in progress that's using
    // alia's internal animation timing system.
    //
    // If this system isn't already refreshing continuously, this requests that
    // it refresh again within a reasonable animation time frame.
    //
    // (You can also call system_needs_refresh (see below) to test if a refresh
    // is necessary.)
    //
    virtual void
    schedule_animation_refresh()
        = 0;

    // Schedule a timer event to be delivered to a specific component at some
    // future time.
    //
    // :id is the ID of the component that the event should be delivered to.
    //
    // :time is the tick count at which the event should be delivered.
    //
    // alia provides an internal system for tracking outstanding requests for
    // timer events. If this system is continuously updating anyway, you can
    // leave this unimplemented and call process_internal_timing_events
    // once per frame to handle timing events. (See below.)
    //
    virtual void
    schedule_timer_event(
        external_component_id component, millisecond_count time)
        = 0;
};

struct default_external_interface : external_interface
{
    system& owner;

    default_external_interface(system& owner) : owner(owner)
    {
    }

    virtual millisecond_count
    get_tick_count() const;

    void
    schedule_animation_refresh()
    {
    }

    void
    schedule_timer_event(
        external_component_id component, millisecond_count time);
};

struct system : noncopyable
{
    data_graph data;
    std::function<void(context)> controller;
    bool refresh_needed = false;
    std::unique_ptr<external_interface> external;
    timer_event_scheduler scheduler;
    component_container_ptr root_component;
};

void
initialize_system(
    system& sys,
    std::function<void(context)> const& controller,
    external_interface* external = nullptr);

// timer event
struct timer_event : targeted_event
{
    millisecond_count trigger_time;
};

// If this system is using internal timer event scheduling, this will check for
// any events that are ready to be issued and issue them.
void
process_internal_timing_events(system& sys, millisecond_count now);

} // namespace alia


namespace alia {

// unit_cubic_bezier represents a cubic bezier whose end points are (0, 0)
// and (1, 1).
struct unit_cubic_bezier
{
    double p1x, p1y, p2x, p2y;
};

// unit_cubic_bezier_coefficients describes a unit cubic bezier curve by the
// coefficients in its parametric equation form.
struct unit_cubic_bezier_coefficients
{
    double ax, ay, bx, by, cx, cy;
};

unit_cubic_bezier_coefficients
compute_curve_coefficients(unit_cubic_bezier const& bezier);

// Solve for t at a point x in a unit cubic curve.
// This should only be called on curves that are expressible in y = f(x) form.
double
solve_for_t_at_x(
    unit_cubic_bezier_coefficients const& coeff,
    double x,
    double error_tolerance);

// This is the same as above but always uses a bisection search.
// It's simple but likely slower and is primarily exposed for testing purposes.
double
solve_for_t_at_x_with_bisection_search(
    unit_cubic_bezier_coefficients const& coeff,
    double x,
    double error_tolerance);

// Evaluate a unit_cubic_bezier at the given x value.
// Since this is an approximation, the caller must specify a tolerance value
// to control the error in the result.
double
eval_curve_at_x(
    unit_cubic_bezier const& curve, double x, double error_tolerance);

} // namespace alia




// This file defines the interface to timer events in alia.

namespace alia {

struct timer_data
{
    bool active = false;
    component_identity identity;
    millisecond_count expected_trigger_time;
};

struct raw_timer
{
    raw_timer(context ctx) : ctx_(ctx)
    {
        get_cached_data(ctx, &data_);
        update();
    }
    raw_timer(dataless_context ctx, timer_data& data) : ctx_(ctx), data_(&data)
    {
        update();
    }

    bool
    is_active() const
    {
        return data_->active;
    }

    void
    start(millisecond_count duration);

    void
    stop()
    {
        data_->active = false;
    }

    bool
    is_triggered()
    {
        return triggered_;
    }

 private:
    void
    update();

    dataless_context ctx_;
    timer_data* data_;
    bool triggered_;
};

} // namespace alia


namespace alia {

// The following is a small utility used for deflickering...

template<class Value>
struct captured_value
{
    bool valid = false;
    Value value;
    captured_id id;
};

template<class Value>
void
clear(captured_value<Value>& captured)
{
    captured.valid = false;
    captured.id.clear();
}

template<class Value, class Signal>
void
capture(captured_value<Value>& captured, Signal const& signal)
{
    captured.value = signal.read();
    captured.id.capture(signal.value_id());
    captured.valid = true;
}

// deflicker(ctx, x, delay) returns a deflickered version of the signal :x.
//
// Whenever :x has a value, the deflickered signal carries the same value.
// Whenever :x loses its value, the deflickered signal retains the old value for
// a period of time, given by :delay. (If a new value arrives on :x during this
// period, it will pick that up instead.)
//
// :delay is specified in milliseconds and can be either a raw value or a
// signal. It defaults to 250 ms.
//

unsigned const default_deflicker_delay = 250;

template<class Value>
struct deflickering_data
{
    timer_data timer;
    captured_value<Value> captured;
};

template<class Wrapped>
struct deflickering_signal : signal<
                                 deflickering_signal<Wrapped>,
                                 typename Wrapped::value_type,
                                 typename Wrapped::direction_tag>
{
    deflickering_signal()
    {
    }
    deflickering_signal(
        Wrapped wrapped, captured_value<typename Wrapped::value_type>& captured)
        : wrapped_(wrapped), captured_(captured)
    {
    }
    bool
    has_value() const
    {
        return captured_.valid;
    }
    typename Wrapped::value_type const&
    read() const
    {
        return captured_.value;
    }
    id_interface const&
    value_id() const
    {
        return captured_.id.is_initialized() ? captured_.id.get() : null_id;
    }
    bool
    ready_to_write() const
    {
        return wrapped_.ready_to_write();
    }
    void
    write(typename Wrapped::value_type const& value) const
    {
        wrapped_.write(value);
    }

 private:
    Wrapped wrapped_;
    captured_value<typename Wrapped::value_type>& captured_;
};

template<class Signal, class Value, class Delay = millisecond_count>
deflickering_signal<Signal>
deflicker(
    dataless_context ctx,
    deflickering_data<Value>& data,
    Signal x,
    Delay delay = default_deflicker_delay)
{
    auto delay_signal = signalize(delay);

    raw_timer timer(ctx, data.timer);
    if (timer.is_triggered())
    {
        // If the timer is triggered, it means we were holding a stale value and
        // it's time to clear it out.
        clear(data.captured);
        abort_traversal(ctx);
    }

    on_refresh(ctx, [&](auto) {
        if (x.has_value())
        {
            if (x.value_id() != data.captured.id)
            {
                // :x is carrying a different value than the one we have
                // captured, so capture the new one.
                capture(data.captured, x);
                // If we had an active timer, stop it.
                timer.stop();
            }
        }
        else
        {
            if (data.captured.id.is_initialized() && !timer.is_active())
            {
                // :x has no value, and this is apparently the first time we've
                // noticed that, so start the timer.
                if (signal_has_value(delay_signal))
                {
                    timer.start(read_signal(delay_signal));
                }
                else
                {
                    // If the delay isn't readable, we can't start the timer, so
                    // just drop the value immediately.
                    clear(data.captured);
                }
            }
        }
    });

    return deflickering_signal<Signal>(x, data.captured);
}

template<class Signal, class Delay = millisecond_count>
deflickering_signal<Signal>
deflicker(context ctx, Signal x, Delay delay = default_deflicker_delay)
{
    typedef typename Signal::value_type value_type;
    auto& data = get_cached_data<deflickering_data<value_type>>(ctx);
    return deflicker(ctx, data, x, delay);
}

} // namespace alia




namespace alia {

// The following are interpolation curves that can be used for animations.
typedef unit_cubic_bezier animation_curve;
animation_curve const default_curve = {0.25, 0.1, 0.25, 1};
animation_curve const linear_curve = {0, 0, 1, 1};
animation_curve const ease_in_curve = {0.42, 0, 1, 1};
animation_curve const ease_out_curve = {0, 0, 0.58, 1};
animation_curve const ease_in_out_curve = {0.42, 0, 0.58, 1};

// animated_transition specifies an animated transition from one state to
// another, defined by a duration and a curve to follow.
struct animated_transition
{
    animation_curve curve;
    millisecond_count duration;
};
animated_transition const default_transition = {default_curve, 250};

// A value_smoother is used to create smoothly changing views of values that
// actually change abruptly.
template<class Value>
struct value_smoother
{
    bool initialized = false, in_transition;
    millisecond_count duration, transition_end;
    Value old_value, new_value;
};

// value_smoother requires the ability to interpolate the values it works with.
// If the value type supplies addition and multiplication by scalers, then it
// can be interpolated using the default implementation below. Another option
// is to simply implement a compatible interpolate function directly for the
// value type.

// interpolate(a, b, factor) yields a * (1 - factor) + b * factor
template<class Value>
std::enable_if_t<!std::is_integral<Value>::value, Value>
interpolate(Value const& a, Value const& b, double factor)
{
    return a * (1 - factor) + b * factor;
}
// Overload it for floats to eliminate warnings about conversions.
static inline float
interpolate(float a, float b, double factor)
{
    return float(a * (1 - factor) + b * factor);
}
// Overload it for integers to add rounding (and eliminate warnings).
template<class Integer>
std::enable_if_t<std::is_integral<Integer>::value, Integer>
interpolate(Integer a, Integer b, double factor)
{
    return Integer(std::round(a * (1 - factor) + b * factor));
}

// reset_smoothing(smoother, value) causes the smoother to transition abruptly
// to the value specified.
template<class Value>
void
reset_smoothing(value_smoother<Value>& smoother, Value const& value)
{
    smoother.in_transition = false;
    smoother.new_value = value;
    smoother.initialized = true;
}

// smooth_raw(ctx, smoother, x, transition) returns a smoothed view of x.
template<class Value>
Value
smooth_raw(
    dataless_context ctx,
    value_smoother<Value>& smoother,
    Value const& x,
    animated_transition const& transition = default_transition)
{
    if (!smoother.initialized)
        reset_smoothing(smoother, x);
    Value current_value = smoother.new_value;
    if (smoother.in_transition)
    {
        millisecond_count ticks_left
            = get_raw_animation_ticks_left(ctx, smoother.transition_end);
        if (ticks_left > 0)
        {
            double fraction = eval_curve_at_x(
                transition.curve,
                1. - double(ticks_left) / smoother.duration,
                1. / smoother.duration);
            current_value
                = interpolate(smoother.old_value, smoother.new_value, fraction);
        }
        else
            smoother.in_transition = false;
    }
    if (is_refresh_event(ctx) && x != smoother.new_value)
    {
        smoother.duration =
            // If we're just going back to the old value, go back in the same
            // amount of time it took to get here.
            smoother.in_transition && x == smoother.old_value
                ? (transition.duration
                   - get_raw_animation_ticks_left(ctx, smoother.transition_end))
                : transition.duration;
        smoother.transition_end
            = get_raw_animation_tick_count(ctx) + smoother.duration;
        smoother.old_value = current_value;
        smoother.new_value = x;
        smoother.in_transition = true;
    }
    return current_value;
}

// smooth is analogous to smooth_raw, but it deals with signals instead of raw
// values.

template<class Wrapped>
struct smoothed_signal : regular_signal<
                             smoothed_signal<Wrapped>,
                             typename Wrapped::value_type,
                             read_only_signal>
{
    smoothed_signal(
        Wrapped wrapped, typename Wrapped::value_type smoothed_value)
        : wrapped_(wrapped), smoothed_value_(smoothed_value)
    {
    }
    id_interface const&
    value_id() const
    {
        if (wrapped_.has_value())
        {
            id_ = make_id(smoothed_value_);
            return id_;
        }
        else
            return null_id;
    }
    bool
    has_value() const
    {
        return wrapped_.has_value();
    }
    typename Wrapped::value_type const&
    read() const
    {
        return smoothed_value_;
    }

 private:
    Wrapped wrapped_;
    typename Wrapped::value_type smoothed_value_;
    mutable simple_id<typename Wrapped::value_type> id_;
};
template<class Wrapped>
smoothed_signal<Wrapped>
make_smoothed_signal(
    Wrapped wrapped, typename Wrapped::value_type smoothed_value)
{
    return smoothed_signal<Wrapped>(wrapped, smoothed_value);
}

template<class Value, class Signal>
auto
smooth(
    dataless_context ctx,
    value_smoother<Value>& smoother,
    Signal x,
    animated_transition const& transition = default_transition)
{
    Value output = Value();
    if (signal_has_value(x))
        output = smooth_raw(ctx, smoother, read_signal(x), transition);
    return make_smoothed_signal(x, output);
}

template<class Signal>
auto
smooth(
    context ctx,
    Signal x,
    animated_transition const& transition = default_transition)
{
    value_smoother<typename Signal::value_type>* data;
    get_cached_data(ctx, &data);
    return smooth(ctx, *data, x, transition);
}

} // namespace alia



namespace alia {

namespace impl {

value_signal<bool>
square_wave(
    context ctx,
    readable<millisecond_count> true_duration,
    readable<millisecond_count> false_duration);

}

// Generates a square wave.
//
// The returned signal alternates between true and false as time passes.
//
// The two duration parameters specify how long (in milliseconds) the signal
// remains at each value during a single cycle.
//
// Both durations can be either signals or raw values.
//
// If :false_duration doesn't have a value (or is omitted), :true_duration is
// used in its place.
//
// If :true_duration doesn't have a value, the square wave will stop
// alternating.
//
template<
    class TrueDuration,
    class FalseDuration = empty_signal<millisecond_count>>
value_signal<bool>
square_wave(
    context ctx,
    TrueDuration true_duration,
    FalseDuration false_duration = empty<millisecond_count>())
{
    return impl::square_wave(
        ctx,
        signal_cast<millisecond_count>(signalize(true_duration)),
        signal_cast<millisecond_count>(signalize(false_duration)));
}

} // namespace alia

#ifdef ALIA_IMPLEMENTATION
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


namespace alia {

context
make_context(
    context_storage* storage,
    system& sys,
    event_traversal& event,
    data_traversal& data,
    timing_subsystem& timing)
{
    return make_context(impl::make_empty_structural_collection(storage))
        .add<system_tag>(sys)
        .add<event_traversal_tag>(event)
        .add<timing_tag>(timing)
        .add<data_traversal_tag>(data);
}

} // namespace alia

namespace alia {

void
mark_dirty_component(component_container_ptr const& container)
{
    component_container* c = container.get();
    while (c && !c->dirty)
    {
        c->dirty = true;
        c = c->parent.get();
    }
}

void
mark_dirty_component(dataless_context ctx)
{
    event_traversal& traversal = get_event_traversal(ctx);
    mark_dirty_component(*traversal.active_container);
}

void
mark_animating_component(component_container_ptr const& container)
{
    component_container* r = container.get();
    while (r && !r->animating)
    {
        r->animating = true;
        r = r->parent.get();
    }
}

void
mark_animating_component(dataless_context ctx)
{
    event_traversal& traversal = get_event_traversal(ctx);
    mark_animating_component(*traversal.active_container);
}

void
scoped_component_container::begin(
    dataless_context ctx, component_container_ptr* container)
{
    event_traversal& traversal = get_event_traversal(ctx);

    ctx_.reset(ctx);

    container_ = container;

    if (traversal.active_container)
    {
        if ((*container)->parent != *traversal.active_container)
            (*container)->parent = *traversal.active_container;
    }
    else
        (*container)->parent.reset();

    parent_ = traversal.active_container;
    traversal.active_container = container;

    is_dirty_ = (*container)->dirty;
    (*container)->dirty = false;

    is_animating_ = (*container)->animating;
    (*container)->animating = false;

    if (traversal.targeted)
    {
        if (traversal.path_to_target
            && traversal.path_to_target->node == container->get())
        {
            traversal.path_to_target = traversal.path_to_target->rest;
            is_on_route_ = true;
        }
        else
            is_on_route_ = false;
    }
    else
        is_on_route_ = true;
}

void
scoped_component_container::begin(context ctx)
{
    component_container_ptr* container;
    if (get_data(ctx, &container))
        container->reset(new component_container);

    this->begin(ctx, container);
}

void
scoped_component_container::end()
{
    if (ctx_)
    {
        auto ctx = *ctx_;
        get_event_traversal(ctx).active_container = parent_;
        ctx_.reset();
    }
}

} // namespace alia

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
                        node->block.clear_cache();
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
            ref.node->block.clear_cache();
        ref.active = false;
    }
}

static void
delete_named_block_ref_list(named_block_ref_node* head)
{
    if (head)
    {
        delete_named_block_ref_list(head->next);
        delete head;
    }
}

// Clear node caches in reverse order to match general C++ semantics.
static void
clear_data_node_caches(data_node* node)
{
	if (node)
	{
		clear_data_node_caches(node->next);
		node->clear_cache();
	}
}

// Same with named_block_ref_nodes.
static void
deactivate_ref_nodes(named_block_ref_node* node)
{
	if (node)
	{
		deactivate_ref_nodes(node->next);
		deactivate(*node);
	}
}

void
data_block::clear_cache()
{
    if (!this->cache_clear)
    {
		clear_data_node_caches(this->nodes);
		deactivate_ref_nodes(this->named_blocks);
        this->cache_clear = true;
    }
}

data_block::~data_block()
{
    clear_data_block(*this);
}

// Delete nodes in reverse order to match general C++ semantics.
static void
clear_data_nodes(data_node* node)
{
    if (node)
    {
        clear_data_nodes(node->next);
        delete node;
    }
}

void
clear_data_block(data_block& block)
{
    // This isn't strictly necessary, but it gives nodes a chance to remove
    // themselves in a more explicit manner before they find themselves in a
    // state of destruction.
    block.clear_cache();

    clear_data_nodes(block.nodes);
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

static void
invoke_controller(system& sys, event_traversal& events)
{
    bool is_refresh = (events.event_type == &typeid(refresh_event));

    data_traversal data;
    scoped_data_traversal sdt(sys.data, data);
    // Only use refresh events to decide when data is no longer needed.
    data.gc_enabled = data.cache_clearing_enabled = is_refresh;

    timing_subsystem timing;
    timing.tick_counter = sys.external->get_tick_count();

    context_storage storage;
    context ctx = make_context(&storage, sys, events, data, timing);

    scoped_component_container root(ctx, &sys.root_component);

    sys.controller(ctx);
}

namespace impl {

static void
route_event_(
    system& sys, event_traversal& traversal, component_container* target)
{
    // In order to construct the path to the target, we start at the target
    // and follow the 'parent' pointers until we reach the root. We do this
    // via recursion so that the path can be constructed entirely on the
    // stack.
    if (target)
    {
        event_routing_path path_node;
        path_node.rest = traversal.path_to_target;
        path_node.node = target;
        traversal.path_to_target = &path_node;
        route_event_(sys, traversal, target->parent.get());
    }
    else
    {
        invoke_controller(sys, traversal);
    }
}

void
route_event(
    system& sys, event_traversal& traversal, component_container* target)
{
    try
    {
        route_event_(sys, traversal, target);
    }
    catch (traversal_abortion&)
    {
    }
}

} // namespace impl

void
abort_traversal(dataless_context ctx)
{
    assert(!is_refresh_event(ctx));
    get_event_traversal(ctx).aborted = true;
    throw traversal_abortion();
}

void
refresh_component_identity(dataless_context ctx, component_identity& identity)
{
    auto const& active_container = get_active_component_container(ctx);
    if (identity != active_container)
        identity = active_container;
}

component_id
get_component_id(context ctx)
{
    component_id id;
    get_cached_data(ctx, &id);
    on_refresh(ctx, [&](auto ctx) { refresh_component_identity(ctx, *id); });
    return id;
}

struct initialization_detection
{
    bool initialized = false;
};

void
on_init(context ctx, action<> on_init)
{
    initialization_detection& data = get_data<initialization_detection>(ctx);
    on_refresh(ctx, [&](auto) {
        if (!data.initialized && on_init.is_ready())
        {
            perform_action(on_init);
            data.initialized = true;
        }
    });
}

void
on_activate(context ctx, action<> on_activate)
{
    initialization_detection& data
        = get_cached_data<initialization_detection>(ctx);
    on_refresh(ctx, [&](auto) {
        if (!data.initialized && on_activate.is_ready())
        {
            perform_action(on_activate);
            data.initialized = true;
        }
    });
}

} // namespace alia

namespace alia {

if_block::if_block(data_traversal& traversal, bool condition)
{
    data_block* block;
    get_data_node(traversal, &block);
    if (condition)
    {
        scoped_data_block_.begin(traversal, *block);
    }
    else if (traversal.cache_clearing_enabled)
    {
        block->clear_cache();
    }
}

loop_block::loop_block(data_traversal& traversal)
{
    traversal_ = &traversal;
    get_data(traversal, &block_);
}
loop_block::~loop_block()
{
    // block_ stores the data block we were expecting to use for the next
    // iteration (and, indirectly, all subsequent iterations), but since the
    // destructor is being invoked, there won't be a next iteration, which means
    // we should clear out that block.
    if (!std::uncaught_exception())
        clear_data_block(*block_);
}
void
loop_block::next()
{
    get_data(*traversal_, &block_);
}

event_dependent_if_block::event_dependent_if_block(
    data_traversal& traversal, bool condition)
{
    data_block* block;
    get_data_node(traversal, &block);
    if (condition)
    {
        scoped_data_block_.begin(traversal, *block);
    }
}

} // namespace alia


namespace alia {

template<class T>
bool
string_to_value(std::string const& str, T* value)
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
std::string
value_to_string(T const& value)
{
    std::ostringstream s;
    s << value;
    return s.str();
}

template<class T>
void
float_from_string(T* value, std::string const& str)
{
    if (!string_to_value(str, value))
        throw validation_error("This input expects a number.");
}

#define ALIA_FLOAT_CONVERSIONS(T)                                              \
    void from_string(T* value, std::string const& str)                         \
    {                                                                          \
        float_from_string(value, str);                                         \
    }                                                                          \
    std::string to_string(T value)                                             \
    {                                                                          \
        return value_to_string(value);                                         \
    }

ALIA_FLOAT_CONVERSIONS(float)
ALIA_FLOAT_CONVERSIONS(double)

template<class T>
void
signed_integer_from_string(T* value, std::string const& str)
{
    long long n;
    if (!string_to_value(str, &n))
        throw validation_error("This input expects an integer.");
    T x = T(n);
    if (x != n)
        throw validation_error("This integer is outside the supported range.");
    *value = x;
}

template<class T>
void
unsigned_integer_from_string(T* value, std::string const& str)
{
    unsigned long long n;
    if (!string_to_value(str, &n))
        throw validation_error("This input expects an integer.");
    T x = T(n);
    if (x != n)
        throw validation_error("This integer is outside the supported range.");
    *value = x;
}

#define ALIA_SIGNED_INTEGER_CONVERSIONS(T)                                     \
    void from_string(T* value, std::string const& str)                         \
    {                                                                          \
        signed_integer_from_string(value, str);                                \
    }                                                                          \
    std::string to_string(T value)                                             \
    {                                                                          \
        return value_to_string(value);                                         \
    }

#define ALIA_UNSIGNED_INTEGER_CONVERSIONS(T)                                   \
    void from_string(T* value, std::string const& str)                         \
    {                                                                          \
        unsigned_integer_from_string(value, str);                              \
    }                                                                          \
    std::string to_string(T value)                                             \
    {                                                                          \
        return value_to_string(value);                                         \
    }

ALIA_SIGNED_INTEGER_CONVERSIONS(short int)
ALIA_UNSIGNED_INTEGER_CONVERSIONS(unsigned short int)
ALIA_SIGNED_INTEGER_CONVERSIONS(int)
ALIA_UNSIGNED_INTEGER_CONVERSIONS(unsigned int)
ALIA_SIGNED_INTEGER_CONVERSIONS(long int)
ALIA_UNSIGNED_INTEGER_CONVERSIONS(unsigned long int)
ALIA_SIGNED_INTEGER_CONVERSIONS(long long int)
ALIA_UNSIGNED_INTEGER_CONVERSIONS(unsigned long long int)

void
from_string(std::string* value, std::string const& str)
{
    *value = str;
}
std::string
to_string(std::string value)
{
    return value;
}

} // namespace alia

#include <chrono>


namespace alia {

bool
system_needs_refresh(system const& sys)
{
    return sys.refresh_needed;
}

void
refresh_system(system& sys)
{
    sys.refresh_needed = false;

    int pass_count = 0;
    while (true)
    {
        refresh_event refresh;
        impl::dispatch_event(sys, refresh);
        if (!sys.root_component->dirty)
            break;
        ++pass_count;
        assert(pass_count < 64);
    };
}

} // namespace alia


namespace alia {

millisecond_count
default_external_interface::get_tick_count() const
{
    static auto start = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<
               std::chrono::duration<millisecond_count, std::milli>>(
               now - start)
        .count();
}

void
default_external_interface::schedule_timer_event(
    external_component_id component, millisecond_count time)
{
    schedule_event(owner.scheduler, component, time);
}

void
initialize_system(
    system& sys,
    std::function<void(context)> const& controller,
    external_interface* external)
{
    sys.controller = controller;
    sys.external.reset(
        external ? external : new default_external_interface(sys));
    sys.root_component.reset(new component_container);
}

void
process_internal_timing_events(system& sys, millisecond_count now)
{
    issue_ready_events(
        sys.scheduler,
        now,
        [&](external_component_id component, millisecond_count trigger_time) {
            timer_event event;
            event.trigger_time = trigger_time;
            dispatch_targeted_event(sys, event, component);
        });
}

} // namespace alia


namespace alia {

namespace {

double
sample_curve_x(unit_cubic_bezier_coefficients const& coeff, double t)
{
    return ((coeff.ax * t + coeff.bx) * t + coeff.cx) * t;
}

double
sample_curve_y(unit_cubic_bezier_coefficients const& coeff, double t)
{
    return ((coeff.ay * t + coeff.by) * t + coeff.cy) * t;
}

double
sample_curve_derivative(unit_cubic_bezier_coefficients const& coeff, double t)
{
    return (3 * coeff.ax * t + 2 * coeff.bx) * t + coeff.cx;
}

} // namespace

unit_cubic_bezier_coefficients
compute_curve_coefficients(unit_cubic_bezier const& bezier)
{
    unit_cubic_bezier_coefficients coeff;
    coeff.cx = 3 * bezier.p1x;
    coeff.bx = 3 * (bezier.p2x - bezier.p1x) - coeff.cx;
    coeff.ax = 1 - coeff.cx - coeff.bx;
    coeff.cy = 3 * bezier.p1y;
    coeff.by = 3 * (bezier.p2y - bezier.p1y) - coeff.cy;
    coeff.ay = 1 - coeff.cy - coeff.by;
    return coeff;
}

double
solve_for_t_at_x_with_bisection_search(
    unit_cubic_bezier_coefficients const& coeff,
    double x,
    double error_tolerance)
{
    double lower = 0.0;
    double upper = 1.0;
    double t = x;
    while (true)
    {
        double x_at_t = sample_curve_x(coeff, t);
        if (std::fabs(x_at_t - x) < error_tolerance)
            return t;
        if (x > x_at_t)
            lower = t;
        else
            upper = t;
        t = (lower + upper) / 2;
    }
}

double
solve_for_t_at_x(
    unit_cubic_bezier_coefficients const& coeff,
    double x,
    double error_tolerance)
{
    // Newton's method should be faster, so try that first.
    double t = x;
    for (int i = 0; i != 8; ++i)
    {
        double x_error = sample_curve_x(coeff, t) - x;
        if (std::fabs(x_error) < error_tolerance)
            return t;
        double dx = sample_curve_derivative(coeff, t);
        if (std::fabs(dx) < 1e-6)
            break;
        t -= x_error / dx;
    }

    // If that fails, fallback to a bisection search.
    return solve_for_t_at_x_with_bisection_search(coeff, x, error_tolerance);
}

double
eval_curve_at_x(unit_cubic_bezier const& curve, double x, double epsilon)
{
    if (x <= 0)
        return 0;
    if (x >= 1)
        return 1;

    auto coeff = compute_curve_coefficients(curve);

    return sample_curve_y(coeff, solve_for_t_at_x(coeff, x, epsilon));
}

} // namespace alia

namespace alia {

void
schedule_event(
    timer_event_scheduler& scheduler,
    external_component_id component,
    millisecond_count time)
{
    timer_event_request rq;
    rq.component = component;
    rq.trigger_time = time;
    rq.frame_issued = scheduler.frame_counter;
    scheduler.requests.push_back(rq);
}

void
issue_ready_events(
    timer_event_scheduler& scheduler,
    millisecond_count now,
    function_view<void(
        external_component_id component, millisecond_count time)> const& issue)
{
    ++scheduler.frame_counter;
    while (true)
    {
        // Ideally, the list would be stored sorted, but it has to be
        // sorted relative to the current tick count (to handle wrapping),
        // and the list is generally not very long anyway.
        auto next_event = scheduler.requests.end();
        for (auto i = scheduler.requests.begin(); i != scheduler.requests.end();
             ++i)
        {
            if (i->frame_issued != scheduler.frame_counter
                && int(now - i->trigger_time) >= 0
                && (next_event == scheduler.requests.end()
                    || int(next_event->trigger_time - i->trigger_time) >= 0))
            {
                next_event = i;
            }
        }
        if (next_event == scheduler.requests.end())
            break;

        timer_event_request request = *next_event;
        scheduler.requests.erase(next_event);

        issue(request.component, request.trigger_time);
    }
}

bool
has_scheduled_events(timer_event_scheduler const& scheduler)
{
    return !scheduler.requests.empty();
}

millisecond_count
get_time_until_next_event(
    timer_event_scheduler& scheduler, millisecond_count now)
{
    auto next_event = scheduler.requests.end();
    for (auto i = scheduler.requests.begin(); i != scheduler.requests.end();
         ++i)
    {
        if (next_event == scheduler.requests.end()
            || int(next_event->trigger_time - i->trigger_time) >= 0)
        {
            next_event = i;
        }
    }
    return int(next_event->trigger_time - now) >= 0
               ? (next_event->trigger_time - now)
               : 0;
}

} // namespace alia


namespace alia {

void
schedule_animation_refresh(dataless_context ctx)
{
    // Invoke the virtual method on the external system interface.
    // And also set a flag to indicate that a refresh is needed.
    system& sys = ctx.get<system_tag>();
    if (!sys.refresh_needed)
    {
        if (sys.external)
            sys.external->schedule_animation_refresh();
        sys.refresh_needed = true;
    }
    // Ensure that this component gets visited on the next refresh pass.
    mark_animating_component(ctx);
}

millisecond_count
get_raw_animation_tick_count(dataless_context ctx)
{
    schedule_animation_refresh(ctx);
    return ctx.get<timing_tag>().tick_counter;
}

value_signal<millisecond_count>
get_animation_tick_count(dataless_context ctx)
{
    return value(get_raw_animation_tick_count(ctx));
}

millisecond_count
get_raw_animation_ticks_left(dataless_context ctx, millisecond_count end_time)
{
    int ticks_remaining = int(end_time - ctx.get<timing_tag>().tick_counter);
    if (ticks_remaining > 0)
    {
        if (is_refresh_event(ctx))
            schedule_animation_refresh(ctx);
        return millisecond_count(ticks_remaining);
    }
    return 0;
}

} // namespace alia

namespace alia {

void
schedule_timer_event(
    dataless_context ctx,
    external_component_id id,
    millisecond_count trigger_time)
{
    auto& sys = get<system_tag>(ctx);
    sys.external->schedule_timer_event(id, trigger_time);
}

bool
detect_timer_event(dataless_context ctx, timer_data& data)
{
    timer_event* event;
    return detect_targeted_event(ctx, &data.identity, &event)
           && event->trigger_time == data.expected_trigger_time;
}

void
start_timer(dataless_context ctx, timer_data& data, millisecond_count duration)
{
    auto now = ctx.get<timing_tag>().tick_counter;
    auto trigger_time = now + duration;
    data.expected_trigger_time = trigger_time;
    schedule_timer_event(ctx, externalize(&data.identity), trigger_time);
}

void
restart_timer(
    dataless_context ctx, timer_data& data, millisecond_count duration)
{
    timer_event* event;
    bool detected = detect_event(ctx, &event);
    assert(detected);
    if (detected)
    {
        auto trigger_time = event->trigger_time + duration;
        data.expected_trigger_time = trigger_time;
        schedule_timer_event(ctx, externalize(&data.identity), trigger_time);
    }
}

void
raw_timer::update()
{
    triggered_ = data_->active && detect_timer_event(ctx_, *data_);
    if (triggered_)
        data_->active = false;
    on_refresh(ctx_, [&](auto ctx) {
        refresh_component_identity(ctx, data_->identity);
    });
}

void
raw_timer::start(unsigned duration)
{
    if (triggered_)
        restart_timer(ctx_, *data_, duration);
    else
        start_timer(ctx_, *data_, duration);
    data_->active = true;
}

} // namespace alia


namespace alia {

namespace impl {

struct square_wave_data
{
    bool value = true;
    timer_data timer;
};

value_signal<bool>
square_wave(
    context ctx,
    readable<millisecond_count> true_duration,
    readable<millisecond_count> false_duration)
{
    auto& data = get_cached_data<square_wave_data>(ctx);

    raw_timer timer(ctx, data.timer);

    if (timer.is_triggered())
        data.value = !data.value;

    if (!timer.is_active())
    {
        auto duration = conditional(
            data.value,
            true_duration,
            add_fallback(false_duration, true_duration));
        if (signal_has_value(duration))
        {
            timer.start(read_signal(duration));
        }
    }

    if (timer.is_triggered())
        abort_traversal(ctx);

    return value(data.value);
}

} // namespace impl

} // namespace alia
#endif
#endif
