//
// Copyright (c) 2013-2016 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_MUTUAL_PTR_HPP
#define BEAST_MUTUAL_PTR_HPP

#include <cstdint>
#include <type_traits>
#include <utility>

namespace beast {

/** A smart pointer container.

    This is a smart pointer that retains shared ownership of an
    object through a pointer. The object is destroyed and its
    memory deallocated when one of the following happens:

    * The last remaining container owning the object is destroyed
    * The last remaining container owning the object is assigned
      another object via @ref operator= or @ref reset.
    * The function @ref reset_all is called.

    Objects of this type are used in the implementation of
    composed operations. Typically the composed operation's shared
    state is managed by the @ref mutual_ptr and an allocator
    associated with the final handler is used to create the managed
    object.
*/
template<class T>
class mutual_ptr
{
    struct B
    {
        T t;
        std::uint16_t n = 1;

        template<class... Args>
        explicit
        B(Args&&... args)
            : t(std::forward<Args>(args)...)
        {
        }

        virtual
        void
        destroy() = 0;
    };

    template<class Alloc>
    struct D;

    void
    release();

    B* p_;
    mutable mutual_ptr* next_;
    mutable mutual_ptr* prev_;

public:
    /// The type of the managed object
    using element_type = T;

    /** Destructor

        If `*this` owns an object and it is the last container
        owning it, the object is destroyed using a copy of the original
        allocator.
        
        After the destruction, the smart pointers that shared ownership
        with `*this`, if any, will report a @ref use_count that is one
        less than its previous value.
    */
    ~mutual_ptr();

    /** Create a managed object using an allocator.

        @param alloc The allocator to use.

        @param args Optional arguments forwarded to
        the managed object's constructor.

        @note This constructor participates in overload resolution
        if and only if the first parameter is not convertible to
        @ref mutual_ptr.
    */
#if GENERATING_DOCS
    template<class Alloc, class... Args>
    explicit
    mutual_ptr(Alloc&& alloc, Args&&... args);
#else
    template<class Alloc, class... Args,
        class = typename std::enable_if<
            ! std::is_convertible<
                typename std::decay<Alloc>::type,
                    mutual_ptr>::value>::type>
    explicit
    mutual_ptr(Alloc&& alloc, Args&&... args);
#endif

    /** Default constructor.

        Default constructed container have no managed object.
    */
    mutual_ptr();

    /** Move constructor.

        When this call returns, the moved-from container
        will have no managed object.
    */
    mutual_ptr(mutual_ptr&& other);

    /// Copy constructor
    mutual_ptr(mutual_ptr const& other);

    /** Move assignment

        When this call returns, the moved-from container
        will have no managed object.
    */
    mutual_ptr& operator=(mutual_ptr&& other);

    /// Copy assignment
    mutual_ptr& operator=(mutual_ptr const& other);

    /// Returns a pointer to the managed object
    T*
    get() const
    {
        return &p_->t;
    }

    /** Return a reference to the managed object. */
    T&
    operator*() const
    {
        return *get();
    }

    /** Return a pointer to the managed object. */
    T*
    operator->() const
    {
        return get();
    }

    /** Returns the number of instances managing the current object.

        If there is no managed object, `0` is returned.
    */
    std::size_t
    use_count() const
    {
        return p_ ? p_->n : 0;
    }

    /// Returns `true` if `*this* is the only owner of the managed object.
    bool
    unique() const
    {
        return use_count() == 1;
    }

    /// Returns `true` if `*this` manages an object.
    explicit
    operator bool() const
    {
        return p_ != nullptr;
    }

    /** Release ownership of the managed object. */
    void
    reset();

    /** Reset all instances managing this object.

        This function releases all instances of the smart pointer
        which point to the same managed object, including this
        instance.

        Before the call, this must point to a managed object.
    */
    void
    reset_all();
};

template<class T, class Alloc, class... Args>
mutual_ptr<T>
allocate_mutual(Alloc&& a, Args&&... args)
{
    return mutual_ptr<T>{std::forward<Alloc>(a),
        std::forward<Args>(args)...};
}

} // beast

#include <beast/core/impl/mutual_ptr.ipp>

#endif
