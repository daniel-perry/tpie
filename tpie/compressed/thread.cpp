// -*- mode: c++; tab-width: 4; indent-tabs-mode: t; c-file-style: "stroustrup"; -*-
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

#include <queue>
#include <snappy.h>
#include <tpie/compressed/thread.h>
#include <tpie/compressed/request.h>
#include <tpie/compressed/buffer.h>

namespace tpie {

class compressor_thread::impl {
public:
	impl()
		: m_done(false)
	{
	}

	void stop(compressor_thread_lock & /*lock*/) {
		m_done = true;
		m_newRequest.notify_one();
	}

	bool request_valid(const compressor_request & r) {
		switch (r.kind()) {
			case compressor_request_kind::NONE:
				return false;
			case compressor_request_kind::READ:
			case compressor_request_kind::WRITE:
				return true;
		}
		throw exception("Unknown request type");
	}

	void run() {
		while (true) {
			compressor_thread_lock::lock_t lock(mutex());
			while (!m_done && m_requests.empty()) m_newRequest.wait(lock);
			if (m_done && m_requests.empty()) break;
			{
				compressor_request r = m_requests.front();
				m_requests.pop();
				lock.unlock();

				switch (r.kind()) {
					case compressor_request_kind::NONE:
						throw exception("Invalid request");
					case compressor_request_kind::READ:
						process_read_request(r.get_read_request());
						break;
					case compressor_request_kind::WRITE:
						process_write_request(r.get_write_request());
						break;
				}
			}
			lock.lock();
			m_requestDone.notify_all();
		}
	}

private:
	void process_read_request(read_request & rr) {
		// Note the blockSize/readOffset semantics defined in a docstring for
		// compressed_stream::m_nextReadOffset.
		stream_size_type blockSize = rr.block_size();
		stream_size_type readOffset = rr.read_offset();
		if (blockSize == 0) {
			memory_size_type nRead = rr.file_accessor().read(readOffset, &blockSize, sizeof(blockSize));
			if (nRead != sizeof(blockSize)) {
				rr.set_end_of_stream();
				return;
			}
			readOffset += sizeof(blockSize);
		}
		if (blockSize == 0) {
			throw exception("Block size was unexpectedly zero");
		}
		array<char> scratch(blockSize + sizeof(blockSize));
		memory_size_type nRead = rr.file_accessor().read(readOffset, scratch.get(), blockSize + sizeof(blockSize));
		stream_size_type nextBlockSize;
		stream_size_type nextReadOffset;
		if (nRead == blockSize + sizeof(blockSize)) {
			// This might be unaligned! Watch out.
			nextBlockSize = *(reinterpret_cast<memory_size_type *>(scratch.get() + blockSize));
			nextReadOffset = readOffset + blockSize + sizeof(blockSize);
		} else if (nRead == blockSize) {
			nextBlockSize = 0;
			nextReadOffset = readOffset + blockSize;
		} else {
			rr.set_end_of_stream();
			return;
		}

		size_t uncompressedLength;
		if (!snappy::GetUncompressedLength(scratch.get(),
										   blockSize,
										   &uncompressedLength))
			throw stream_exception("Internal error; snappy::GetUncompressedLength failed");
		if (uncompressedLength > rr.buffer()->capacity()) {
			log_error() << uncompressedLength << ' ' << rr.buffer()->capacity() << std::endl;
			throw stream_exception("Internal error; snappy::GetUncompressedLength exceeds the block size");
		}
		rr.buffer()->set_size(uncompressedLength);
		snappy::RawUncompress(scratch.get(),
							  blockSize,
							  reinterpret_cast<char *>(rr.buffer()->get()));

		compressor_thread_lock::lock_t lock(mutex());
		rr.set_next_block(nextReadOffset, nextBlockSize);
	}

	void process_write_request(write_request & wr) {
		size_t inputLength = wr.buffer()->size();
		memory_size_type blockSize = snappy::MaxCompressedLength(inputLength);
		array<char> scratch(sizeof(blockSize) + blockSize);
		snappy::RawCompress(reinterpret_cast<const char *>(wr.buffer()->get()),
							inputLength,
							scratch.get() + sizeof(blockSize),
							&blockSize);
		*reinterpret_cast<memory_size_type *>(scratch.get()) = blockSize;
		{
			compressor_thread_lock::lock_t lock(mutex());
			wr.set_block_info(wr.file_accessor().file_size() + sizeof(blockSize),
							  blockSize);
		}
		wr.file_accessor().append(scratch.get(), sizeof(blockSize) + blockSize);
		wr.file_accessor().increase_size(wr.block_items());
	}

public:
	mutex_t & mutex() {
		return m_mutex;
	}

	void request(const compressor_request & r) {
		if (!request_valid(r))
			throw exception("Invalid request");

		m_requests.push(r);
		m_requests.back().get_request_base().initiate_request();
		m_newRequest.notify_one();
	}

	void wait_for_request_done(compressor_thread_lock & l) {
		m_requestDone.wait(l.get_lock());
	}

private:
	mutex_t m_mutex;
	std::queue<compressor_request> m_requests;
	boost::condition_variable m_newRequest;
	boost::condition_variable m_requestDone;
	bool m_done;
};

} // namespace tpie

namespace {

tpie::compressor_thread the_compressor_thread;
boost::thread the_compressor_thread_handle;
bool compressor_thread_already_finished = false;

void run_the_compressor_thread() {
	the_compressor_thread.run();
}

} // unnamed namespace

namespace tpie {

compressor_thread & the_compressor_thread() {
	return ::the_compressor_thread;
}

void init_compressor() {
	if (the_compressor_thread_handle != boost::thread()) {
		log_debug() << "Attempted to initiate compressor thread twice" << std::endl;
		return;
	}
	boost::thread t(run_the_compressor_thread);
	the_compressor_thread_handle.swap(t);
	compressor_thread_already_finished = false;
}

void finish_compressor() {
	if (the_compressor_thread_handle == boost::thread()) {
		if (compressor_thread_already_finished) {
			log_debug() << "Compressor thread already finished" << std::endl;
		} else {
			log_debug() << "Attempted to finish compressor thread that was never initiated" << std::endl;
		}
		return;
	}
	{
		compressor_thread_lock lock(the_compressor_thread());
		the_compressor_thread().stop(lock);
	}
	the_compressor_thread_handle.join();
	boost::thread t;
	the_compressor_thread_handle.swap(t);
	compressor_thread_already_finished = true;
}

compressor_thread::compressor_thread()
	: pimpl(new impl)
{
}

compressor_thread::~compressor_thread() {
	delete pimpl;
}

compressor_thread::mutex_t & compressor_thread::mutex() {
	return pimpl->mutex();
}

void compressor_thread::request(compressor_request & r) {
	pimpl->request(r);
}

void compressor_thread::run() {
	pimpl->run();
}

void compressor_thread::wait_for_request_done(compressor_thread_lock & l) {
	pimpl->wait_for_request_done(l);
}

void compressor_thread::stop(compressor_thread_lock & lock) {
	pimpl->stop(lock);
}

}
