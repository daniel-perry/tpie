// -*- mode: c++; tab-width: 4; indent-tabs-mode: t; eval: (progn (c-set-style "stroustrup") (c-set-offset 'innamespace 0)); -*-
// vi:set ts=4 sts=4 sw=4 noet :
// Copyright 2013, The TPIE development team
// 
// This file is part of TPIE.
// 
// TPIE is free software: you can redistribute it and/or modify it under
// the terms of the GNU Lesser General Public License as published by the
// Free Software Foundation, either version 3 of the License, or (at your
// option) any later version.
// 
// TPIE is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
// License for more details.
// 
// You should have received a copy of the GNU Lesser General Public License
// along with TPIE.  If not, see <http://www.gnu.org/licenses/>

#ifndef __TPIE_PIPELINING_PARALLEL_FACTORY_H__
#define __TPIE_PIPELINING_PARALLEL_FACTORY_H__

namespace tpie {

namespace pipelining {

namespace parallel_bits {

///////////////////////////////////////////////////////////////////////////////
/// \brief Factory instantiating a parallel multithreaded pipeline.
///////////////////////////////////////////////////////////////////////////////
template <typename fact_t>
class factory : public factory_base {
	fact_t fact;
	const options opts;
public:
	template <typename dest_t>
	struct constructed {
		typedef typename dest_t::item_type T2;

		typedef after<T2> after_t;
		typedef typename fact_t::template constructed<after_t>::type processor_t;
		typedef typename processor_t::item_type T1;

		typedef producer<T1, T2> type;
	};

	factory(const fact_t & fact, const options opts)
		: fact(fact)
		, opts(opts)
	{
	}

	template <typename dest_t>
	typename constructed<dest_t>::type
	construct(const dest_t & dest) const {
		typedef constructed<dest_t> gen_t;

		typedef typename gen_t::T1 input_type;
		typedef typename gen_t::T2 output_type;
		typedef state<input_type, output_type> state_t;

		typedef consumer_impl<input_type, output_type, dest_t> consumer_t;

		typedef typename gen_t::type producer_t;

		typename state_t::ptr st(new state_t(opts, fact));

		consumer_t consumer(dest, st);
		this->init_node(consumer);
		producer_t producer(st, consumer);
		this->init_node(producer);
		return producer;
	}
};

} // namespace parallel_bits

} // namespace pipelining

} // namespace tpie

#endif // __TPIE_PIPELINING_PARALLEL_FACTORY_H__
