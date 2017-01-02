//
// Copyright (c) 2013-2016 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_HANDLER_PTR_HPP
#define BEAST_HANDLER_PTR_HPP

#include <cstdint>
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
    state is managed by the @ref handler_ptr and an allocator
    associated with the final handler is used to create the managed
    object.
*/
template<class T, class Handler>
class handler_ptr
{
    struct D
    {
        ~D() = delete;

        template<class... Args>
        explicit
        D(Handler&& h_, Args&&... args);

        template<class... Args>
        explicit
        D(Handler const& h_, Args&&... args);

        void
        destroy();
        
        T& get()
        {
            return *reinterpret_cast<T*>(t);
        }

        Handler h;

    private:
        std::uint8_t t[sizeof(T)];
    };

    void
    release();

    D* p_;
    mutable handler_ptr* next_;
    mutable handler_ptr* prev_;

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
    ~handler_ptr();

    /** Create a managed object using an allocator.

        @param handler The handler to store. If possible the
        implementation will take ownership by forwarding the value,
        else a copy of the handler is stored.

        @param args Optional arguments forwarded to
        the managed object's constructor.

        @note This constructor participates in overload resolution
        if and only if the first parameter is not convertible to
        @ref handler_ptr.
    */
#if GENERATING_DOCS
    template<class DeducedHandler, class... Args>
    explicit
    handler_ptr(DeducedHandler&& alloc, Args&&... args);
#else
    template<class DeducedHandler, class... Args,
        class = typename std::enable_if<
            ! std::is_convertible<
                typename std::decay<DeducedHandler>::type,
                    handler_ptr>::value>::type>
    explicit
    handler_ptr(DeducedHandler&& handler, Args&&... args);
#endif

    /** Default constructor.

        Default constructed container have no managed object.
    */
    handler_ptr();

    /** Move constructor.

        When this call returns, the moved-from container
        will have no managed object.
    */
    handler_ptr(handler_ptr&& other);

    /// Copy constructor
    handler_ptr(handler_ptr const& other);

    /** Move assignment

        When this call returns, the moved-from container
        will have no managed object.
    */
    handler_ptr& operator=(handler_ptr&& other);

    /// Copy assignment
    handler_ptr& operator=(handler_ptr const& other);

    /// Return a reference to the contained handler
    Handler&
    handler() const
    {
        return p_->h;
    }

    /// Returns a pointer to the managed object
    T*
    get() const
    {
        return &p_->get();
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

} // beast

#include <boost/asio/detail/handler_alloc_helpers.hpp>
#include <boost/assert.hpp>
#include <memory>
#include <new>

namespace beast {

template<class T, class Handler>
template<class... Args>
handler_ptr<T, Handler>::D::
D(Handler&& h_, Args&&... args)
    : h(std::move(h_))
{
    try
    {
        new(t) T{std::forward<Args>(args)...};
    }
    catch(...)
    {
        h_ = std::move(h);
        throw;
    }
}

template<class T, class Handler>
template<class... Args>
handler_ptr<T, Handler>::D::
D(Handler const& h_, Args&&... args)
    : h(h_)
{
    new(t) T{std::forward<Args>(args)...};
}

template<class T, class Handler>
void
handler_ptr<T, Handler>::D::
destroy()
{
    auto h_ = Handler{std::move(h)};
    get().~T();
    boost_asio_handler_alloc_helpers::deallocate(
        this, sizeof(*this), h_);
}

template<class T, class Handler>
inline
void
handler_ptr<T, Handler>::
release()
{
    BOOST_ASSERT(p_);
    if(prev_)
    {
        prev_->next_ = next_;
        if(next_)
            next_->prev_ = prev_;
        return;
    }
    else if(next_)
    {
        next_->prev_ = prev_;
        if(prev_)
            prev_->next_ = next_;
        return;
    }
    p_->destroy();
}

template<class T, class Handler>
handler_ptr<T, Handler>::
~handler_ptr()
{
    static_assert(! std::is_array<T>::value,
        "Array type is not supported");
    if(p_)
        release();
}

template<class T, class Handler>
template<class DeducedHandler, class... Args, class>
handler_ptr<T, Handler>::
handler_ptr(DeducedHandler&& handler, Args&&... args)
{
    auto p = reinterpret_cast<D*>(
        boost_asio_handler_alloc_helpers::allocate(
            sizeof(D), handler));
    if(! p)
        throw std::bad_alloc{};
    try
    {
        new(p) D(std::forward<DeducedHandler>(handler),
            std::forward<Args>(args)...);
    }
    catch(...)
    {
        boost_asio_handler_alloc_helpers::deallocate(
            p, sizeof(*p), handler);
        throw;
    }
    p_ = p;
    next_ = nullptr;
    prev_ = nullptr;
}

template<class T, class Handler>
handler_ptr<T, Handler>::
handler_ptr()
    : p_(nullptr)
{
}

template<class T, class Handler>
handler_ptr<T, Handler>::
handler_ptr(handler_ptr&& other)
    : p_(other.p_)
{
    if(! p_)
        return;
    other.p_ = nullptr;
    prev_ = other.prev_;
    next_ = other.next_;
    if(prev_)
        prev_->next_ = this;
    if(next_)
        next_->prev_ = this;
}

template<class T, class Handler>
handler_ptr<T, Handler>::
handler_ptr(handler_ptr const& other)
    : p_(other.p_)
{
    if(! p_)
        return;
    ++p_->n;
    prev_ = other.prev_;
    if(prev_)
        prev_->next_ = this;
    next_ =
        const_cast<handler_ptr*>(&other);
    next_->prev_ = this;
}

template<class T, class Handler>
auto
handler_ptr<T, Handler>::
operator=(handler_ptr&& other) ->
    handler_ptr&
{
    if(p_)
        release();
    p_ = other.p_;
    if(! p_)
        return *this;
    other.p_ = nullptr;
    prev_ = other.prev_;
    next_ = other.next_;
    if(prev_)
        prev_->next_ = this;
    if(next_)
        next_->prev_ = this;
    return *this;
}

template<class T, class Handler>
auto
handler_ptr<T, Handler>::
operator=(handler_ptr const& other) ->
    handler_ptr&
{
    if(this == &other)
        return *this;
    if(p_)
        release();
    p_ = other.p_;
    if(! p_)
        return *this;
    ++p_->n;
    prev_ = other.prev_;
    if(prev_)
        prev_->next_ = this;
    next_ =
        const_cast<handler_ptr*>(&other);
    next_->prev_ = this;
    return *this;
}

template<class T, class Handler>
void
handler_ptr<T, Handler>::
reset()
{
    if(p_)
    {
        release();
        p_ = nullptr;
    }
}

template<class T, class Handler>
void
handler_ptr<T, Handler>::
reset_all()
{
    if(! p_)
        return;
    auto it = prev_;
    while(it)
    {
        auto cur = it;
        it = it->prev_;
        BOOST_ASSERT(cur->p_);
        cur->p_ = nullptr;
    }
    it = next_;
    while(it)
    {
        auto cur = it;
        it = it->next_;
        BOOST_ASSERT(cur->p_);
        cur->p_ = nullptr;
    }
    this->p_->destroy();
    this->p_ = nullptr;
}

} // beast

#endif