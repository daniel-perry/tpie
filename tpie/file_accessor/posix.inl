// -*- mode: c++; tab-width: 4; indent-tabs-mode: t; c-file-style: "stroustrup"; -*-
// vi:set ts=4 sts=4 sw=4 noet :
// Copyright 2009, 2010, The TPIE development team
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
#include <tpie/config.h>
#include <string.h>
#include <tpie/exception.h>
#include <tpie/file_count.h>
#include <tpie/file_accessor/posix.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <iostream>
#include <sstream>

namespace tpie {
namespace file_accessor {

void posix::throw_errno() {
	if (errno == ENOSPC) throw out_of_space_exception(strerror(errno));
	else throw io_exception(strerror(errno));
}

posix::posix()
	: m_fd(0)
	, m_cacheHint(access_normal)
{
}

inline void posix::set_cache_hint(cache_hint cacheHint) {
	m_cacheHint = cacheHint;
}

inline void posix::give_advice() {
#ifndef __MACH__
	int advice;
	switch (m_cacheHint) {
		case access_normal:
			advice = POSIX_FADV_NORMAL;
			break;
		case access_sequential:
			advice = POSIX_FADV_SEQUENTIAL;
			break;
		case access_random:
			advice = POSIX_FADV_RANDOM;
			break;
		default:
			advice = POSIX_FADV_NORMAL;
			break;
	}
	::posix_fadvise(m_fd, 0, 0, advice);
#endif // __MACH__
}

inline void posix::read_i(void * data, memory_size_type size) {
	memory_offset_type bytesRead = ::read(m_fd, data, size);
	if (bytesRead == -1)
		throw_errno();
	if (bytesRead != static_cast<memory_offset_type>(size))
		throw io_exception("Wrong number of bytes read");
	increment_bytes_read(size);
}

inline void posix::write_i(const void * data, memory_size_type size) {
	if (::write(m_fd, data, size) != (memory_offset_type)size) throw_errno();
	increment_bytes_written(size);
}

inline void posix::seek_i(stream_size_type size) {
	if (::lseek(m_fd, size, SEEK_SET) == -1) throw_errno();
}

void posix::open_wo(const std::string & path) {
	m_fd = ::open(path.c_str(), O_RDWR | O_TRUNC | O_CREAT,  S_IRUSR | S_IWUSR);
	if (m_fd == -1) throw_errno();
	give_advice();
}

void posix::open_ro(const std::string & path) {
	m_fd = ::open(path.c_str(), O_RDONLY);
	if (m_fd == -1) throw_errno();
	give_advice();
}

bool posix::try_open_rw(const std::string & path) {
	m_fd = ::open(path.c_str(), O_RDWR);
	if (m_fd == -1) {
		if (errno != ENOENT) throw_errno();
		return false;
	}
	give_advice();
	return true;
}

void posix::open_rw_new(const std::string & path) {
	m_fd = ::open(path.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
	if (m_fd == -1) throw_errno();
	give_advice();
}

bool posix::is_open() const {
	return m_fd != 0;
}

void posix::close_i() {
	if (m_fd != 0) {
		::close(m_fd);
	}
	m_fd=0;
}

void posix::truncate_i(stream_size_type bytes) {
	if (ftruncate(m_fd, bytes) == -1) throw_errno();
}

}
}
