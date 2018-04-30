
/*
This is an implementation of std::span from P0122R7
http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2018/p0122r7.pdf

Copyright 2018 Tristan Brindle

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef TCB_SPAN_HPP_INCLUDED
#define TCB_SPAN_HPP_INCLUDED

#include <array>
#include <cstddef>
#include <type_traits>

#ifndef TCB_SPAN_NO_EXCEPTIONS
// Attempt to discover whether we're being compiled with exception support
#if !(defined(__cpp_exceptions) || defined(__EXCEPTIONS))
#define TCB_SPAN_NO_EXCEPTIONS
#endif
#endif

#ifndef TCB_SPAN_NO_EXCEPTIONS
#include <cstdio>
#include <stdexcept>
#endif

// Various feature test macros

#ifndef TCB_SPAN_NAMESPACE_NAME
#define TCB_SPAN_NAMESPACE_NAME tcb
#endif

#ifdef TCB_SPAN_STD_COMPLIANT_MODE
#define TCB_SPAN_NO_DEPRECATION_WARNINGS
#endif

#ifndef TCB_SPAN_NO_DEPRECATION_WARNINGS
#define TCB_SPAN_DEPRECATED_FOR(msg) [[deprecated(msg)]]
#else
#define TCB_SPAN_DEPRECATED_FOR(msg)
#endif

#if __cplusplus >= 201703L
#define TCB_SPAN_HAVE_CPP17
#endif

#if __cplusplus >= 201402L
#define TCB_SPAN_HAVE_CPP14
#endif

#if defined(TCB_HAVE_CPP17) || defined(__cpp_inline_variables)
#define TCB_SPAN_INLINE_VAR inline
#else
#define TCB_SPAN_INLINE_VAR
#endif

#if defined(TCB_HAVE_CPP14) || (defined(__cpp_constexpr)  && __cpp_constexpr >= 201304)
#define TCB_SPAN_CONSTEXPR14 constexpr
#else
#define TCB_SPAN_CONSTEXPR14
#endif

#if defined(TCB_HAVE_CPP17) || defined(__cpp_deduction_guides)
#define TCB_SPAN_HAVE_DEDUCTION_GUIDES
#endif

#if defined(TCB_HAVE_CPP17) || defined(__cpp_lib_byte)
#define TCB_SPAN_HAVE_STD_BYTE
#endif

#if defined(TCB_HAVE_CPP17) || defined(__cpp_lib_array_constexpr)
#define TCB_SPAN_HAVE_CONSTEXPR_STD_ARRAY_ETC
#endif

#if defined(TCB_HAVE_CONSTEXPR_STD_ARRAY_ETC)
#define TCB_SPAN_ARRAY_CONSTEXPR constexpr
#else
#define TCB_SPAN_ARRAY_CONSTEXPR
#endif

namespace TCB_SPAN_NAMESPACE_NAME {

#ifdef TCB_SPAN_HAVE_STD_BYTE
using byte = std::byte;
#else
using byte = unsigned char;
#endif

TCB_SPAN_INLINE_VAR constexpr std::ptrdiff_t dynamic_extent = -1;

template <typename ElementType, std::ptrdiff_t Extent = dynamic_extent>
class span;

namespace detail {

template <typename E, std::ptrdiff_t S>
struct span_storage {
    constexpr span_storage() noexcept = default;

    constexpr span_storage(E* ptr, std::ptrdiff_t /*unused*/) noexcept
        : ptr(ptr)
    {}

    E* ptr = nullptr;
    static constexpr std::ptrdiff_t size = S;
};

template <typename E>
struct span_storage<E, dynamic_extent> {
    constexpr span_storage() noexcept = default;

    constexpr span_storage(E* ptr, std::ptrdiff_t size) noexcept
        : ptr(ptr),
          size(size)
    {}

    E* ptr = nullptr;
    std::ptrdiff_t size = 0;
};

// Reimplementation of C++17 std::size() and std::data()
#if defined(TCB_SPAN_HAVE_CPP17) || defined(__cpp_lib_nonmember_container_access)
using std::size;
using std::data;
#else
template <class C>
constexpr auto size(const C& c) -> decltype(c.size())
{
    return c.size();
}

template <class T, std::size_t N>
constexpr std::size_t size(const T (&array)[N]) noexcept
{
    return N;
}

template <class C>
constexpr auto data(C& c) -> decltype(c.data())
{
    return c.data();
}

template <class C>
constexpr auto data(const C& c) -> decltype(c.data())
{
    return c.data();
}

template <class T, std::size_t N>
constexpr T* data(T (&array)[N]) noexcept
{
    return array;
}

template <class E>
constexpr const E* data(std::initializer_list<E> il) noexcept
{
    return il.begin();
}
#endif // TCB_SPAN_HAVE_CPP17

#if defined(TCB_SPAN_HAVE_CPP17) || defined(__cpp_lib_void_t)
using std::void_t;
#else
template <typename...>
using void_t = void;
#endif

template <typename T>
using uncvref_t = typename std::remove_cv<typename std::remove_reference<T>::type>::type;

template <typename>
struct is_span : std::false_type {};

template <typename T, std::ptrdiff_t S>
struct is_span<span<T, S>> : std::true_type {};

template <typename>
struct is_std_array : std::false_type {};

template <typename T, std::size_t N>
struct is_std_array<std::array<T, N>> : std::true_type {};

template <typename, typename = void>
struct has_size_and_data : std::false_type {};

template <typename T>
struct has_size_and_data<T, void_t<
    decltype(detail::size(std::declval<T>())),
    decltype(detail::data(std::declval<T>()))>>
    : std::true_type {};

template <typename C, typename U = uncvref_t<C>>
struct is_container {
    static constexpr bool value =
            !is_span<U>::value &&
            !is_std_array<U>::value &&
            !std::is_array<U>::value &&
            has_size_and_data<C>::value;
};

template <typename T>
using remove_pointer_t = typename std::remove_pointer<T>::type;

template <typename, typename, typename = void>
struct is_container_element_type_compatible : std::false_type {};

template <typename T, typename E>
struct is_container_element_type_compatible<T, E, void_t<decltype(detail::data(std::declval<T>()))>>
        : std::is_convertible<
                remove_pointer_t<decltype(detail::data(std::declval<T>()))>(*)[],
                E(*)[]> {};

}

template <typename ElementType, std::ptrdiff_t Extent>
class span {
    static_assert(Extent == dynamic_extent || Extent >= 0,
                  "A span must have an extent greater than or equal to zero, or a dynamic extent");

    using storage_type = detail::span_storage<ElementType, Extent>;

public:
    // constants and types
    using element_type = ElementType;
    using value_type = typename std::remove_cv<ElementType>::type;
    using index_type = std::ptrdiff_t;
    using difference_type = std::ptrdiff_t;
    using pointer = ElementType*;
    using reference = ElementType&;
    using iterator = pointer;
    using const_iterator = const ElementType*;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    static constexpr index_type extent = Extent;

    // [span.cons], span constructors, copy, assignment, and destructor
    template <std::ptrdiff_t E = Extent, typename std::enable_if<
        E <= 0, int>::type = 0
    >
    constexpr span() noexcept
    {}

    constexpr span(pointer ptr, index_type count)
        : storage_(ptr, count)
    {}

    constexpr span(pointer first_elem, pointer last_elem)
        : storage_(first_elem, last_elem - first_elem)
    {}

    template <std::size_t N, std::ptrdiff_t E = Extent, typename std::enable_if<
        (E == dynamic_extent || N == E) &&
        detail::is_container_element_type_compatible<element_type (&)[N], ElementType>::value, int>::type = 0
    >
    constexpr span(element_type (&arr)[N]) noexcept
        : storage_(arr, N)
    {}

    template <std::size_t N, std::ptrdiff_t E = Extent, typename std::enable_if<
        (E == dynamic_extent || N == E) &&
        detail::is_container_element_type_compatible<std::array<value_type, N>&, ElementType>::value, int>::type = 0
    >
    TCB_SPAN_ARRAY_CONSTEXPR span(std::array<value_type, N>& arr) noexcept
        : storage_(arr.data(), N)
    {}

    template <std::size_t N, std::ptrdiff_t E = Extent, typename std::enable_if<
        (E == dynamic_extent || N == E) &&
        detail::is_container_element_type_compatible<const std::array<value_type, N>&, ElementType>::value, int>::type = 0
    >
    TCB_SPAN_ARRAY_CONSTEXPR span(const std::array<value_type, N>& arr) noexcept
        : storage_(arr.data(), N)
    {}

    template <typename Container, typename std::enable_if<
        detail::is_container<Container>::value &&
        detail::is_container_element_type_compatible<Container&, ElementType>::value,
    int>::type = 0>
    constexpr span(Container& cont)
        : storage_(detail::data(cont), detail::size(cont))
    {}

    template <typename Container, typename std::enable_if<
        detail::is_container<Container>::value &&
        detail::is_container_element_type_compatible<const Container&, ElementType>::value, int>::type = 0
    >
    constexpr span(const Container& cont)
        : storage_(detail::data(cont), detail::size(cont))
    {
    }

    constexpr span(const span& other) noexcept = default;

    template <typename OtherElementType, std::ptrdiff_t OtherExtent, typename std::enable_if<
        (Extent == OtherExtent || Extent == dynamic_extent) &&
         std::is_convertible<OtherElementType(*)[], ElementType(*)[]>::value, int>::type = 0
    >
    constexpr span(const span<OtherElementType, OtherExtent>& other) noexcept
        : storage_(other.data(), other.size())
    {}

    ~span() noexcept = default;

    TCB_SPAN_CONSTEXPR14 span& operator=(const span& other) noexcept = default;

    // [span.sub], span subviews
    template <std::ptrdiff_t Count>
    constexpr span<element_type, Count> first() const
    {
        return {data(), Count};
    }

    template <std::ptrdiff_t Count>
    constexpr span<element_type, Count> last() const
    {
        return {data() + (size() - Count), Count};
    }

    template <std::ptrdiff_t Offset, std::ptrdiff_t Count = dynamic_extent>
    constexpr auto subspan() const
        -> span<ElementType,
                Count != dynamic_extent ? Count : (Extent != dynamic_extent ? Extent - Offset : dynamic_extent)>
    {
        return {data() + Offset,
                Count != dynamic_extent ? Count : (Extent != dynamic_extent ? Extent - Offset
                    : size() - Offset)};
    }

    constexpr span<element_type, dynamic_extent> first(index_type count) const
    {
        return {data(), count};
    }

    constexpr span<element_type, dynamic_extent> last(index_type count) const
    {
        return {data() + (size() - count), count};
    }

    constexpr span<element_type, dynamic_extent> subspan(index_type offset, index_type count = dynamic_extent) const
    {
        return { data() + offset, count == dynamic_extent ? size() - offset : count };
    }

    // [span.obs], span observers
    constexpr index_type size() const noexcept { return storage_.size; }

    constexpr index_type size_bytes() const noexcept
    {
        return size() * sizeof(element_type);
    }

    constexpr bool empty() const noexcept
    {
        return size() == 0;
    }

    // [span.elem], span element access
    constexpr reference operator[](index_type idx) const
    {
        return *(data() + idx);
    }

    /* Extension: not in P0122 */
#ifndef TCB_SPAN_STD_COMPLIANT_MODE
    TCB_SPAN_CONSTEXPR14 reference at(index_type idx) const
    {
#ifndef TCB_SPAN_NO_EXCEPTIONS
        if (idx < 0 || idx >= size()) {
            char msgbuf[64] = {0, };
            std::snprintf(msgbuf, sizeof(msgbuf), "Index %td is out of range for span of size %td", idx, size());
            throw std::out_of_range{msgbuf};
        }
#endif // TCB_SPAN_NO_EXCEPTIONS
        return this->operator[](idx);
    }

    constexpr reference front() const { return this->operator[](0); }

    constexpr reference back() const { return this->operator[](size() - 1); }

#endif // TCB_SPAN_STD_COMPLIANT_MODE

#ifndef TCB_SPAN_NO_FUNCTION_CALL_OPERATOR
    TCB_SPAN_DEPRECATED_FOR("Use operator[] instead")
    constexpr reference operator()(index_type idx) const
    {
        return *(data() + idx);
    }
#endif // TCB_SPAN_NO_FUNCTION_CALL_OPERATOR

    constexpr pointer data() const noexcept
    {
        return storage_.ptr;
    }

    // [span.iterators], span iterator support
    constexpr iterator begin() const noexcept { return data(); }

    constexpr iterator end() const noexcept { return data() + size(); }

    constexpr const_iterator cbegin() const noexcept { return begin(); }

    constexpr const_iterator cend() const noexcept { return end(); }

    TCB_SPAN_ARRAY_CONSTEXPR reverse_iterator rbegin() const noexcept { return reverse_iterator(end()); }

    TCB_SPAN_ARRAY_CONSTEXPR reverse_iterator rend() const noexcept { return reverse_iterator(begin()); }

    TCB_SPAN_ARRAY_CONSTEXPR const_reverse_iterator crbegin() const noexcept { return const_reverse_iterator(cend()); }

    TCB_SPAN_ARRAY_CONSTEXPR const_reverse_iterator crend() const noexcept { return const_reverse_iterator(cbegin()); }

private:
    storage_type storage_{};
};

#ifdef TCB_SPAN_HAVE_DEDUCTION_GUIDES

/* Deduction Guides */
template<class T, size_t N>
span(T (&)[N]) -> span<T, N>;

template<class T, size_t N>
span(std::array<T, N>&) -> span<T, N>;

template<class T, size_t N>
span(const std::array<T, N>&) -> span<const T, N>;

template<class Container>
span(Container&) -> span<typename Container::value_type>;

template<class Container>
span(const Container&) -> span<const typename Container::value_type>;

#endif // TCB_HAVE_DEDUCTION_GUIDES

template <typename ElementType, std::ptrdiff_t Extent>
constexpr span<ElementType, Extent> make_span(span<ElementType, Extent> s) noexcept
{
    return s;
}

template <typename T, std::size_t N>
constexpr span<T, N> make_span(T (&arr)[N]) noexcept
{
    return {arr};
}

template <typename T, std::size_t N>
TCB_SPAN_CONSTEXPR14 span<T, N> make_span(std::array<T, N>& arr) noexcept
{
    return {arr};
}

template <typename T, std::size_t N>
TCB_SPAN_CONSTEXPR14 span<const T, N> make_span(const std::array<T, N>& arr) noexcept
{
    return {arr};
}

template <typename Container>
constexpr span<typename Container::value_type> make_span(Container& cont)
{
    return {cont};
}

template <typename Container>
constexpr span<const typename Container::value_type> make_span(const Container& cont)
{
    return {cont};
}

/* Comparison operators */
// Implementation note: the implementations of == and < are equivalent to
// 4-legged std::equal and std::lexicographical_compare respectively

template <typename T, std::ptrdiff_t X, typename U, std::ptrdiff_t Y>
TCB_SPAN_CONSTEXPR14 bool operator==(span<T, X> lhs, span<U, Y> rhs)
{
    if (lhs.size() != rhs.size()) {
        return false;
    }

    for (std::ptrdiff_t i = 0; i < lhs.size(); i++) {
        if (lhs[i] != rhs[i]) {
            return false;
        }
    }

    return true;
}

template <typename T, std::ptrdiff_t X, typename U, std::ptrdiff_t Y>
TCB_SPAN_CONSTEXPR14 bool operator!=(span<T, X> lhs, span<U, Y> rhs)
{
    return !(lhs == rhs);
}

template <typename T, std::ptrdiff_t X, typename U, std::ptrdiff_t Y>
TCB_SPAN_CONSTEXPR14 bool operator<(span<T, X> lhs, span<U, Y> rhs)
{
    // No std::min to avoid dragging in <algorithm>
    const std::ptrdiff_t size = lhs.size() < rhs.size() ? lhs.size() : rhs.size();

    for (std::ptrdiff_t i = 0; i < size; i++) {
        if (lhs[i] < rhs[i]) {
            return true;
        }
        if (lhs[i] > rhs[i]) {
            return false;
        }
    }
    return lhs.size() < rhs.size();
}

template <typename T, std::ptrdiff_t X, typename U, std::ptrdiff_t Y>
TCB_SPAN_CONSTEXPR14 bool operator<=(span<T, X> lhs, span<U, Y> rhs)
{
    return !(rhs < lhs);
}

template <typename T, std::ptrdiff_t X, typename U, std::ptrdiff_t Y>
TCB_SPAN_CONSTEXPR14 bool operator>(span<T, X> lhs, span<U, Y> rhs)
{
    return rhs < lhs;
}

template <typename T, std::ptrdiff_t X, typename U, std::ptrdiff_t Y>
TCB_SPAN_CONSTEXPR14 bool operator>=(span<T, X> lhs, span<U, Y> rhs)
{
    return !(lhs < rhs);
}

template <typename ElementType, std::ptrdiff_t Extent>
span<const byte, ((Extent == dynamic_extent) ?
                    dynamic_extent :
                    (static_cast<ptrdiff_t>(sizeof(ElementType)) * Extent))>
as_bytes(span<ElementType, Extent> s) noexcept
{
    return {reinterpret_cast<const byte*>(s.data()), s.size_bytes()};
}

template <class ElementType, ptrdiff_t Extent, typename std::enable_if<
    !std::is_const<ElementType>::value, int>::type = 0
>
span<byte, ((Extent == dynamic_extent) ?
                dynamic_extent :
                (static_cast<ptrdiff_t>(sizeof(ElementType)) * Extent))>
as_writable_bytes(span<ElementType, Extent> s) noexcept
{
    return {reinterpret_cast<byte*>(s.data()), s.size_bytes()};
}

/* Extension: nonmember subview operations */

#ifndef TCB_SPAN_STD_COMPLIANT_MODE

template <std::ptrdiff_t Count, typename T>
constexpr auto first(T&& t)
    -> decltype(make_span(std::forward<T>(t)).template first<Count>())
{
    return make_span(std::forward<T>(t)).template first<Count>();
}

template <std::ptrdiff_t Count, typename T>
constexpr auto last(T&& t)
    -> decltype(make_span(std::forward<T>(t)).template last<Count>())
{
    return make_span(std::forward<T>(t)).template last<Count>();
}

template <std::ptrdiff_t Offset, std::ptrdiff_t Count = dynamic_extent, typename T>
constexpr auto subspan(T&& t)
    -> decltype(make_span(std::forward<T>(t)).template subspan<Offset, Count>())
{
    return make_span(std::forward<T>(t)).template subspan<Offset, Count>();
}

template <typename T>
constexpr auto first(T&& t, std::ptrdiff_t count)
    -> decltype(make_span(std::forward<T>(t)).first(count))
{
    return make_span(std::forward<T>(t)).first(count);
}

template <typename T>
constexpr auto last(T&& t, std::ptrdiff_t count)
    -> decltype(make_span(std::forward<T>(t)).last(count))
{
    return make_span(std::forward<T>(t)).last(count);
}

template <typename T>
constexpr auto subspan(T&& t, std::ptrdiff_t offset, std::ptrdiff_t count = dynamic_extent)
    -> decltype(make_span(std::forward<T>(t)).subspan(offset, count))
{
    return make_span(std::forward<T>(t)).subspan(offset, count);
}

#endif // TCB_SPAN_STD_COMPLIANT_MODE

} // End namespace tcb


/* Extension: support for C++17 structured bindings */

#ifndef TCB_SPAN_STD_COMPLIANT_MODE

namespace TCB_SPAN_NAMESPACE_NAME {

template <std::ptrdiff_t N, typename E, std::ptrdiff_t S>
constexpr auto get(span<E, S> s) -> decltype(s[N])
{
    return s[N];
}

} // end namespace tcb

namespace std {

template <typename E, std::ptrdiff_t S>
struct tuple_size<tcb::span<E, S>>
    : integral_constant<std::size_t, S>
{};

template <typename E>
class tuple_size<tcb::span<E, tcb::dynamic_extent>>; // not defined

template <std::size_t N, typename E, std::ptrdiff_t S>
struct tuple_element<N, tcb::span<E, S>>
{
    using type = E&;
};

} // end namespace std

#endif // TCB_SPAN_STD_COMPLIANT_MODE

#endif // TCB_SPAN_HPP_INCLUDED