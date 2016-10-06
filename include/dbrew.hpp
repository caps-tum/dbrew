/**
 * This file is part of DBrew, the dynamic binary rewriting library.
 *
 * (c) 2016, Jens Breitbart <jbreitbart@gmail.com>
 *
 * DBrew is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License (LGPL)
 * as published by the Free Software Foundation, either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * DBrew is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with DBrew.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * C++ Header file defining the DBrew API C++.
 */

#ifndef DBREW_HPP
#define DBREW_HPP

#ifndef __cplusplus
#error Do not include dbrew.hpp when using a C compiler.
#endif

#include "dbrew.h"

#include <functional>
#include <type_traits>

#include <cassert>

namespace dbrew {

template <typename T>
struct rewriter {
    Rewriter* _rewriter;
    // TODO add check if T is a callable

    // for T == function pointer
    rewriter(T func) {
        // when a function is passed to a function it is automatically converted
        // to a function pointer, but std::function expects a function type, not
        // a function pointer
        using function_type = typename std::remove_pointer<T>::type;
        auto stdfunc = std::function<function_type>(func);
        init(stdfunc, (uint64_t)func);
    }

    template <typename R, typename ...Args>
    void init(std::function<R(Args...)> stdfunc, uint64_t ptr) {
        _rewriter = dbrew_new();
        dbrew_set_function(_rewriter, ptr);

        // set the number of parameters
        dbrew_config_parcount(_rewriter, sizeof...(Args));
//        foo_t f = (foo_t) dbrew_rewrite(_rewriter, 2, 3);
    }

};

}

#endif // DBREW_HPP
