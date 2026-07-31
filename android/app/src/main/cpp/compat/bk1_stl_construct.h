#pragma once
// std::construct and std::destroy, as the STL that shipped with MSVC 6 had
// them.
//
// They were SGI extensions, never standard, and no library has carried them
// for twenty years. The engine calls std::construct at sixteen places across
// AILogic and Main, always with one argument -- construct an object in memory
// that is already allocated. That is placement new, and this says so.
//
// This lives in namespace std because the calls are qualified and there is
// nowhere else they would be found. Adding to that namespace is not something
// to do lightly; it is right here because the whole purpose of this layer is
// to be the library the engine was written against, and these two names are
// part of that library.
#include <memory>
#include <new>

namespace std {

template <class T>
inline void construct( T *pWhere )
{
    ::new ( static_cast<void *>( pWhere ) ) T();
}

template <class T, class TValue>
inline void construct( T *pWhere, const TValue &value )
{
    ::new ( static_cast<void *>( pWhere ) ) T( value );
}

// The counterpart, for completeness: the engine does not call it today, but a
// half-present pair is worse than either.
template <class T>
inline void destroy( T *pWhat )
{
    pWhat->~T();
}

template <class TIterator>
inline void destroy( TIterator first, TIterator last )
{
    for ( ; first != last; ++first )
        destroy( &*first );
}

}   // namespace std
