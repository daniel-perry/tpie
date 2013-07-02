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

#ifndef TPIE_COMPRESSED_STREAM_H
#define TPIE_COMPRESSED_STREAM_H

///////////////////////////////////////////////////////////////////////////////
/// \file compressed/stream.h
///////////////////////////////////////////////////////////////////////////////

#include <snappy.h>
#include <tpie/array.h>
#include <tpie/tempname.h>
#include <tpie/file_base_crtp.h>
#include <tpie/file_stream_base.h>
#include <tpie/file_accessor/byte_stream_accessor.h>
#include <tpie/compressed/thread.h>
#include <tpie/compressed/buffer.h>
#include <tpie/compressed/request.h>
#include <tpie/compressed/position.h>

namespace tpie {

class compressed_stream_base {
public:
	typedef boost::shared_ptr<compressor_buffer> buffer_t;

protected:
	struct seek_state {
		enum type {
			none,
			beginning,
			end,
			position
		};
	};

	struct buffer_state {
		enum type {
			write_only,
			read_only
		};
	};

	compressed_stream_base(memory_size_type itemSize,
						   double blockFactor)
		: m_bufferDirty(false)
		, m_blockItems(block_size(blockFactor) / itemSize)
		, m_blockSize(block_size(blockFactor))
		, m_canRead(false)
		, m_canWrite(false)
		, m_open(false)
		, m_itemSize(itemSize)
		, m_ownedTempFile()
		, m_tempFile(0)
		, m_size(0)
		, m_buffers(m_blockSize)
	{
	}

	virtual void flush_block() = 0;

	virtual void post_open() = 0;

	void open_inner(const std::string & path,
					access_type accessType,
					memory_size_type userDataSize,
					cache_hint cacheHint)
	{
		if (userDataSize != 0)
			throw stream_exception("Compressed stream does not support user data");

		m_canRead = accessType == access_read || accessType == access_read_write;
		m_canWrite = accessType == access_write || accessType == access_read_write;
		m_byteStreamAccessor.open(path, m_canRead, m_canWrite, m_itemSize, m_blockSize, userDataSize, cacheHint);
		m_size = m_byteStreamAccessor.size();
		m_open = true;

		this->post_open();
	}

	stream_size_type size() const;

	compressor_thread & compressor() {
		return the_compressor_thread();
	}

public:
	bool is_readable() const throw() {
		return m_canRead;
	}

	bool is_writable() const throw() {
		return m_canWrite;
	}

	static memory_size_type block_size(double blockFactor) throw () {
		return static_cast<memory_size_type>(get_block_size() * blockFactor);
	}

	static double calculate_block_factor(memory_size_type blockSize) throw () {
		return (double)blockSize / (double)block_size(1.0);
	}

	static memory_size_type block_memory_usage(double blockFactor) {
		return block_size(blockFactor);
	}

	memory_size_type block_items() const {
		return m_blockItems;
	}

	memory_size_type block_size() const {
		return m_blockSize;
	}

	template <typename TT>
	void read_user_data(TT & /*data*/) {
		throw stream_exception("Compressed stream does not support user data");
	}

	memory_size_type read_user_data(void * /*data*/, memory_size_type /*count*/) {
		throw stream_exception("Compressed stream does not support user data");
	}

	template <typename TT>
	void write_user_data(const TT & /*data*/) {
		throw stream_exception("Compressed stream does not support user data");
	}

	void write_user_data(const void * /*data*/, memory_size_type /*count*/) {
		throw stream_exception("Compressed stream does not support user data");
	}

	memory_size_type user_data_size() const {
		return 0;
	}

	memory_size_type max_user_data_size() const {
		return 0;
	}

	const std::string & path() const {
		assert(m_open);
		return m_byteStreamAccessor.path();
	}

	void open(const std::string & path,
			  access_type accessType = access_read_write,
			  memory_size_type userDataSize = 0,
			  cache_hint cacheHint=access_sequential)
	{
		close();
		open_inner(path, accessType, userDataSize, cacheHint);
	}

	void open(memory_size_type userDataSize = 0,
			  cache_hint cacheHint = access_sequential)
	{
		close();
		m_ownedTempFile.reset(tpie_new<temp_file>());
		m_tempFile = m_ownedTempFile.get();
		open_inner(m_tempFile->path(), access_read_write, userDataSize, cacheHint);
	}

	void open(temp_file & file,
			  access_type accessType = access_read_write,
			  memory_size_type userDataSize = 0,
			  cache_hint cacheHint = access_sequential)
	{
		close();
		m_tempFile = &file;
		open_inner(m_tempFile->path(), accessType, userDataSize, cacheHint);
	}

	void close() {
		if (m_open) {
			if (m_bufferDirty) {
				flush_block();
			}
			m_buffer.reset();

			compressor_thread_lock l(compressor());
			finish_requests(l);

			m_byteStreamAccessor.close();
		}
		m_open = false;
		m_tempFile = NULL;
		m_ownedTempFile.reset();
	}

protected:
	void finish_requests(compressor_thread_lock & l) {
		m_buffers.clean();
		while (!m_buffers.empty()) {
			compressor().wait_for_request_done(l);
			m_buffers.clean();
		}
	}

public:
	bool is_open() const {
		return m_open;
	}

	stream_size_type file_size() const {
		return m_size;
	}

protected:
	bool m_bufferDirty;
	memory_size_type m_blockItems;
	memory_size_type m_blockSize;
	bool m_canRead;
	bool m_canWrite;
	bool m_open;
	memory_size_type m_itemSize;
	file_accessor::byte_stream_accessor<default_raw_file_accessor> m_byteStreamAccessor;
	tpie::auto_ptr<temp_file> m_ownedTempFile;
	temp_file * m_tempFile;
	stream_size_type m_size;
	buffer_t m_buffer;
	stream_buffers m_buffers;
};

namespace ami {
	template <typename T> class cstream;
}

template <typename T>
class compressed_stream : public compressed_stream_base {
	using compressed_stream_base::seek_state;
	using compressed_stream_base::buffer_state;

	friend class ami::cstream<T>;

	static const file_stream_base::offset_type beginning = file_stream_base::beginning;
	static const file_stream_base::offset_type end = file_stream_base::end;

public:
	typedef T item_type;
	typedef file_stream_base::offset_type offset_type;

	compressed_stream(double blockFactor=1.0)
		: compressed_stream_base(sizeof(T), blockFactor)
		, m_seekState(seek_state::beginning)
		, m_bufferState(buffer_state::write_only)
		, m_position(stream_position(0, 0, 0, 0))
		, m_nextReadOffset(0)
		, m_nextBlockSize(0)
		, m_streamBlocks(0)
	{
	}

	~compressed_stream() {
		this->close();
	}

	void describe(std::ostream & out) {
		if (!this->is_open()) {
			out << "[Closed stream]";
			return;
		}

		out << "[(" << m_byteStreamAccessor.path() << ") item " << offset()
			<< " of " << size();
		out << " (block " << m_position.block_number()
			<< " @ byte " << m_position.read_offset()
			<< ", item " << m_position.block_item_index()
			<< ")";

		switch (m_seekState) {
			case seek_state::none:
				break;
			case seek_state::beginning:
				out << ", seeking to beginning";
				break;
			case seek_state::end:
				out << ", seeking to end";
				break;
			case seek_state::position:
				out << ", seeking to position";
				break;
		}

		switch (m_bufferState) {
			case buffer_state::write_only:
				out << ", buffer write-only";
				break;
			case buffer_state::read_only:
				out << ", buffer read-only";
				break;
		}

		if (m_bufferDirty)
			out << " dirty";

		if (can_read()) out << ", can read";
		else out << ", cannot read";

		out << ", at least " << m_streamBlocks << " blocks";

		out << ']';
	}

	std::string describe() {
		std::stringstream ss;
		describe(ss);
		return ss.str();
	}

	virtual void post_open() override {
		seek(0);
	}

	void seek(stream_offset_type offset, offset_type whence=beginning) {
		assert(this->is_open());
		if (whence == beginning && offset == 0) {
			m_seekState = seek_state::beginning;
			if (size() > 0)
				m_bufferState = buffer_state::read_only;
			else
				m_bufferState = buffer_state::write_only;
			m_position = stream_position(0, 0, 0, 0);
		} else if (whence == end && offset == 0) {
			m_seekState = seek_state::end;
			m_bufferState = buffer_state::write_only;
			// TODO m_position = ?
			//m_offset = size();
		} else {
			throw stream_exception("Random seeks are not supported");
		}
	}

	stream_size_type offset() const {
		return m_position.offset();
	}

	stream_size_type size() const {
		stream_size_type sz = m_byteStreamAccessor.size();
		if (m_bufferDirty) sz += (m_nextItem - m_bufferBegin);
		return sz;
	}

	void truncate(stream_size_type offset) {
		if (offset == size()) return;
		if (offset != 0)
			throw stream_exception("Arbitrary truncate is not supported");

		// No need to flush block
		m_buffer.reset();
		compressor_thread_lock l(compressor());
		finish_requests(l);

		m_byteStreamAccessor.truncate(0);
		seek(0);
	}

	stream_position get_position() {
		switch (m_bufferState) {
			case buffer_state::read_only:
				return m_position;
			case buffer_state::write_only:
				throw exception("What do?");
		}
		throw exception("get_position: Unreachable statement");
	}

	void set_position(const stream_position & pos) {
		m_position = pos;
		m_seekState = seek_state::position;
	}

private:
	const T & read_ref() {
		if (m_seekState != seek_state::none) perform_seek();
		if (!can_read()) throw stream_exception("!can_read()");
		if (m_nextItem == m_lastItem) {
			compressor_thread_lock l(compressor());
			read_next_block(l, m_position.block_number() + 1);
		} else {
			m_position.advance_item();
		}
		return *m_nextItem++;
	}

public:
	T read() {
		return read_ref();
	}

	template <typename IT>
	void read(IT const a, IT const b) {
		for (IT i = a; i != b; ++i) *i = read();
	}

	bool can_read() {
		if (!this->m_open)
			return false;

		if (m_nextReadOffset == 0 && m_byteStreamAccessor.size() > 0)
			return true;

		if (m_seekState != seek_state::none)
			perform_seek();

		if (m_bufferState != buffer_state::read_only)
			return false;

		if (m_nextItem != m_lastItem)
			return true;

		if (m_nextBlockSize != 0)
			return true;

		return false;
	}

	void write(const T & item) {
		if (m_seekState != seek_state::none) perform_seek();
		if (m_bufferState != buffer_state::write_only)
			throw stream_exception("Non-appending write attempted");
		if (m_nextItem == m_bufferEnd) {
			flush_block();
			m_nextItem = m_bufferBegin;
		}
		*m_nextItem++ = item;
		this->m_bufferDirty = true;
		++m_offset;
	}

	template <typename IT>
	void write(IT const a, IT const b) {
		for (IT i = a; i != b; ++i) write(*i);
	}

protected:
	void perform_seek() {
		if (m_seekState == seek_state::none)
			return;

		if (this->m_bufferDirty) {
			flush_block();
		}

		m_buffer.reset();
		compressor_thread_lock l(compressor());
		finish_requests(l);

		if (m_seekState == seek_state::beginning
			&& !m_byteStreamAccessor.empty())
		{
			m_nextReadOffset = 0;
			read_next_block(l, 0);
			m_bufferState = buffer_state::read_only;
		} else if (m_seekState == seek_state::position) {
			m_nextReadOffset = m_position.read_offset();
			m_nextBlockSize = 0;
			memory_size_type blockItemIndex = m_position.block_item_index();
			read_next_block(l, m_position.block_number());

			memory_size_type blockItems = m_lastItem - m_bufferBegin;

			if (blockItemIndex > blockItems) {
				throw exception("perform_seek: Item offset out of bounds");
			} else if (blockItemIndex == blockItems) {
				throw exception("perform_seek: Item offset at bounds");
			}

			m_nextItem = m_bufferBegin + blockItemIndex;
			m_bufferState = buffer_state::read_only;
		} else {
			get_buffer(l, m_streamBlocks++);
			m_bufferState = buffer_state::write_only;
			m_nextItem = m_bufferBegin;
		}
		m_seekState = seek_state::none;
	}

private:
	void get_buffer(compressor_thread_lock & l, stream_size_type blockNumber) {
		m_buffer = this->m_buffers.get_buffer(l, blockNumber);
		m_bufferBegin = reinterpret_cast<T *>(m_buffer->get());
		m_bufferEnd = m_bufferBegin + block_items();
		this->m_bufferDirty = false;
	}

public:
	virtual void flush_block() override {
		memory_size_type blockItems = m_nextItem - m_bufferBegin;
		m_buffer->set_size(blockItems * sizeof(T));
		compressor_request r;
		r.set_write_request(m_buffer,
							&m_byteStreamAccessor,
							blockItems,
							m_streamBlocks,
							&m_response);
		compressor_thread_lock lock(compressor());
		compressor().request(r);
		get_buffer(lock, m_streamBlocks++);
	}

	void read_next_block(compressor_thread_lock & lock, stream_size_type blockNumber) {
		get_buffer(lock, blockNumber);

		compressor_request r;
		r.set_read_request(m_buffer,
						   &m_byteStreamAccessor,
						   m_nextReadOffset,
						   m_nextBlockSize,
						   &m_response);

		compressor().request(r);
		while (!m_response.done()) {
			m_response.wait(lock);
		}
		if (m_response.end_of_stream())
			throw end_of_stream_exception();

		if (blockNumber >= m_streamBlocks)
			m_streamBlocks = blockNumber + 1;

		m_position = stream_position(m_nextReadOffset, 0, blockNumber, m_nextBlockSize);

		m_nextReadOffset = m_response.next_read_offset();
		m_nextBlockSize = m_response.next_block_size();
		m_nextItem = m_bufferBegin;
		memory_size_type itemsRead = m_buffer->size() / sizeof(T);
		m_lastItem = m_bufferBegin + itemsRead;
		m_bufferState = buffer_state::read_only;
	}

private:
	seek_state::type m_seekState;

	buffer_state::type m_bufferState;

	T * m_bufferBegin;
	T * m_bufferEnd;

	/** Next item in buffer to read/write. */
	T * m_nextItem;
	/** In read mode only: End of readable buffer. */
	T * m_lastItem;

	stream_position m_position;

	/** If nextBlockSize is zero,
	 * the size of the block to read is the first eight bytes,
	 * and the block begins after those eight bytes.
	 * If nextBlockSize is non-zero,
	 * the next block begins at the given offset and has the given size.
	 */
	stream_size_type m_nextReadOffset;
	stream_size_type m_nextBlockSize;

	stream_size_type m_offset;

	stream_size_type m_streamBlocks;

	compressor_response m_response;
};

} // namespace tpie

#endif // TPIE_COMPRESSED_STREAM_H
