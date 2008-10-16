// Copyright (C) 2001 Octavian Procopiuc
//
// File:    ami_block_base.h
// Author:  Octavian Procopiuc <tavi@cs.duke.edu>
//
// $Id: ami_block_base.h,v 1.14 2004-08-17 16:47:38 jan Exp $
//
// Definition of the AMI_block_base class and supporting types:
// AMI_bid, AMI_block_status.
//

#ifndef _AMI_BLOCK_BASE_H
#define _AMI_BLOCK_BASE_H

// Get definitions for working with Unix and Windows
#include <portability.h>

// The AMI error codes.
#include <err.h>
// The AMI_COLLECTION class.
#include <coll.h>

namespace tpie {

    namespace ami {
	
// AMI block id type.
	typedef TPIE_BLOCK_ID_TYPE bid_t;
	
// Block status type.
	enum block_status {
	    BLOCK_STATUS_VALID = 0,
	    BLOCK_STATUS_INVALID = 1
	};

    }  //  ami namespace

}  //  tpie namespace

namespace tpie {

    namespace ami {
	
	template<class BTECOLL>

	class block_base {

	protected:
	    
	    // Pointer to the block collection.
	    BTECOLL * pcoll_;
	    
	    // Unique ID. Represents the offset of the block in the blocks file.
	    bid_t bid_;
	    
	    // Dirty bit. If set, the block needs to be written back.
	    char dirty_;
	    
	    // Pointer to the actual data.
	    void * pdata_;
	    
	    // Persistence flag.
	    persistence per_;
	    
	public:
	    
	    // Constructor.
	    // Read and initialize a block with a given ID.
	    // When bid is missing or 0, a new block is created.
	    block_base(collection_single<BTECOLL>* pacoll, bid_t bid = 0)
		: bid_(bid), dirty_(0), per_(PERSIST_PERSISTENT) {

		pcoll_ = pacoll->bte();

		if (bid != 0) {

		    // Get an existing block from disk.
		    if (pcoll_->get_block(bid_, pdata_) != bte::NO_ERROR) {
			pdata_ = NULL;
		    }

		} else {

		    // Create a new block in the collection.
		    if (pcoll_->new_block(bid_, pdata_) != bte::NO_ERROR) {
			pdata_ = NULL;
		    }
		}
	    }

	    err sync() {
		if (pcoll_->sync_block(bid_, pdata_) != bte::NO_ERROR)
		    return BTE_ERROR;
		else
		    return NO_ERROR;
	    }

	    // Get the block id.
	    bid_t bid() const { 
		return bid_; 
	    }

	    // Get a reference to the dirty bit.
	    char& dirty() { 
		return dirty_; 
	    };
	    
	    char dirty() const { 
		return dirty_; 
	    }

	    // Copy block rhs into this block.
	    block_base<BTECOLL>& operator=(const block_base<BTECOLL>& rhs) { 
		if (pcoll_ == rhs.pcoll_) {
		    memcpy(pdata_, rhs.pdata_, pcoll_->block_size());
		    dirty_ = 1;
		} else 
		    pdata_ = NULL;
		return *this; 
	    }

	    // Get the block's status.
	    block_status status() const { 
		return (pdata_ == NULL) ? 
		    BLOCK_STATUS_INVALID : BLOCK_STATUS_VALID; 
	    }

	    // Return true if the block is valid.
	    bool is_valid() const {
		return (pdata_ != NULL);
	    }

	    // Return true if the block is invalid.
	    bool operator!() const {
		return (pdata_ == NULL);
	    }

	    void persist(persistence per) { 
		per_ = per; 
	    }

	    persistence persist() const { 
		return per_; 
	    }

	    size_t block_size() const {
		return pcoll_->block_size(); 
	    }

	    // Destructor.
	    ~block_base() {
		// Check first the status of the collection. 
		if (pdata_ != NULL){
		    if (per_ == PERSIST_PERSISTENT) {
			// Write back the block.
			pcoll_->put_block(bid_, pdata_); 
		    } else {
			// Delete the block from the collection.
			pcoll_->delete_block(bid_, pdata_);
		    }
		}
	    }
	};

    }  //  ami namespace

}  //  tpie namespace

#endif //_TPIE_AMI_BLOCK_BASE_H