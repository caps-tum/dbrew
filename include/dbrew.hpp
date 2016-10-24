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

// Generic template. Should never be instantiated.
template <typename T, typename enable = void> struct rewriter;

// specialization with T == function pointer && T != member function pointer
template <typename T>
struct rewriter<T, typename std::enable_if<std::is_function<typename std::remove_pointer<T>::type>::value>::type> {
	Rewriter *_rewriter;
	T _func;

	rewriter(T func) : _rewriter(dbrew_new()), _func(func) {
		// when a function is passed to a function it is automatically converted
		// to a function pointer, but std::function expects a function type, not
		// a function pointer
		using function_type = typename std::remove_pointer<T>::type;
		auto stdfunc = std::function<function_type>(func);
		init(stdfunc, (uint64_t)func);
	}

	~rewriter() { dbrew_free(_rewriter); }

	template <typename R, typename... Args> void init(std::function<R(Args...)> stdfunc, uint64_t ptr) {
		// FIXME: generate ptr from stdfunc
		dbrew_set_function(_rewriter, ptr);

		// set the number of parameters
		dbrew_config_parcount(_rewriter, sizeof...(Args));
		// TODO loop over all parameters and handle special cases like references
	}

	// TODO specialize for A == pointer ... I guess, check dbrew docu
	template <typename A> rewriter<T> bind(size_t pos, const A &val) {
		// TODO add assert for pos
		dbrew_config_staticpar(_rewriter, pos);

		// FIXME magic numbers!
		T f = (T)dbrew_rewrite(_rewriter, 1, 0);
		return rewriter<T>(f);
	}

	// caller for the function
	template <typename... Args> auto operator()(Args... a) { return _func(a...); }
};

// TODO specialization for member function pointer
template <typename T> struct rewriter<T, typename std::enable_if<std::is_member_function_pointer<T>::value>::type>;
}

#endif // DBREW_HPP
