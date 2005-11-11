//
// File: bte_stream_mmap.h (formerly bte_mmb.h)
// Author: Darren Erik Vengroff <dev@cs.duke.edu>
// Created: 5/13/94
//
// $Id: bte_stream_mmap.h,v 1.18 2005-11-11 13:24:16 jan Exp $
//
// Memory mapped streams.  This particular implementation explicitly manages
// blocks, and only ever maps in one block at a time.
//
// TODO: Get rid of or fix the LIBAIO stuff. As it is now it has no
// chance of working, since it uses the static
// BTE_STREAM_MMAP_BLOCK_FACTOR, which is no longer the true
// factor. The true block factor is determined dynamically, from the
// header.
//

#ifndef _BTE_STREAM_MMAP_H
#define _BTE_STREAM_MMAP_H

// Get definitions for working with Unix and Windows
#include <portability.h>

// For header's type field (77 == 'M').
#define BTE_STREAM_MMAP 77

#include <tpie_assert.h>
#include <tpie_log.h>

#ifdef VERBOSE
#  include <iostream>
#endif

#if USE_LIBAIO
#  if !HAVE_LIBAIO
#    error USE_LIBAIO requested, but aio library not in configuration.
#  endif
#  include <sys/asynch.h>
#endif

#ifdef BTE_STREAM_MMAP_READ_AHEAD
#  define BTE_STREAM_MMAP_MM_BUFFERS 2
#else
#  define BTE_STREAM_MMAP_MM_BUFFERS 1
#endif

// Get the BTE_stream_base class and other definitions.
#include <bte_stream_base.h>

#ifndef  BTE_STREAM_MMAP_BLOCK_FACTOR
#  define BTE_STREAM_MMAP_BLOCK_FACTOR 8
#endif

// Figure out the block offset for an offset (pos) in file.
// os_block_size_ is assumed to be the header size.
//#define BLOCK_OFFSET(pos) (((pos - os_block_size_) / header->block_size) * header->block_size + os_block_size_)

#define BLOCK_OFFSET(pos) (((pos - m_osBlockSize) / m_header->m_blockSize) * m_header->m_blockSize + m_osBlockSize)


//
// BTE_stream_mmap<T>
//
// This is a class template for the mmap() based implementation of a 
// BTE stream of objects of type T.  This version maps in only one
// block of the file at a time. 
//
template < class T > 
class BTE_stream_mmap: public BTE_stream_base < T > {

// These are for gcc-3.4 compatibility
protected:
    using BTE_stream_base<T>::remaining_streams;
    using BTE_stream_base<T>::m_substreamLevel;
    using BTE_stream_base<T>::m_status;
    using BTE_stream_base<T>::m_persistenceStatus;
    using BTE_stream_base<T>::m_readOnly;
    using BTE_stream_base<T>::m_path;
    using BTE_stream_base<T>::m_osBlockSize;
    using BTE_stream_base<T>::m_fileOffset;
    using BTE_stream_base<T>::m_logicalBeginOfStream;
    using BTE_stream_base<T>::m_logicalEndOfStream;
    using BTE_stream_base<T>::m_fileLength;
    using BTE_stream_base<T>::m_osErrno;
    using BTE_stream_base<T>::m_header;

    using BTE_stream_base<T>::check_header;
    using BTE_stream_base<T>::init_header;
    using BTE_stream_base<T>::register_memory_allocation;
    using BTE_stream_base<T>::register_memory_deallocation;
    using BTE_stream_base<T>::record_statistics;
    using BTE_stream_base<T>::name;

    
public:
    using BTE_stream_base<T>::os_block_size;
// End: These are for gcc-3.4 compatibility

public:
    // Constructor.
    // [tavi 01/09/02] Careful with the lbf (logical block factor)
    // parameter. I introduced it in order to avoid errors when reading
    // a stream having a different block factor from the default, but
    // this may cause errors in applications. For example,
    // AMI_partition_and merge computes memory requirements of temporary
    // streams based on the memory usage of the INPUT stream. However,
    // the input stream may have different block size from the temporary
    // streams created later. Until these issues are addressed, the
    // usage of lbf is discouraged.
    BTE_stream_mmap(const char *dev_path, 
		    BTE_stream_type st, 
		    TPIE_OS_SIZE_T lbf = BTE_STREAM_MMAP_BLOCK_FACTOR);

    // A substream constructor.
    BTE_stream_mmap(BTE_stream_mmap * super_stream,
		    BTE_stream_type st, 
		    TPIE_OS_OFFSET sub_begin, 
		    TPIE_OS_OFFSET sub_end);

    // A psuedo-constructor for substreams.
    BTE_err new_substream(BTE_stream_type st, 
			  TPIE_OS_OFFSET sub_begin,
			   TPIE_OS_OFFSET sub_end,
			   BTE_stream_base < T > **sub_stream);

    // Destructor
    ~BTE_stream_mmap ();

    inline BTE_err read_item(T ** elt);
    inline BTE_err write_item(const T & elt);

    // Move to a specific position in the stream.
    BTE_err seek(TPIE_OS_OFFSET offset);

    // Truncate the stream.
    BTE_err truncate(TPIE_OS_OFFSET offset);

    // Return the number of items in the stream.
    inline TPIE_OS_OFFSET stream_len() const;

    // Return the current position in the stream.
    inline TPIE_OS_OFFSET tell() const;

    // Query memory usage
    BTE_err main_memory_usage(size_t * usage, MM_stream_usage usage_type);

    TPIE_OS_OFFSET chunk_size() const;


    inline BTE_err grow_file (TPIE_OS_OFFSET block_offset);

#ifdef VERBOSE
    void print (char *pref = "");
#endif

private:

#ifdef BTE_STREAM_MMAP_READ_AHEAD
    // Read ahead into the next logical block.
    void read_ahead ();
#endif

    void initialize ();

    BTE_stream_header *map_header ();
    void unmap_header ();

    inline BTE_err validate_current ();
    inline BTE_err invalidate_current ();

    BTE_err map_current ();
    BTE_err unmap_current ();

    inline BTE_err advance_current ();

    inline TPIE_OS_OFFSET item_off_to_file_off (TPIE_OS_OFFSET item_off) const;
    inline TPIE_OS_OFFSET file_off_to_item_off (TPIE_OS_OFFSET item_off) const;

#ifdef COLLECT_STATS
    long stats_hits;
    long stats_misses;
    long stats_compulsory;
    long stats_eos;
#endif

    unsigned int m_mmapStatus;

    // descriptor of the mapped file.
    TPIE_OS_FILE_DESCRIPTOR m_fileDescriptor;	

    // Pointer to the current item (mapped in).
    T *m_currentItem;

    // Pointer to beginning of the currently mapped block.
    T *m_currentBlock;

    // True if current points to a valid, mapped block.
    bool m_blockValid;

    // True if the m_currentBlock is mapped.
    bool m_blockMapped;

    // for use in double buffering
    T *m_nextBlock;		        // ptr to next block
    TPIE_OS_OFFSET m_nextBlockOffset;	// position of next block
    bool m_haveNextBlock;		// is next block mapped
    bool m_writeOnly;			// stream is write-only


#ifdef BTE_STREAM_MMAP_READ_AHEAD
    T* m_nextBlock;
#endif

#if USE_LIBAIO
    // A buffer to read the first word of each OS block in the next logical
    // block for read ahead.
    int read_ahead_buffer[BTE_STREAM_MMAP_BLOCK_FACTOR];

    // Results of asyncronous I/O.
    aio_result_t aio_results[BTE_STREAM_MMAP_BLOCK_FACTOR];
#endif

};

/* ********************************************************************** */

/* definitions start here */

static int call_munmap (void *addr, size_t len) {
    return TPIE_OS_MUNMAP (addr, len);
}

static void *call_mmap (void *addr, size_t len, 
			bool r_only, bool w_only,
			TPIE_OS_FILE_DESCRIPTOR fd, 
			TPIE_OS_OFFSET off, int fixed) {
    void *ptr;
    int flags = 0;
    
    assert (!fixed || addr);

#ifdef MACH_ALPHA
    // for enhanced mmap calls
#define MAP_OVERWRITE 0x1000	// block will be overwritten
    flags = (MAP_FILE |
	     (fixed ? TPIE_OS_FLAG_MAP_FIXED :MAP_VARIABLE) |
	     (w_only ? MAP_OVERWRITE : 0));
#else
    flags = (fixed ? TPIE_OS_FLAG_MAP_FIXED : 0);
#endif
    flags |= TPIE_OS_FLAG_MAP_SHARED;
    
    ptr = TPIE_OS_MMAP (addr, len,
			(r_only ? TPIE_OS_FLAG_PROT_READ : TPIE_OS_FLAG_PROT_READ | TPIE_OS_FLAG_PROT_WRITE),
			flags, fd, off);
    assert (ptr);
    return ptr;
}


template < class T > void BTE_stream_mmap < T >::initialize () {
#ifdef COLLECT_STATS
    stats_misses = stats_hits = stats_compulsory = stats_eos = 0;
#endif
    m_haveNextBlock = false;
    m_blockValid    = false;
    m_blockMapped   = false;
    m_fileOffset    = m_logicalBeginOfStream = m_osBlockSize;
    m_nextBlock     = m_currentBlock = m_currentItem = NULL;
}

//
// This constructor creates a stream whose contents are taken from the
// file whose path is given.
//
template < class T >
BTE_stream_mmap < T >::BTE_stream_mmap (const char *dev_path, BTE_stream_type st, size_t lbf) {
    m_status = BTE_STREAM_STATUS_NO_STATUS;

    if (remaining_streams <= 0) {

	m_status = BTE_STREAM_STATUS_INVALID;

	TP_LOG_FATAL_ID ("BTE internal error: cannot open more streams.");

	return;
    }

    // Cache the path name
    if (strlen (dev_path) > BTE_STREAM_PATH_NAME_LEN - 1) {

	m_status = BTE_STREAM_STATUS_INVALID;

	TP_LOG_FATAL_ID ("Path name \"" << dev_path << "\" too long.");

	return;
    }

    strncpy (m_path, dev_path, BTE_STREAM_PATH_NAME_LEN);

    m_readOnly  = (st == BTE_READ_STREAM);
    m_writeOnly = (st == BTE_WRITEONLY_STREAM);

    m_osBlockSize = os_block_size();

    // This is a top level stream
    m_substreamLevel = 0;
    // Reduce the number of streams available.
    remaining_streams--;

    switch (st) {
    case BTE_READ_STREAM:
	// Open the file for reading.
	if (!TPIE_OS_IS_VALID_FILE_DESCRIPTOR(m_fileDescriptor = TPIE_OS_OPEN_ORDONLY(m_path, TPIE_OS_FLAG_USE_MAPPING_TRUE))) { 

	    m_status  = BTE_STREAM_STATUS_INVALID;
	    m_osErrno = errno;

	    TP_LOG_FATAL ("open() failed to open \"");
	    TP_LOG_FATAL (m_path);
	    TP_LOG_FATAL ("\": ");
	    TP_LOG_FATAL (strerror (m_osErrno));
	    TP_LOG_FATAL ("\n");
	    TP_LOG_FLUSH_LOG;
	    // [tavi 01/07/02] Commented this out. No need to panic.
	    //assert (0);
	    return;
	}

	// Get ready to read the first item out of the file.
	initialize ();

	m_header = map_header ();
	if (check_header() < 0) {

	    m_status = BTE_STREAM_STATUS_INVALID;

	    // [tavi 01/07/02] Commented this out. No need to panic.
	    //assert (0);
	    return;
	}

	if (m_header->m_type != BTE_STREAM_MMAP) {
	    TP_LOG_WARNING_ID("Using MMAP stream implem. on another type of stream.");
	    TP_LOG_WARNING_ID("Stream implementations may not be compatible.");
	}

	if ((m_header->m_blockSize % m_osBlockSize != 0) || 
	    (m_header->m_blockSize == 0)) {

	    m_status = BTE_STREAM_STATUS_INVALID;

	    TP_LOG_FATAL_ID ("m_header: incorrect logical block size;");
	    TP_LOG_FATAL_ID ("expected multiple of OS block size.");

	    return;
	}

	if (m_header->m_blockSize != BTE_STREAM_MMAP_BLOCK_FACTOR * m_osBlockSize) {
	    TP_LOG_WARNING_ID("Stream has different block factor than the default.");
	    TP_LOG_WARNING_ID("This may cause problems in some existing applications.");
	}

	break;

    case BTE_WRITE_STREAM:
    case BTE_WRITEONLY_STREAM:
    case BTE_APPEND_STREAM:

	// Open the file for writing.  First we will try to open 
	// is with the O_EXCL flag set.  This will fail if the file
	// already exists.  If this is the case, we will call open()
	// again without it and read in the header block.
	if (!TPIE_OS_IS_VALID_FILE_DESCRIPTOR(m_fileDescriptor = TPIE_OS_OPEN_OEXCL(m_path, TPIE_OS_FLAG_USE_MAPPING_TRUE))) {

	    // Try again, hoping the file already exists.
	    if (!TPIE_OS_IS_VALID_FILE_DESCRIPTOR(m_fileDescriptor = TPIE_OS_OPEN_ORDWR(m_path, TPIE_OS_FLAG_USE_MAPPING_TRUE))) {

		m_status = BTE_STREAM_STATUS_INVALID;
		m_osErrno = errno;

		TP_LOG_FATAL ("open() failed to open \"");
		TP_LOG_FATAL (m_path);
		TP_LOG_FATAL ("\": ");
		TP_LOG_FATAL (strerror (m_osErrno));
		TP_LOG_FATAL ("\n");
		TP_LOG_FLUSH_LOG;
		return;
	    }

	    initialize ();
	    // The file already exists, so read the header.

	    m_header = map_header ();
	    if (check_header () < 0) {

		m_status = BTE_STREAM_STATUS_INVALID;

		// [tavi 01/07/02] Commented this out. No need to panic.
		//assert (0);
		return;
	    }

	    if (m_header->m_type != BTE_STREAM_MMAP) {
		TP_LOG_WARNING_ID("Using MMAP stream implem. on another type of stream.");
		TP_LOG_WARNING_ID("Stream implementations may not be compatible.");
	    }

	    if ((m_header->m_blockSize % m_osBlockSize != 0) || 
		(m_header->m_blockSize == 0)) {

		m_status = BTE_STREAM_STATUS_INVALID;

		TP_LOG_FATAL_ID ("m_header: incorrect logical block size;");
		TP_LOG_FATAL_ID ("expected multiple of OS block size.");
		return;
	    }

	    if (m_header->m_blockSize != BTE_STREAM_MMAP_BLOCK_FACTOR * m_osBlockSize) {
		TP_LOG_WARNING_ID("Stream has different block factor than the default.");
		TP_LOG_WARNING_ID("This may cause problems in some existing applications.");
	    }
	}
	else {	 // The file was just created.
	    
	    m_logicalEndOfStream = m_osBlockSize;
	    // Rajiv
	    // [tavi 01/07/02] Commented this out. Aren't we sure the file is OK?
	    //assert (lseek (m_fileDescriptor, 0, SEEK_END) == 0);

#ifdef VERBOSE
	    if (verbose)
		cout << "CONS created file: " << m_path << "\n";
#endif

	    // what does this do??? Rajiv
	    // Create and map in the header.
	    if (TPIE_OS_LSEEK(m_fileDescriptor, m_osBlockSize - 1, TPIE_OS_FLAG_SEEK_SET) != m_osBlockSize - 1) {

		m_status = BTE_STREAM_STATUS_INVALID;
		m_osErrno = errno;

		TP_LOG_FATAL ("lseek() failed to move past header of \"");
		TP_LOG_FATAL (m_path);
		TP_LOG_FATAL ("\": ");
		TP_LOG_FATAL (strerror (m_osErrno));
		TP_LOG_FATAL ("\n");
		TP_LOG_FLUSH_LOG;
		// [tavi 01/07/02] Commented this out. No need to panic.
		//assert (0 == 1);
		return;
	    }

	    initialize ();

	    m_header = map_header ();
	    if (m_header == NULL) {
		m_status = BTE_STREAM_STATUS_INVALID;
		return;
	    }

	    init_header();

	    if (lbf == 0) {
		lbf = 1;
		TP_LOG_WARNING_ID("Block factor 0 requested. Using 1 instead.");
	    }

	    // Set the logical block size.
	    m_header->m_blockSize = lbf * m_osBlockSize;

	    // Set the type.
	    m_header->m_type = BTE_STREAM_MMAP;

	    record_statistics(STREAM_CREATE);

	}
	break;
    }

    // We can't handle streams of large objects.
    if (sizeof (T) > m_header->m_blockSize) {

	m_status = BTE_STREAM_STATUS_INVALID;

	TP_LOG_FATAL_ID ("Object is too big (object size/block size):");
	TP_LOG_FATAL_ID (sizeof(T));
	TP_LOG_FATAL_ID (static_cast<TPIE_OS_OUTPUT_SIZE_T>(m_header->m_blockSize));
	return;
    }

    m_fileLength = TPIE_OS_LSEEK(m_fileDescriptor, 0, TPIE_OS_FLAG_SEEK_END);
    assert (m_fileLength >= 0);

    m_logicalEndOfStream = item_off_to_file_off (m_header->m_itemLogicalEOF);

    if (st == BTE_APPEND_STREAM) {
	m_fileOffset = m_logicalEndOfStream;
    } 
    else {
	m_fileOffset = m_osBlockSize;
    }
#ifdef VERBOSE
    // Rajiv
    if (verbose)
	cout << "CONS logical eof=" << m_header->m_itemLogicalEOF << "\n";
#endif

    // By default, all streams are deleted at destruction time.
    // [tavi 01/07/02] No. Streams initialized with given names are persistent.
    m_persistenceStatus = PERSIST_PERSISTENT;

    // Register memory usage before returning.
    // [The header is allocated using "new", so no need to register.]
    // Since blocks and header are allocated by mmap and not "new",
    // register memory manually. No mem overhead with mmap
//    register_memory_allocation (sizeof (BTE_stream_header));
    register_memory_allocation (BTE_STREAM_MMAP_MM_BUFFERS * m_header->m_blockSize);

    record_statistics(STREAM_OPEN);

#ifdef VERBOSE
    // Rajiv
    if (verbose) {
	switch (st) {
	case BTE_READ_STREAM:
	    cout << "CONS read stream\n";
	    break;
	case BTE_WRITE_STREAM:
	    cout << "CONS read/write stream\n";
	    break;
	default:
	    cout << "CONS someother stream\n";
	    break;
	}
	print ("CONS ");
    }
#endif
}


// A substream constructor.
// sub_begin is the item offset of the first item in the stream.
// sub_end is the item offset that of the last item in the stream.
// Thus, m_logicalEndOfStream in the new substream will be set to point one item beyond
// this.
//
// For example, if a stream contains [A,B,C,D,...] then substream(1,3)
// will contain [B,C,D].
template < class T >
BTE_stream_mmap < T >::BTE_stream_mmap (BTE_stream_mmap * super_stream, BTE_stream_type st, TPIE_OS_OFFSET sub_begin, TPIE_OS_OFFSET sub_end) {

    m_status = BTE_STREAM_STATUS_NO_STATUS;

    if (remaining_streams <= 0) {
	TP_LOG_FATAL_ID ("BTE error: cannot open more streams.");
	m_status = BTE_STREAM_STATUS_INVALID;
	return;
    }

    if (super_stream->m_status == BTE_STREAM_STATUS_INVALID) {
	m_status = BTE_STREAM_STATUS_INVALID;
	TP_LOG_FATAL_ID ("BTE error: super stream is invalid.");
	return;
    }

    if (super_stream->m_readOnly && (st != BTE_READ_STREAM)) {
	m_status = BTE_STREAM_STATUS_INVALID;
	TP_LOG_FATAL_ID
	    ("BTE error: super stream is read only and substream is not.");
	return;
    }
    // Rajiv
    initialize ();

    // Reduce the number of streams avaialble.
    remaining_streams--;

    // Copy the relevant fields from the super_stream.
    m_fileDescriptor = super_stream->m_fileDescriptor;
    m_osBlockSize    = super_stream->m_osBlockSize;
    m_header         = super_stream->m_header;
    m_fileLength     = super_stream->m_fileLength;
    m_substreamLevel = super_stream->m_substreamLevel + 1;

    m_persistenceStatus = PERSIST_PERSISTENT;

    // The arguments sub_start and sub_end are logical item positions
    // within the stream.  We need to convert them to offsets within
    // the stream where items are found.

    TPIE_OS_OFFSET super_item_begin = file_off_to_item_off (super_stream->m_logicalBeginOfStream);

    m_logicalBeginOfStream = item_off_to_file_off (super_item_begin + sub_begin);
    m_logicalEndOfStream   = item_off_to_file_off (super_item_begin + sub_end + 1);

    if (m_logicalEndOfStream > super_stream->m_logicalEndOfStream) {
	m_status = BTE_STREAM_STATUS_INVALID;
	return;
    }

    m_fileOffset = m_logicalBeginOfStream;

    m_currentBlock = NULL;
    m_blockValid = false;

    m_readOnly  = super_stream->m_readOnly;
    m_writeOnly = super_stream->m_writeOnly;

    strncpy (m_path, super_stream->m_path, BTE_STREAM_PATH_NAME_LEN);

    record_statistics(STREAM_OPEN);
    record_statistics(SUBSTREAM_CREATE);

    // substreams are considered to have no memory overhead!
}

// A psuedo-constructor for substreams.  This serves as a wrapper for
// the constructor above in order to get around the fact that one
// cannot have virtual constructors.
template < class T >
BTE_err BTE_stream_mmap < T >::new_substream (BTE_stream_type st, TPIE_OS_OFFSET sub_begin, TPIE_OS_OFFSET sub_end, BTE_stream_base < T > **sub_stream) {
    // Check permissions.

    if ((st != BTE_READ_STREAM) && ((st != BTE_WRITE_STREAM) || m_readOnly)) {
	*sub_stream = NULL;
	return BTE_ERROR_PERMISSION_DENIED;
    }

    tp_assert (((st == BTE_WRITE_STREAM) && !m_readOnly) ||
	       (st == BTE_READ_STREAM),
	       "Bad things got through the permisssion checks.");

    BTE_stream_mmap < T > *sub =
	new BTE_stream_mmap < T > (this, st, sub_begin, sub_end);

    *sub_stream = (BTE_stream_base < T > *)sub;

    return BTE_ERROR_NO_ERROR;
}

template < class T > BTE_stream_mmap < T >::~BTE_stream_mmap () {

    // If the stream is already invalid for some reason, then don't
    // worry about anything.
    if (m_status == BTE_STREAM_STATUS_INVALID) {
	TP_LOG_WARNING_ID ("BTE internal error: invalid stream in destructor.");
	return;
    }

    // Increase the number of streams avaialble.
    if (remaining_streams >= 0) {
	remaining_streams++;
    }

    // If this is writable and not a substream, then put the logical
    // eos back into the header before unmapping it.
    if (!m_readOnly && !m_substreamLevel) {
	m_header->m_itemLogicalEOF = file_off_to_item_off (m_logicalEndOfStream);
    }

#ifdef VERBOSE
    // Rajiv
    if (verbose) {
	cout << "DELE logical eof=" << m_header->m_itemLogicalEOF << "\n";
    }
#endif
    assert (m_substreamLevel ||
	    m_header->m_itemLogicalEOF == file_off_to_item_off (m_logicalEndOfStream));

    // Unmap the current block if necessary.
    if (m_blockMapped) {
	unmap_current();
    }

    // If this is not a substream then close the file.
    if (!m_substreamLevel) {
	// [Rajiv] make sure the length of the file is correct
	// [tavi 06/23/02] and file is not read-only! 
	if ((m_fileLength > m_logicalEndOfStream) && (!m_readOnly) &&
	    (TPIE_OS_FTRUNCATE (m_fileDescriptor, BLOCK_OFFSET(m_logicalEndOfStream) + m_header->m_blockSize) < 0)) {

	    m_osErrno = errno;
	    
	    TP_LOG_FATAL_ID("Failed to ftruncate() to the new end of " << m_path);
	    TP_LOG_FATAL_ID("m_fileLength:" << m_fileLength << ", m_logicalEndOfStream:" << m_logicalEndOfStream);
	    TP_LOG_FATAL_ID("argument to ftruncate:" << BLOCK_OFFSET(m_logicalEndOfStream) + m_header->m_blockSize);
	    TP_LOG_FATAL_ID(strerror (m_osErrno));

	}

	// Unmap the header.
	// [tavi 06/23/02] Added test for m_readOnly. 
	if (!m_readOnly) {
	    unmap_header();
	}

	// Close the file.
	if (TPIE_OS_CLOSE (m_fileDescriptor)) {
	    m_osErrno = errno;
	    TP_LOG_WARNING_ID("Failed to close() " << m_path);
	    TP_LOG_WARNING_ID(strerror (m_osErrno));
	}

	// If it should not persist, unlink the file.
	if (m_persistenceStatus == PERSIST_DELETE) {
	    if (m_readOnly) {
		TP_LOG_WARNING_ID("PERSIST_DELETE for read-only stream in " << m_path);
	    }
	    else {
		if (TPIE_OS_UNLINK (m_path)) {
		    m_osErrno = errno;
		    TP_LOG_WARNING_ID ("unlink() failed during destruction of " << m_path);
		    TP_LOG_WARNING_ID (strerror (m_osErrno));
		} 
		else {
		    record_statistics(STREAM_DELETE);
		}
	    }
	}

	// Register memory deallocation before returning.
//	register_memory_deallocation (sizeof (BTE_stream_header)); // for the header. [This is done via delete.]
	register_memory_deallocation (BTE_STREAM_MMAP_MM_BUFFERS *
				      m_header->m_blockSize);

//	// Free the in-memory header (previously allocated with malloc).
//	free (m_header);
	delete m_header;

    } 
    else {
	record_statistics(SUBSTREAM_DELETE);
    }

    record_statistics(STREAM_CLOSE);

#ifdef VERBOSE
    if (verbose) {
	if (m_persistenceStatus == PERSIST_DELETE)
	    cout << "DELE unlinked file\n";
	if (m_substreamLevel) {
	    cout << "DELE substream destructor\n";
	}
	print ("DELE ");
    }
#endif
}


// m_logicalEndOfStream points just past the last item ever written, so if f_current
// is there we are at the end of the stream and cannot read.

template < class T >
inline BTE_err BTE_stream_mmap < T >::read_item (T ** elt) {

    BTE_err bte_err;

    if (m_writeOnly) {
#ifdef VERBOSE
	if (verbose)
	    cerr << "ERROR read on a write-only stream\n";
#endif
	return BTE_ERROR_WRITE_ONLY;
    }
    // Make sure we are not currently at the EOS.
    if (m_fileOffset + sizeof (T) > m_logicalEndOfStream) {
	//tp_assert(0, "Can't read past eos.");
	//TP_LOG_WARNING("Reading past eos.\n");
	return BTE_ERROR_END_OF_STREAM;
    }

    // Validate the current block.
    if ((bte_err = validate_current ()) != BTE_ERROR_NO_ERROR) {
	return bte_err;
    }

    // Check and make sure that the current pointer points into the current
    // block.
    tp_assert (((char *) m_currentItem - (char *) m_currentBlock <=
		m_header->m_blockSize - sizeof (T)),
	       "m_currentItem is past the end of the current block");
    tp_assert (((char *) m_currentItem - (char *) m_currentBlock >= 0),
	       "m_currentItem is before the begining of the current block");

    record_statistics(ITEM_READ);

    *elt = m_currentItem;	// Read
    advance_current ();		// move ptr to next elt

    // If we are in a substream, there should be no way for f_current to
    // pass m_logicalEndOfStream.
    tp_assert (!m_substreamLevel || (m_fileOffset <= m_logicalEndOfStream),
	       "Got past eos in a substream.");

    return BTE_ERROR_NO_ERROR;
}

// m_logicalEndOfStream points just past the last item ever written, so if f_current
// is there we are at the end of the stream and can only write if this
// is not a substream.
template < class T >
inline BTE_err BTE_stream_mmap < T >::write_item (const T & elt) {

    BTE_err bte_err;
    
    // This better be a writable stream.
    if (m_readOnly) {
	TP_LOG_WARNING_ID ("write on a read-only stream\n");
	return BTE_ERROR_READ_ONLY;    
    }

    // Make sure we are not currently at the EOS of a substream.
    if (m_substreamLevel && (m_logicalEndOfStream <= m_fileOffset)) {
	tp_assert (m_logicalEndOfStream == m_fileOffset, "Went too far in a substream.");
	return BTE_ERROR_END_OF_STREAM;
    }

    // Validate the current block.
    bte_err = validate_current ();
    if (bte_err != BTE_ERROR_NO_ERROR) {
	return bte_err;
    }

    // Check and make sure that the current pointer points into the current
    // block.
    tp_assert (((char *) m_currentItem - (char *) m_currentBlock <=
		m_header->m_blockSize - sizeof (T)),
	       "m_currentItem is past the end of the current block");
    tp_assert (((char *) m_currentItem - (char *) m_currentBlock >= 0),
	       "m_currentItem is before the begining of the current block");
    assert (m_currentItem);
    
    record_statistics(ITEM_WRITE);
    
    *m_currentItem = elt;		// write
    advance_current ();		// Advance the current pointer.
    
    // If we are in a substream, there should be no way for f_current
    // to pass m_logicalEndOfStream.
    tp_assert (!m_substreamLevel || (m_fileOffset <= m_logicalEndOfStream),
	       "Got past eos in a substream.");
    
    // If we moved past eos, then update eos unless we are in a
    // substream, in which case EOS will be returned on the next call.
    if ((m_fileOffset > m_logicalEndOfStream) && !m_substreamLevel) {
	// I dont like this assert Rajiv
	// tp_assert(m_fileOffset == m_logicalEndOfStream + sizeof(T), "Advanced too far somehow.");
	tp_assert (m_fileOffset <= m_fileLength, "Advanced too far somehow.");
	m_logicalEndOfStream = m_fileOffset;
	    // this is the only place m_logicalEndOfStream is changed excluding 
	    // constructors and truncate Rajiv
    }
    
    return BTE_ERROR_NO_ERROR;
}

// Query memory usage

// Note that in a substream we do not charge for the memory used by
// the header, since it is accounted for in the 0 level superstream.
template < class T >
BTE_err BTE_stream_mmap < T >::main_memory_usage (size_t * usage, MM_stream_usage usage_type) {
    switch (usage_type) {
    case MM_STREAM_USAGE_OVERHEAD:
	//Fixed costs. Only 2*mem overhead, because only class and base
	//are allocated dynamicall via "new". Header is read via mmap
	*usage = sizeof(*this) + sizeof(BTE_stream_header) +
	    2*MM_manager.space_overhead();
	break;

    case MM_STREAM_USAGE_BUFFER:
	//no mem manager overhead when allocated via mmap
	*usage = BTE_STREAM_MMAP_MM_BUFFERS * m_header->m_blockSize;
	break;

    case MM_STREAM_USAGE_CURRENT:
	*usage = (sizeof(*this) + sizeof(BTE_stream_header) +
		  2*MM_manager.space_overhead() + 
		  ((m_currentBlock == NULL) ? 0 :
		   BTE_STREAM_MMAP_MM_BUFFERS * m_header->m_blockSize));
	break;

    case MM_STREAM_USAGE_MAXIMUM:
    case MM_STREAM_USAGE_SUBSTREAM:
	*usage = (sizeof(*this) + sizeof(BTE_stream_header) +
		  2*MM_manager.space_overhead() + 
		  BTE_STREAM_MMAP_MM_BUFFERS * m_header->m_blockSize);
	break;
    }

    return BTE_ERROR_NO_ERROR;
};

// Return the number of items in the stream.
template < class T > 
TPIE_OS_OFFSET BTE_stream_mmap < T >::stream_len () const {
    return file_off_to_item_off (m_logicalEndOfStream) - 
	file_off_to_item_off (m_logicalBeginOfStream);
};

// Move to a specific position.
template < class T > 
BTE_err BTE_stream_mmap < T >::seek (TPIE_OS_OFFSET offset) {
    
    BTE_err be;
    TPIE_OS_OFFSET new_offset;
    
    // Looks like we can only seek within the file Rajiv
    if ((offset < 0) ||
	(offset >
	 file_off_to_item_off (m_logicalEndOfStream) - 
	 file_off_to_item_off (m_logicalBeginOfStream))) {
	return BTE_ERROR_OFFSET_OUT_OF_RANGE;
    }

    // Compute the new offset
    new_offset =
	item_off_to_file_off (file_off_to_item_off (m_logicalBeginOfStream) + 
			      offset);

    if (m_readOnly) {
	tp_assert (new_offset <= m_logicalEndOfStream, "Advanced too far somehow.");
    }

// // If it is not in the same block as the current position then
// // invalidate the current block.
//if (((new_offset - m_osBlockSize) / m_header->m_blockSize) !=
//    ((m_fileOffset - m_osBlockSize) / m_header->m_blockSize)) {

// The above was the old code which was wrong: we also need to check that
// we have the correct block mapped in (m_fileOffset does not always point into
// the current block!)
    
    if (((char *) m_currentItem - (char *) m_currentBlock >=
	 m_header->m_blockSize) ||
	(((new_offset - m_osBlockSize) / m_header->m_blockSize) !=
	 ((m_fileOffset - m_osBlockSize) / m_header->m_blockSize))) {
	if (m_blockValid && ((be = unmap_current()) != BTE_ERROR_NO_ERROR)) {
	    return be;
	}
    } 
    else {
	if (m_blockValid) {
	    
	    // We have to adjust current.
	    register TPIE_OS_OFFSET internal_block_offset;
	    
	    internal_block_offset = file_off_to_item_off (new_offset) %
		(m_header->m_blockSize / sizeof (T));
	    
	    m_currentItem = m_currentBlock + internal_block_offset;
	}
    }
    
    m_fileOffset = new_offset;
    
    record_statistics(ITEM_SEEK);

    return BTE_ERROR_NO_ERROR;
}

template < class T > 
TPIE_OS_OFFSET BTE_stream_mmap < T >::tell() const {
    return file_off_to_item_off(m_fileOffset);
}

// Truncate the stream.
template < class T > 
BTE_err BTE_stream_mmap < T >::truncate (TPIE_OS_OFFSET offset) {

    BTE_err be;
    TPIE_OS_OFFSET new_offset;
    TPIE_OS_OFFSET block_offset;

    // Sorry, we can't truncate a substream.
    if (m_substreamLevel) {
	return BTE_ERROR_STREAM_IS_SUBSTREAM;
    }

    if (offset < 0) {
	return BTE_ERROR_OFFSET_OUT_OF_RANGE;
    }
    // Compute the new offset
    new_offset =
	item_off_to_file_off (file_off_to_item_off (m_logicalBeginOfStream) + 
			      offset);

    // If it is not in the same block as the current position then
    // invalidate the current block.
    
    // We also need to check that we have the correct block mapped in
    // (m_fileOffset does not always point into the current block!) -
    // see comment in seek()

    if (((char *) m_currentItem - (char *) m_currentBlock >= 
	 m_header->m_blockSize) ||
	(((new_offset - m_osBlockSize) / m_header->m_blockSize) !=
	 ((m_fileOffset - m_osBlockSize) / m_header->m_blockSize))) {

	if (m_blockValid && ((be = unmap_current()) != BTE_ERROR_NO_ERROR)) {
	    return be;
	}
    }

    // If it is not in the same block as the current end of stream
    // then truncate the file to the end of the new last block.
    if (((new_offset - m_osBlockSize) / m_header->m_blockSize) !=
	((m_logicalEndOfStream - m_osBlockSize) / m_header->m_blockSize)) {

	if(m_blockMapped) {	
	    unmap_current();
	}

	// Determine the offset of the block that new_offset is in.
	block_offset = BLOCK_OFFSET (new_offset);
	// Rajiv 
	// ((new_offset - m_osBlockSize) / m_header->m_blockSize)
	// * m_header->m_blockSize + m_osBlockSize;
	m_fileLength = block_offset + m_header->m_blockSize;

	if (TPIE_OS_FTRUNCATE (m_fileDescriptor, m_fileLength)) {

	    m_osErrno = errno;

	    TP_LOG_FATAL ("Failed to ftruncate() to the new end of \"");
	    TP_LOG_FATAL (m_path);
	    TP_LOG_FATAL ("\": ");
	    TP_LOG_FATAL (strerror (m_osErrno));
	    TP_LOG_FATAL ('\n');
	    TP_LOG_FLUSH_LOG;

	    return BTE_ERROR_OS_ERROR;
	}
    }

    if (m_blockValid) {
	// This can happen if we didn't truncate much 
	// and stayed within the current block
	// then the current block is still valid, but the current item
	// pointer may not be valid. We have to adjust current.
	TPIE_OS_OFFSET internal_block_offset;
	internal_block_offset = file_off_to_item_off (new_offset) %
	    (m_header->m_blockSize / sizeof (T));
	m_currentItem = m_currentBlock + internal_block_offset;
    }

    // Reset the current position to the end.    
    m_fileOffset = m_logicalEndOfStream = new_offset;

    return BTE_ERROR_NO_ERROR;
}

// Map in the header from the file.  This assumes that the path
// has been cached in path and that the file has been opened and
// m_fileDescriptor contains a valid descriptor.
template < class T >
BTE_stream_header * BTE_stream_mmap < T >::map_header () {

    TPIE_OS_OFFSET file_end;
    BTE_stream_header *mmap_hdr;
    BTE_stream_header *ptr_to_header;

    // If the underlying file is not at least long enough to contain
    // the header block, then, assuming the stream is writable, we have
    // to create the space on disk by doing an explicit write().
    if ((file_end = TPIE_OS_LSEEK(m_fileDescriptor, 0, TPIE_OS_FLAG_SEEK_END)) < m_osBlockSize) {
        if (m_readOnly) {

	    m_status = BTE_STREAM_STATUS_INVALID;

	    TP_LOG_FATAL ("No header block in read only stream \"");
	    TP_LOG_FATAL (m_path);
	    TP_LOG_FATAL ('\n');
	    TP_LOG_FLUSH_LOG;
	    return NULL;

	} 
	else {
	    // A writable stream, so we can ftruncate() space for a
	    // header block.
	    if (TPIE_OS_FTRUNCATE (m_fileDescriptor, m_osBlockSize)) {

		m_osErrno = errno;

		TP_LOG_FATAL ("Failed to ftruncate() to end of header of \"");
		TP_LOG_FATAL (m_path);
		TP_LOG_FATAL ("\": ");
		TP_LOG_FATAL (strerror (m_osErrno));
		TP_LOG_FATAL ('\n');
		TP_LOG_FLUSH_LOG;
		return NULL;
	    }
	}
    }

    // Map in the header block.  If the stream is writable, the header 
    // block should be too.
    // took out the SYSTYPE_BSD ifdef for convenience
    // changed from MAP_FIXED to MAP_VARIABLE because we are using NULL
    mmap_hdr = (BTE_stream_header *)
	(call_mmap ((NULL), sizeof (BTE_stream_header),
		    m_readOnly, m_writeOnly, 
		    m_fileDescriptor, 0, 0));

    if (mmap_hdr == (BTE_stream_header *) (-1)) {

	m_status = BTE_STREAM_STATUS_INVALID;
	m_osErrno = errno;

	TP_LOG_FATAL ("mmap() failed to map in header from \"");
	TP_LOG_FATAL (m_path);
	TP_LOG_FATAL ("\": ");
	TP_LOG_FATAL (strerror (m_osErrno));
	TP_LOG_FATAL ("\n");
	TP_LOG_FLUSH_LOG;

	return NULL;
    }
    
    // Commented this out to unify the way headers are allocated
    // (see also bte_stream_ufs.h) [jv]

//     m_header = (BTE_stream_header *) malloc (sizeof (BTE_stream_header));
//     if (!m_header) {
// 	TP_LOG_FATAL ("out of virtual memory");
// 	return NULL;
//     }

//     memcpy (m_header, mmap_hdr, sizeof (BTE_stream_header));

    ptr_to_header = new BTE_stream_header();
    memcpy (ptr_to_header, mmap_hdr, sizeof (BTE_stream_header));
    call_munmap (mmap_hdr, sizeof (BTE_stream_header));

    return ptr_to_header;
}

// Map in the header from the file.  This assumes that the path
// has been cached in path and that the file has been opened and
// m_fileDescriptor contains a valid descriptor.
template < class T > void BTE_stream_mmap < T >::unmap_header ()
{
    BTE_stream_header *mmap_hdr;
    TPIE_OS_OFFSET file_end;

    // If the underlying file is not at least long enough to contain
    // the header block, then, assuming the stream is writable, we have
    // to create the space on disk by doing an explicit write().
    if ((file_end = TPIE_OS_LSEEK(m_fileDescriptor, 0, TPIE_OS_FLAG_SEEK_END)) < m_osBlockSize) {
	if (m_readOnly) {

	    m_status = BTE_STREAM_STATUS_INVALID;

	    TP_LOG_FATAL ("No header block in read only stream \"");
	    TP_LOG_FATAL (m_path);
	    TP_LOG_FATAL ('\n');
	    TP_LOG_FLUSH_LOG;
	    return;

	} 
	else {
	    // A writable stream, so we can ftruncate() space for a
	    // header block.
	    if (TPIE_OS_FTRUNCATE (m_fileDescriptor, m_osBlockSize)) {

		m_osErrno = errno;

		TP_LOG_FATAL ("Failed to ftruncate() to end of header of \"");
		TP_LOG_FATAL (m_path);
		TP_LOG_FATAL ("\": ");
		TP_LOG_FATAL (strerror (m_osErrno));
		TP_LOG_FATAL ('\n');
		TP_LOG_FLUSH_LOG;
		return;
	    }
	}
    }

    // Map in the header block.  If the stream is writable, the header 
    // block should be too.
    // took out the SYSTYPE_BSD ifdef for convenience
    // changed from MAP_FIXED to MAP_VARIABLE because we are using NULL
    mmap_hdr = (BTE_stream_header *)
	(call_mmap ((NULL), sizeof (BTE_stream_header),
		    m_readOnly, m_writeOnly,
		    m_fileDescriptor, 0, 0));

    if (mmap_hdr == (BTE_stream_header *) (-1)) {

	m_status = BTE_STREAM_STATUS_INVALID;
	m_osErrno = errno;

	TP_LOG_FATAL ("mmap() failed to map in header from \"");
	TP_LOG_FATAL (m_path);
	TP_LOG_FATAL ("\": ");
	TP_LOG_FATAL (strerror (m_osErrno));
	TP_LOG_FATAL ("\n");
	TP_LOG_FLUSH_LOG;

	return;
    }

    memcpy (mmap_hdr, m_header, sizeof (BTE_stream_header));
    call_munmap (mmap_hdr, sizeof (BTE_stream_header));
}

//
// Make sure the current block is mapped in and all internal pointers are
// set as appropriate.  
// 
// 

template < class T >
inline BTE_err BTE_stream_mmap < T >::validate_current () {
    TPIE_OS_SIZE_T blockSpace;		// The space left in the current block.
    BTE_err bte_err;

    // If the current block is valid and current points into it and has
    // enough room in the block for a full item, we are fine.  If it is
    // valid but there is not enough room, invalidate it.
    if (m_blockValid) {
	assert (m_currentItem);		// sanity check - rajiv
	if ((blockSpace = m_header->m_blockSize -
	     ((char *) m_currentItem - (char *) m_currentBlock)) >= 
	    sizeof (T)) {
	    return BTE_ERROR_NO_ERROR;
	} 
	else {			// Not enough room left.
	    // no real need to call invalidate here 
	    // since we call map_current anyway Rajiv
	    if ((bte_err = unmap_current ()) != BTE_ERROR_NO_ERROR) {
		return bte_err;
	    }
	    m_fileOffset += blockSpace;
	}
    }
    // The current block is invalid, since it was either invalid to start
    // with or we just invalidated it because we were out of space.

    tp_assert (!m_blockValid, "Block is already mapped in.");

    // Now map it the block.
    bte_err = map_current ();
    assert (m_currentItem);

#ifdef VERBOSE
    // Rajiv
    if (verbose && bte_err != BTE_ERROR_NO_ERROR)
	cerr << "validate_current failed\n";
#endif

    // Rajiv
    tp_assert (m_fileOffset + sizeof (T) <= m_fileLength,
	       "Advanced too far somehow.");
    return bte_err;
}

// Map in the current block.
// m_fileOffset is used to determine what block is needed.
template < class T > BTE_err BTE_stream_mmap < T >::map_current () {
    TPIE_OS_OFFSET blockOffset;
    bool do_mmap = false;
    BTE_err err;

    // We should not currently have a valid block.
    tp_assert (!m_blockValid, "Block is already mapped in.");

    // Determine the offset of the block that the current item is in.
    blockOffset = BLOCK_OFFSET (m_fileOffset);
    // Rajiv
    // - m_osBlockSize) / m_header->m_blockSize)
    // * m_header->m_blockSize + m_osBlockSize;

    // If the block offset is beyond the logical end of the file, then
    // we either record this fact and return (if the stream is read
    // only) or ftruncate() out to the end of the current block.
    assert (TPIE_OS_LSEEK(m_fileDescriptor, 0, TPIE_OS_FLAG_SEEK_END) ==
	    m_fileLength);

    // removed -1 from rhs of comparison below Rajiv
    if (m_fileLength < blockOffset + m_header->m_blockSize) {
	if (m_readOnly) {
	    //TP_LOG_WARNING_ID("hit eof while reading\n");
	    return BTE_ERROR_END_OF_STREAM;
	} 
	else {
	    err = grow_file (blockOffset);
	    if (err != BTE_ERROR_NO_ERROR) {
		return err;
	    }
	}
    }
    // this is what we just fixed. Rajiv
    tp_assert (m_fileOffset + sizeof (T) <= m_fileLength,
	       "Advanced too far somehow.");

    // If the current block is already mapped in by this process then
    // some systems, (e.g. HP-UX), will not allow us to map it in
    // again.  This presents all kinds of problems, not only with
    // sub/super-stream interactions, which we could probably detect
    // by looking back up the path to the level 0 stream, but also
    // with overlapping substreams, which are very hard to detect
    // since the application can build them however it sees fit.  We
    // can also have problems if we break a stream into two substreams
    // such that their border is in the middle of a block, and then we
    // read to the end of the fisrt substream while we are still at
    // the beginning of the second.

    // Map it in either r/w or read only.  
#ifdef BTE_STREAM_MMAP_READ_AHEAD
    if (m_haveNextBlock && (blockOffset == m_nextBlockOffset)) {
	T *temp;

	temp            = m_currentBlock;
	m_currentBlock  = m_nextBlock;
	m_nextBlock     = temp;
	m_haveNextBlock = false;
#ifdef COLLECT_STATS
	stats_hits++;
#endif
    } 
    else {
#ifdef COLLECT_STATS
	if (m_haveNextBlock) {
	    // not sequential access
	    //munmap((caddr_t)m_nextBlock, m_header->m_blockSize);
	    //m_haveNextBlock = false;
	    //m_nextBlock = NULL;
	    stats_misses++;
	}
	stats_compulsory++;
#endif
	do_mmap = true;
    }
#else
    do_mmap = true;
#endif
    if (do_mmap) {

	// took out the SYSTYPE_BSD ifdef for convenience
	// MAP_VARIABLE the first time round
	// (m_currentBlock ? MAP_FIXED : MAP_VARIABLE) |
	if (blockOffset + m_header->m_blockSize > m_fileLength) {
	    grow_file(blockOffset);
	}

	m_currentBlock = (T *) (call_mmap (m_currentBlock, 
					   m_header->m_blockSize,
					   m_readOnly, m_writeOnly, 
					   m_fileDescriptor, 
					   blockOffset,
					   (m_currentBlock != NULL)));
	m_blockMapped = false;
    }

    assert ((void *) m_currentBlock != (void *) m_header);

    if (m_currentBlock == (T *) (-1)) {

	m_status = BTE_STREAM_STATUS_INVALID;
	m_osErrno = errno;

	TP_LOG_FATAL ("mmap() failed to map in block at ");
	TP_LOG_FATAL (blockOffset);
	TP_LOG_FATAL (" from \"");
	TP_LOG_FATAL (m_path);
	TP_LOG_FATAL ("\": ");
	TP_LOG_FATAL (strerror (m_osErrno));
	TP_LOG_FATAL ('\n');
	TP_LOG_FLUSH_LOG;
	perror ("mmap failed");	// Rajiv

	return BTE_ERROR_OS_ERROR;
    }

    m_blockValid = true;    

#ifdef BTE_STREAM_MMAP_READ_AHEAD
    // Start the asyncronous read of the next logical block.
    read_ahead ();
#endif

    // The offset, in terms of number of items, that current should
    // have relative to m_currentBlock.

    register TPIE_OS_OFFSET internalBlockOffset;

    internalBlockOffset = file_off_to_item_off (m_fileOffset) %
	(m_header->m_blockSize / sizeof (T));

    m_currentItem = m_currentBlock + internalBlockOffset;
    assert (m_currentItem);

    record_statistics(BLOCK_READ);

    return BTE_ERROR_NO_ERROR;
}

template < class T >
inline BTE_err BTE_stream_mmap < T >::invalidate_current ()
{
    // We should currently have a valid block.
    tp_assert (m_blockValid, "No block is mapped in.");
    m_blockValid = false;

    return BTE_ERROR_NO_ERROR;
}

template < class T > BTE_err BTE_stream_mmap < T >::unmap_current () {

    //  2003/02/27: Commented this out as it causes test_ami_pmerge to crash.
    //  (reason: destructor for superstream fails when using substreams)
    //  Jan.
    //    invalidate_current ();	// not really necessary

    // Unmap it.
    if (call_munmap (m_currentBlock, m_header->m_blockSize)) {

	m_status = BTE_STREAM_STATUS_INVALID;
	m_osErrno = errno;

	TP_LOG_FATAL ("munmap() failed to unmap current block");
	TP_LOG_FATAL ("\": ");
	TP_LOG_FATAL (strerror (m_osErrno));
	TP_LOG_FATAL ('\n');
	TP_LOG_FLUSH_LOG;

	return BTE_ERROR_OS_ERROR;
    }

    m_currentBlock = NULL;		// to be safe
    m_blockMapped  = false;
    m_blockValid   = false;

    record_statistics(BLOCK_WRITE);

    return BTE_ERROR_NO_ERROR;
}

// A uniform method for advancing the current pointer.  No mapping,
// unmapping, or anything like that is done here.
template < class T >
inline BTE_err BTE_stream_mmap < T >::advance_current () {

    tp_assert (m_fileOffset <= m_fileLength, "Advanced too far somehow.");

    // Advance the current pointer and the file offset of the current
    // item.
    m_currentItem++;
    m_fileOffset += sizeof (T);

    return BTE_ERROR_NO_ERROR;
}


// increase the length of the file, to at least 
// blockOffset + m_header->m_blockSize
template < class T > inline BTE_err
BTE_stream_mmap < T >::grow_file (TPIE_OS_OFFSET blockOffset)
{
    // can't grow substreams (except if called for the
    // last substream in a stream. this may happen if map_current
    // maps in the last block of a (sub-)stream).
    // (tavi) I took this out since ignoreSubstream is not declared...
    //    assert (ignoreSubstream || !m_substreamLevel);
    assert(!m_substreamLevel);

    m_fileLength = blockOffset + m_header->m_blockSize;
    if (TPIE_OS_FTRUNCATE (m_fileDescriptor, m_fileLength) < 0) {

	m_osErrno = errno;

	TP_LOG_FATAL ("Failed to ftruncate() out a new block of \"");
	TP_LOG_FATAL (m_path);
	TP_LOG_FATAL ("\": ");
	TP_LOG_FATAL (strerror (m_osErrno));
	TP_LOG_FATAL ('\n');
	TP_LOG_FLUSH_LOG;
	return BTE_ERROR_END_OF_STREAM;	// generate an error Rajiv
    }
    assert (TPIE_OS_LSEEK(m_fileDescriptor, 0, TPIE_OS_FLAG_SEEK_END) ==
	    m_fileLength);
    return BTE_ERROR_NO_ERROR;
}


template < class T >
TPIE_OS_OFFSET BTE_stream_mmap < T >::item_off_to_file_off (TPIE_OS_OFFSET itemOffset) const {
    TPIE_OS_OFFSET fileOffset;

    // Move past the header.
    fileOffset = m_osBlockSize;

    // Add m_header->m_blockSize for each full block.
    fileOffset += m_header->m_blockSize *
	(itemOffset / (m_header->m_blockSize / sizeof (T)));

    // Add sizeof(T) for each item in the partially full block.
    fileOffset += sizeof (T) * 
	(itemOffset % (m_header->m_blockSize / sizeof (T))); 

    return fileOffset;
}

template < class T >
TPIE_OS_OFFSET BTE_stream_mmap < T >::file_off_to_item_off (TPIE_OS_OFFSET fileOffset) const {
    TPIE_OS_OFFSET itemOffset;

    // Subtract off the header.
    fileOffset -= m_osBlockSize;

    // Account for the full blocks.
    itemOffset = (m_header->m_blockSize / sizeof (T)) *
	(fileOffset / m_header->m_blockSize);

    // Add in the number of items in the last block.
    itemOffset += (fileOffset % m_header->m_blockSize) / sizeof (T);

    return itemOffset;
}

template < class T > 
TPIE_OS_OFFSET BTE_stream_mmap < T >::chunk_size () const {
    return m_header->m_blockSize / sizeof (T);
}

#ifdef VERBOSE
// pref = prefix
template < class T > void BTE_stream_mmap < T >::print (char *pref) {
    
#ifdef COLLECT_STATS
#ifdef BTE_STREAM_MMAP_READ_AHEAD
    fprintf (stdout, "%sPFSTATS %d %d %d %d (m_fileDescriptor=%d)\n",
	     pref, stats_hits, stats_misses,
	     stats_compulsory - stats_misses, stats_eos, m_fileDescriptor);
    cout << pref << stats << " RWMUSCD\n";
#endif
#endif
    fprintf (stdout, "%sfile=%s", pref, m_path);
    fprintf (stdout, ", m_logicalEndOfStream=%d, m_fileLength=%d", m_logicalEndOfStream, m_fileLength);
    fprintf (stdout, ", length=%d\n",
	     file_off_to_item_off (m_logicalEndOfStream) - 
	     file_off_to_item_off (m_logicalBeginOfStream));
    fprintf (stdout, "\n");
}
#endif // VERBOSE

#ifdef BTE_STREAM_MMAP_READ_AHEAD
template < class T > 
void BTE_stream_mmap < T >::read_ahead () {

    TPIE_OS_OFFSET currentBlock;

    // The current block had better already be valid or we made a
    // mistake in being here.

    tp_assert (m_blockValid,
	       "Trying to read ahead when current block is invalid.");

    // Check whether there is a next block.  If we are already in the
    // last block of the file then it makes no sense to read ahead.
    // What if we are writing?? Rajiv
    currentBlock = ((m_fileOffset - m_osBlockSize) / m_header->m_blockSize) *
	m_header->m_blockSize + m_osBlockSize;

    if (m_logicalEndOfStream < currentBlock + 2 * m_header->m_blockSize) {
	return;			// XXX
	// need to fix this    
	// if not read only, we can extend the file and prefetch.
	// (only if not a substream)
	// Rajiv
	// prefetch only if write only and not substream
	if (!m_writeOnly || m_substreamLevel) {
#ifdef COLLECT_STATS
	    stats_eos++;
#endif
	    return;
	}
	if (m_writeOnly &&
	    !m_substreamLevel &&
	    (currentBlock + 2 * m_header->m_blockSize > m_fileLength)) {
#ifdef VERBOSE
	    if (verbose)
		cout << "growing file (m_fileDescriptor" << m_fileDescriptor << ") in advance\n";
#endif
	    grow_file (currentBlock);
	}
    }

    m_nextBlockOffset = currentBlock + m_header->m_blockSize;

    // Rajiv
    assert (m_nextBlockOffset + m_header->m_blockSize <= m_fileLength);
    assert (m_nextBlock != m_currentBlock);
#if !USE_LIBAIO
    // took out the SYSTYPE_BSD ifdef for readability Rajiv
    m_nextBlock = (T *) (call_mmap (m_nextBlock, m_header->m_blockSize,
				   m_readOnly, m_writeOnly,
				   m_fileDescriptor, m_nextBlockOffset, (m_nextBlock != NULL)));
    assert (m_nextBlock != (T *) - 1);
    m_haveNextBlock = true;
#endif				// !USE_LIBAIO

#if USE_LIBAIO
    // Asyncronously read the first word of each os block in the next
    // logical block.
    for (unsigned int ii = 0; ii < BTE_STREAM_MMAP_BLOCK_FACTOR; ii++) {

	// Make sure there is not a pending request for this block
	// before requesting it.

	if (aio_results[ii].aio_return != AIO_INPROGRESS) {
	    aio_results[ii].aio_return = AIO_INPROGRESS;

	    // We have to cancel the last one, even though it completed,
	    // in order to allow another one with the same result.
	    aiocancel (aio_results + ii);

	    // Start the async I/O.
	    if (aioread (m_fileDescriptor, (char *) (read_ahead_buffer + ii), sizeof (int),
			 m_nextBlockOffset + ii * m_osBlockSize, SEEK_SET,
			 aio_results + ii)) {

		m_osErrno = errno;

		TP_LOG_FATAL ("aioread() failed to read ahead");
		TP_LOG_FATAL ("\": ");
		TP_LOG_FATAL (strerror (m_osErrno));
		TP_LOG_FATAL ('\n');
		TP_LOG_FLUSH_LOG;
	    }
	}
    }
#endif				// USE_LIBAIO
}

#endif				// BTE_STREAM_MMAP_READ_AHEAD

#undef BTE_STREAM_MMAP_MM_BUFFERS

#endif	// _BTE_STREAM_MMAP_H
