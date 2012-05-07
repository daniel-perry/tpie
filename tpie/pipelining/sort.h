// -*- mode: c++; tab-width: 4; indent-tabs-mode: t; eval: (progn (c-set-style "stroustrup") (c-set-offset 'innamespace 0)); -*-
// vi:set ts=4 sts=4 sw=4 noet :
// Copyright 2011, 2012, The TPIE development team
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

#ifndef __TPIE_PIPELINING_SORT_H__
#define __TPIE_PIPELINING_SORT_H__

#include <tpie/pipelining/core.h>
#include <tpie/pipelining/factory_helpers.h>
#include <tpie/sort.h>
#include <tpie/parallel_sort.h>
#include <tpie/file_stream.h>
#include <tpie/tempname.h>
#include <tpie/memory.h>
#include <queue>

namespace tpie {

namespace pipelining {

struct sort_parameters {
	memory_size_type runLength;
	memory_size_type fanout;
};

template <typename T>
struct merger {
	inline bool can_pull() {
		return !pq.empty();
	}

	inline T pull() {
		T el = pq.top().first;
		size_t i = pq.top().second;
		pq.pop();
		if (in[i].can_read() && itemsRead[i] < runLength) {
			pq.push(std::make_pair(in[i].read(), i));
			++itemsRead[i];
		}
		return el;
	}

	inline void reset(array<file_stream<T> > & inputs, size_t runLength) {
		this->runLength = runLength;
		tp_assert(pq.empty(), "Reset before we are done");
		n = inputs.size();
		in.swap(inputs);
		for (size_t i = 0; i < n; ++i) {
			pq.push(std::make_pair(in[i].read(), i));
		}
		itemsRead.resize(0);
		itemsRead.resize(n, 1);
	}

private:
	std::priority_queue<std::pair<T, size_t> > pq;
	array<file_stream<T> > in;
	std::vector<size_t> itemsRead;
	size_t runLength;
	size_t n;
};

template <typename T>
struct merge_sorter {
	inline merge_sorter() {
		calculate_parameters();
	}

	inline void begin() {
		m_currentRunItems.resize(p.runLength);
		m_runFiles.resize(p.fanout*2);
		m_currentRunItemCount = 0;
		m_finishedRuns = 0;
	}

	inline void push(const T & item) {
		if (m_currentRunItemCount >= p.runLength) {
			sort_current_run();
			empty_current_run();
		}
		m_currentRunItems[m_currentRunItemCount] = item;
		++m_currentRunItemCount;
	}

	inline void end() {
		sort_current_run();
		if (m_finishedRuns == 0) {
			m_reportInternal = true;
			m_itemsPulled = 0;
		} else {
			m_reportInternal = false;
			empty_current_run();
			prepare_pull();
		}
	}

	inline void sort_current_run() {
		parallel_sort(m_currentRunItems.begin(), m_currentRunItems.begin()+m_currentRunItemCount, std::less<T>());
	}

	// postcondition: m_currentRunItemCount = 0
	inline void empty_current_run() {
		file_stream<T> fs;
		//TP_LOG_DEBUG_ID("Empty run no. " << m_finishedRuns);
		open_run_file(fs, 0, m_finishedRuns, true);
		for (size_t i = 0; i < m_currentRunItemCount; ++i) {
			fs.write(m_currentRunItems[i]);
		}
		m_currentRunItemCount = 0;
		++m_finishedRuns;
	}

	// merge the runNumber'th to the (runNumber+runCount)'th run in mergeLevel
	inline void initialize_merger(size_t mergeLevel, size_t runNumber, size_t runCount) {
		//TP_LOG_DEBUG_ID("Initialize merger at level " << mergeLevel << " from run " << runNumber << " with " << runCount << " runs");
		array<file_stream<T> > in(runCount);
		for (size_t i = 0; i < runCount; ++i) {
			//TP_LOG_DEBUG_ID(runNumber+i);
			open_run_file(in[i], mergeLevel, runNumber+i, false);
		}
		m_merger.reset(in, p.runLength);
	}

	inline void merge_runs(size_t mergeLevel, size_t runNumber, size_t runCount) {
		initialize_merger(mergeLevel, runNumber, runCount);
		file_stream<T> out;
		//TP_LOG_DEBUG_ID("Merging into level " << mergeLevel+1 << ", run number " << runNumber/runCount);
		open_run_file(out, mergeLevel+1, runNumber/runCount, true);
		while (m_merger.can_pull()) {
			out.write(m_merger.pull());
		}
	}

	// merge all runs and initialize merger for public pulling
	inline void prepare_pull() {
		size_t mergeLevel = 0;
		size_t runCount = m_finishedRuns;
		while (runCount > p.fanout) {
			TP_LOG_WARNING_ID("Level " << mergeLevel << " has " << runCount << " runs");
			size_t newRunCount = 0;
			for (size_t i = 0; i < runCount; i += p.fanout) {
				merge_runs(mergeLevel, i, p.fanout);
				++newRunCount;
			}
			++mergeLevel;
			runCount = newRunCount;
		}
		//TP_LOG_DEBUG_ID("Final level " << mergeLevel << " has " << runCount << " runs");
		initialize_merger(mergeLevel, 0, runCount);
	}

	inline bool can_pull() {
		if (m_reportInternal) return m_itemsPulled < m_currentRunItemCount;
		else return m_merger.can_pull();
	}

	inline T pull() {
		if (m_reportInternal && m_itemsPulled < m_currentRunItemCount) return m_currentRunItems[m_itemsPulled++];
		else return m_merger.pull();
	}

private:
	inline void calculate_parameters() {
		p.runLength = 64;
		p.fanout = 4;
	}

	// forWriting = false: open an existing run and seek to correct offset
	// forWriting = true: open run file and seek to end
	inline void open_run_file(file_stream<T> & fs, size_t mergeLevel, size_t runNumber, bool forWriting) {
		size_t idx = (mergeLevel % 2)*p.fanout + (runNumber % p.fanout);
		//TP_LOG_DEBUG_ID("mrglvl " << mergeLevel << " run no. " << runNumber << " has index " << idx);
		if (forWriting) {
			if (runNumber < p.fanout) m_runFiles[idx].free();
			fs.open(m_runFiles[idx], file_base::read_write);
			fs.seek(0, file_base::end);
		} else {
			fs.open(m_runFiles[idx], file_base::read);
			//TP_LOG_DEBUG_ID("seek to " << p.runLength * (runNumber / p.fanout) << " stream size " << fs.size());
			fs.seek(p.runLength * (runNumber / p.fanout), file_base::beginning);
		}
	}

	sort_parameters p;

	merger<T> m_merger;

	array<temp_file> m_runFiles;

	// number of runs already written to disk.
	size_t m_finishedRuns;

	// current run buffer. size 0 before begin(), size runLength after begin().
	array<T> m_currentRunItems;

	// number of items in current run buffer.
	size_t m_currentRunItemCount;

	bool m_reportInternal;
	
	// when doing internal reporting: the number of items already reported
	size_t m_itemsPulled;
};

template <typename dest_t>
struct sort_t : public pipe_segment {
	///////////////////////////////////////////////////////////////////////////
	/// \brief Virtual dtor.
	///////////////////////////////////////////////////////////////////////////
	~sort_t() {}

	typedef typename dest_t::item_type item_type;

	inline sort_t(const dest_t & dest) : dest(dest) {
	}

	inline sort_t(const sort_t<dest_t> & other) : dest(other.dest) {
		// don't copy tmpfile or tmpstream
	}

	inline void begin() {
		m_sorter.begin();
	}

	inline void push(const item_type & item) {
		m_sorter.push(item);
	}

	inline void end() {
		m_sorter.end();
		dest.begin();
		while (m_sorter.can_pull()) {
			dest.push(m_sorter.pull());
		}
		dest.end();
	}

	void push_successors(std::deque<const pipe_segment *> & q) const {
		q.push_back(&dest);
	}

private:
	dest_t dest;
	merge_sorter<item_type> m_sorter;
};

inline pipe_middle<factory_0<sort_t> >
pipesort() {
	return factory_0<sort_t>();
}

template <typename T, typename pred_t>
struct passive_sorter {
	inline passive_sorter()
		: current_output(0)
	{
	}

	struct output_t;

	struct input_t : public pipe_segment {
		typedef T item_type;

		inline input_t(temp_file * file, output_t ** current_output)
			: file(file)
			, current_output(current_output)
		{
		}

		inline void begin() {
			pbuffer = tpie_new<file_stream<T> >();
			pbuffer->open(*file);
		}

		inline void push(const T & item) {
			pbuffer->write(item);
		}

		inline void end() {
			pbuffer->seek(0);
			pred_t pred;
			progress_indicator_null pi;
			sort(*pbuffer, *pbuffer, pred, pi);
			pbuffer->close();

			tpie_delete(pbuffer);
		}

		void push_successors(std::deque<const pipe_segment *> & q) const {
			q.push_back(*current_output);
		}

		bool buffering() const {
			return true;
		}

	private:
		temp_file * file;
		file_stream<T> * pbuffer;
		output_t ** current_output;

		input_t();
		input_t & operator=(const input_t &);
	};

	struct output_t : public pipe_segment {
		typedef T item_type;

		inline output_t(temp_file * file, output_t ** current_output)
			: file(file)
			, current_output(current_output)
		{
			*current_output = this;
		}

		inline output_t(const output_t & other)
			: file(other.file)
			, current_output(other.current_output)
		{
			*current_output = this;
		}

		inline void begin() {
			buffer = tpie_new<file_stream<T> >();
			buffer->open(*file);
		}

		inline bool can_pull() {
			return buffer->can_read();
		}

		inline T pull() {
			return buffer->read();
		}

		inline void end() {
			buffer->close();
		}

		void push_successors(std::deque<const pipe_segment *> &) const { }

	private:
		temp_file * file;
		file_stream<T> * buffer;
		output_t ** current_output;

		output_t();
		output_t & operator=(const output_t &);
	};

	inline pipe_end<termfactory_2<input_t, temp_file *, output_t **> > input() {
		std::cout << "Construct input factory " << typeid(pred_t).name() << " with " << &file << std::endl;
		return termfactory_2<input_t, temp_file *, output_t **>(&file, &current_output);
	}

	inline output_t output() {
		return output_t(&file, &current_output);
	}

private:
	pred_t pred;
	temp_file file;
	output_t * current_output;
	passive_sorter(const passive_sorter &);
	passive_sorter & operator=(const passive_sorter &);
};

}

}

#endif
