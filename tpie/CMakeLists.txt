set_property(GLOBAL PROPERTY USE_FOLDERS On)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/sysinfo.cpp.in" "${CMAKE_CURRENT_BINARY_DIR}/sysinfo.cpp" @ONLY)

set (HEADERS
		access_type.h
		backtrace.h
		cache_hint.h
		comparator.h
		config.h.cmake
		cpu_timer.h
		deprecated.h
		disjoint_sets.h
		exception.h
		err.h
		file.h
		file_base.h
		file_base_crtp.h
		file_stream.h
		file_stream_base.h
		file_accessor/file_accessor.h
		file_accessor/stream_accessor.h
		file_accessor/stream_accessor.inl
		file_count.h
		execution_time_predictor.h
		imported/cycle.h
		internal_sort.h
		internal_queue.h
		internal_stack.h
		internal_vector.h
		internal_stack_vector_base.h
		job.h
		loglevel.h
		logstream.h
		merge.h
		mergeheap.h
		merge_sorted_runs.h
		memory.h
		memory.inl
		persist.h
		pipelining/buffer.h
		pipelining/exception.h
		pipelining/factory_base.h
		pipelining/factory_helpers.h
		pipelining/file_stream.h
		pipelining/graph.h
		pipelining/helpers.h
		pipelining/join.h
		pipelining/maintain_order_type.h
		pipelining/merge.h
		pipelining/merge_sorter.h
		pipelining/merger.h
		pipelining/node.h
		pipelining/node_map_dump.h
		pipelining/numeric.h
		pipelining/pair_factory.h
		pipelining/parallel.h
		pipelining/parallel/aligned_array.h
		pipelining/parallel/base.h
		pipelining/parallel/factory.h
		pipelining/parallel/options.h
		pipelining/parallel/pipes.h
		pipelining/parallel/worker_state.h
		pipelining/pipe_base.h
		pipelining/pipeline.h
		pipelining/reverse.h
		pipelining/serialization_sort.h
		pipelining/sort.h
		pipelining/std_glue.h
		pipelining/stdio.h
		pipelining/tokens.h
		pipelining/uniq.h
		pipelining/virtual.h
		portability.h
		internal_priority_queue.h
		priority_queue.inl
		priority_queue.h
		pq_overflow_heap.h
		pq_overflow_heap.inl
		pq_merge_heap.h
		pq_merge_heap.inl
		fractional_progress.h
		parallel_sort.h
		dummy_progress.h
		progress_indicator_subindicator.h
		progress_indicator_arrow.h
		progress_indicator_base.h
		progress_indicator_spin.h
		progress_indicator_null.h
		progress_indicator_terminal.h
		queue.h
		serialization.h
		serialization2.h
		serialization_stream.h
		serialization_sort.h
		sort.h
		sort_deprecated.h
		sort_manager.h
		stack.h
		stream.h
		stream_crtp.h
		stream_header.h
		stream_usage.h
		sysinfo.h
		tpie_assert.h
		tpie_log.h
		stats.h
		types.h
		tempname.h
		unused.h
		util.h
		array.h
		bit_array.h
		packed_array.h
		array_view_base.h
		array_view.h
		hash_map.h
		prime.h
		concepts.h
		concept_doc.h
		tpie.h
		unittest.h
		)

set (SOURCES
	backtrace.cpp
	cpu_timer.cpp
	file_base.cpp
	file_count.cpp
	file_stream_base.cpp
	execution_time_predictor.cpp
	fractional_progress.cpp
	job.cpp
	logstream.cpp
	memory.cpp
	pipelining/graph.cpp
	pipelining/pipeline.cpp
	pipelining/tokens.cpp
	portability.cpp
	prime.cpp
	progress_indicator_base.cpp
	progress_indicator_subindicator.cpp
	serialization_stream.cpp
	tempname.cpp
	tpie.cpp
	tpie_log.cpp
	stats.cpp
	util.cpp
	unittest.cpp
	"${CMAKE_CURRENT_BINARY_DIR}/sysinfo.cpp"
	)

if (WIN32)
set (HEADERS ${HEADERS} file_accessor/win32.h file_accessor/win32.inl)
else(WIN32)
set (HEADERS ${HEADERS} file_accessor/posix.h file_accessor/posix.inl)
endif(WIN32)

add_library(tpie ${HEADERS} ${SOURCES})
target_link_libraries(tpie ${Boost_LIBRARIES})
set_target_properties(tpie PROPERTIES FOLDER tpie)

install(TARGETS tpie
	LIBRARY DESTINATION lib
	ARCHIVE DESTINATION lib)
