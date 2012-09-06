// -*- mode: c++; tab-width: 4; indent-tabs-mode: t; c-file-style: "stroustrup"; -*-
// vi:set ts=4 sts=4 sw=4 noet cino+=(0 :
// Copyright 2012, The TPIE development team
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

///////////////////////////////////////////////////////////////////////////////
/// \file virtual.h  Virtual wrappers for pipe_segments
///////////////////////////////////////////////////////////////////////////////

#ifndef __TPIE_PIPELINING_VIRTUAL_H__
#define __TPIE_PIPELINING_VIRTUAL_H__

namespace tpie {

namespace pipelining {

namespace bits {

template <typename Input>
class virtsrc : public pipe_segment {
public:
	virtual const segment_token & get_token() = 0;
	virtual void push(Input v) = 0;
};

template <typename dest_t>
class virtsrc_impl : public virtsrc<typename dest_t::item_type> {
public:
	typedef typename dest_t::item_type item_type;

	dest_t dest;
	virtsrc_impl(const dest_t & dest)
		: dest(dest)
	{
		pipe_segment::add_push_destination(dest);
	}

	const segment_token & get_token() {
		return pipe_segment::get_token();
	}

	void push(item_type v) {
		dest.push(v);
	}
};

template <typename Output>
class virtrecv : public pipe_segment {
	virtrecv *& m_self;
public:
	typedef Output item_type;

	virtsrc<Output> * m_virtdest;

	virtrecv(virtrecv *& self)
		: m_self(self)
		, m_virtdest(0)
	{
		m_self = this;
	}

	virtrecv(const virtrecv & other)
		: pipe_segment(other)
		, m_self(other.m_self)
		, m_virtdest(other.m_virtdest)
	{
		m_self = this;
	}

	void begin() {
		tpie::log_info() << this << " begin " << m_virtdest << std::endl;
		pipe_segment::begin();
		if (m_virtdest == 0) {
			throw false;
		}
	}

	void push(Output v) {
		m_virtdest->push(v);
	}

	void set_destination(virtsrc<Output> & dest) {
		tp_assert(m_virtdest == 0, "Destination set twice");

		m_virtdest = &dest;
		add_push_destination(dest.get_token());
	}
};

} // namespace bits

class virtual_phase_base {
public:
	virtual ~virtual_phase_base() {}
};

template <typename Input>
class virtual_phase_end : public virtual_phase_base {
public:
	boost::shared_ptr<bits::virtsrc<Input> > m_src;

	virtual_phase_end() {}

	template <typename fact_t>
	virtual_phase_end(const pipe_end<fact_t> & pipe) {
		*this = pipe;
	}

	bits::virtsrc<Input> & get_src() {
		if (m_src.get() == 0) throw virtual_phase_missing_end();
		return *m_src;
	}

	template <typename fact_t>
	virtual_phase_end & operator=(const pipe_end<fact_t> & pipe) {
		tp_assert(m_src.get() == 0, "Virtual phase assigned twice");

		typedef typename fact_t::generated_type generated_type;
		m_src.reset(new bits::virtsrc_impl<generated_type>(pipe.factory.construct()));

		return *this;
	}
};

template <typename Input, typename Output>
struct set_empty_pipe_if_equal;

template <typename Input>
struct set_empty_pipe_if_equal<Input, Input> {
	typedef bits::virtrecv<Input> recv_type;
	inline static void set(std::auto_ptr<bits::virtsrc<Input> > & m_src, recv_type *& m_recv) {
		recv_type temp(m_recv);
		m_src.reset(new bits::virtsrc_impl<recv_type>(temp));
	}
};

template <typename Input, typename Output>
struct set_empty_pipe_if_equal {
	typedef bits::virtrecv<Input> recv_type;
	inline static void set(std::auto_ptr<bits::virtsrc<Input> > & /*m_src*/, recv_type *& /*m_recv*/) {
	}
};

template <typename Input, typename Output>
class virtual_phase_impl {
	typedef bits::virtrecv<Output> recv_type;
public:
	recv_type * m_recv;
	std::auto_ptr<bits::virtsrc<Input> > m_src;
	std::auto_ptr<virtual_phase_base> m_dest;

	virtual_phase_impl()
		: m_recv(0)
	{
	}
};

template <typename Input, typename Output>
class virtual_phase : public virtual_phase_base {
	typedef bits::virtrecv<Output> recv_type;
public:
	boost::shared_ptr<virtual_phase_impl<Input, Output> > impl;

	virtual_phase()
		: impl(new virtual_phase_impl<Input, Output>())
	{
	}

	template <typename fact_t>
	virtual_phase(const pipe_middle<fact_t> & pipe)
		: impl(new virtual_phase_impl<Input, Output>())
	{
		*this = pipe;
	}

	template <typename fact_t>
	virtual_phase & operator=(const pipe_middle<fact_t> & pipe) {
		tp_assert(impl->m_src.get() == 0, "Virtual phase assigned twice");

		typedef typename fact_t::template generated<recv_type>::type generated_type;
		recv_type temp(impl->m_recv);
		impl->m_src.reset(new bits::virtsrc_impl<generated_type>(pipe.factory.construct(temp)));

		return *this;
	}

	void set_empty_pipe() {
		tp_assert(impl->m_src.get() == 0, "Virtual phase assigned twice");
		set_empty_pipe_if_equal<Input, Output>::set(impl->m_src, impl->m_recv);
		if (impl->m_src.get() == 0) throw virtual_phase_missing_middle();
	}

	bits::virtsrc<Input> & get_src() {
		if (impl->m_src.get() == 0) set_empty_pipe();
		return *impl->m_src;
	}

	template <typename NextOutput>
	virtual_phase<Output, NextOutput> & operator|(virtual_phase<Output, NextOutput> dest) {
		if (impl->m_recv == 0) set_empty_pipe();
		impl->m_recv->set_destination(dest.get_src());
		virtual_phase<Output, NextOutput> * res = new virtual_phase<Output, NextOutput>(dest);
		impl->m_dest.reset(res);
		return *res;
	}

	virtual_phase_end<Output> & operator|(virtual_phase_end<Output> dest) {
		if (impl->m_recv == 0) set_empty_pipe();
		impl->m_recv->set_destination(dest.get_src());
		virtual_phase_end<Output> * res = new virtual_phase_end<Output>(dest);
		impl->m_dest.reset(res);
		return *res;
	}
};

template <typename Output>
class virtual_phase_begin_impl {
	typedef bits::virtrecv<Output> recv_type;
public:
	recv_type * m_recv;
	std::auto_ptr<pipe_segment> m_src;
	std::auto_ptr<virtual_phase_base> m_dest;
};

template <typename Output>
class virtual_phase_begin : public virtual_phase_base {
	typedef bits::virtrecv<Output> recv_type;
public:
	boost::shared_ptr<virtual_phase_begin_impl<Output> > impl;

	virtual_phase_begin()
		: impl(new virtual_phase_begin_impl<Output>())
	{
	}

	template <typename fact_t>
	virtual_phase_begin(const pipe_begin<fact_t> & pipe)
		: impl(new virtual_phase_begin_impl<Output>())
	{
		*this = pipe;
	}

	template <typename fact_t>
	virtual_phase_begin & operator=(const pipe_begin<fact_t> & pipe) {
		tp_assert(impl->m_src.get() == 0, "Virtual phase assigned twice");

		typedef typename fact_t::template generated<recv_type>::type generated_type;
		recv_type temp(impl->m_recv);
		impl->m_src.reset(new generated_type(pipe.factory.construct(temp)));

		return *this;
	}

	template <typename NextOutput>
	virtual_phase<Output, NextOutput> & operator|(virtual_phase<Output, NextOutput> dest) {
		if (impl->m_recv == 0) throw virtual_phase_missing_begin();
		impl->m_recv->set_destination(dest.get_src());
		virtual_phase<Output, NextOutput> * res = new virtual_phase<Output, NextOutput>(dest);
		impl->m_dest.reset(res);
		return *res;
	}

	virtual_phase_end<Output> & operator|(virtual_phase_end<Output> dest) {
		if (impl->m_recv == 0) throw virtual_phase_missing_begin();
		impl->m_recv->set_destination(dest.get_src());
		virtual_phase_end<Output> * res = new virtual_phase_end<Output>(dest);
		impl->m_dest.reset(res);
		return *res;
	}

	inline void operator()() {
		progress_indicator_null pi;
		(*this)(pi);
	}

	inline void operator()(progress_indicator_base & pi) {
		if (impl->m_src.get() == 0) throw virtual_phase_missing_begin();
		impl->m_src->go(pi);
	}
};

} // namespace pipelining

} // namespace tpie

#endif // __TPIE_PIPELINING_VIRTUAL_H__