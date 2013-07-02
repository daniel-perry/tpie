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

#ifndef TPIE_COMPRESSED_POSITION_H
#define TPIE_COMPRESSED_POSITION_H

///////////////////////////////////////////////////////////////////////////////
/// \file compressed/position.h
///////////////////////////////////////////////////////////////////////////////

#include <tpie/compressed/predeclare.h>

namespace tpie {

class stream_position {
private:
	template <typename T>
	friend class compressed_stream;

	uint64_t m_readOffset;
	uint64_t m_blockNumber;
	uint64_t m_offset;
	uint32_t m_blockItemIndex;

	stream_position(stream_size_type readOffset,
					memory_size_type blockItemIndex,
					stream_size_type blockNumber,
					stream_size_type offset)
		: m_readOffset(readOffset)
		, m_blockNumber(blockNumber)
		, m_offset(offset)
		, m_blockItemIndex(static_cast<uint32_t>(blockItemIndex))
	{
		if (m_blockItemIndex != blockItemIndex)
			throw exception("stream_position: Block item index out of bounds");
	}

	stream_size_type read_offset() const {
		return m_readOffset;
	}

	memory_size_type block_item_index() const {
		return m_blockItemIndex;
	}

	stream_size_type block_number() const {
		return m_blockNumber;
	}

	stream_size_type offset() const {
		return m_offset;
	}

	void advance_item() {
		++m_blockItemIndex;
		++m_offset;
	}
};

} // namespace tpie

#endif // TPIE_COMPRESSED_POSITION_H
