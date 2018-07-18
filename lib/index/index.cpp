#include <iostream>

#include "index.h"

namespace tachyon{
namespace index{

bool Index::buildMetaIndex(void){
	// This criterion should never be satisfied
	if(this->index_.size() == 0)
		return false;

	for(U32 c = 0; c < this->index_.size(); ++c){ // foreach contig
		if(this->index_.linear_at(c).size() == 0){
			this->index_meta_ += entry_meta_type();
			continue;
		}

		entry_meta_type indexindex;
		indexindex(this->index_.linear_at(c)[0]); // Start reference
		for(U32 i = 1; i < this->index_.linear_at(c).size(); ++i){
			if(indexindex == this->index_.linear_at(c)[i]) // If the blocks share the same contig identifier
				indexindex += this->index_.linear_at(c)[i];
			else { // Otherwise push one entry onto the chain and start a new reference
				this->index_meta_ += indexindex;
				indexindex(this->index_.linear_at(c)[i]);
			}
		}

		this->index_meta_ += indexindex;
	}

	return true;
}

std::vector<IndexEntry> Index::findOverlap(const U32& contig_id) const{
	if(contig_id > this->getMetaIndex().size())
		return(std::vector<entry_type>());

	std::vector<entry_type> yon_blocks;
	for(U32 i = 0; i < this->getIndex().linear_at(contig_id).size(); ++i){
		yon_blocks.push_back(this->getIndex().linear_at(contig_id).at(i));
	}

	return(yon_blocks);
}

std::vector<IndexEntry> Index::findOverlap(const U32& contig_id, const U64& start_pos, const U64& end_pos) const{
	if(contig_id > this->getMetaIndex().size())
		return(std::vector<entry_type>());

	if(this->getMetaIndex().at(contig_id).n_blocks == 0)
		return(std::vector<entry_type>());

	const U32 block_offset_start = this->getIndex().linear_at(contig_id).at(0).blockID;

	std::cerr << "Linear index " << this->getIndex().linear_at(contig_id).size() << std::endl;
	for(U32 i = 0; i < this->getIndex().linear_at(contig_id).size(); ++i){
		this->getIndex().linear_at(contig_id).at(i).print(std::cerr) << std::endl;
	}


	// We also need to know possible overlaps in the quad-tree:
	// Seek from root to origin in quad-tree for potential overlapping bins with counts > 0

	// Retrieve vector of bins that might contain the data
	// The possibleBins function does not check if they exist
	std::vector<bin_type> possible_chunks = this->index_[contig_id].possibleBins(start_pos, end_pos);
	std::vector<U32> yon_blocks;
	std::cerr << "Possible chunks: " << possible_chunks.size() << std::endl;

	// Check if possible bins exists in the linear index
	for(U32 i = 0; i < possible_chunks.size(); ++i){
		// Cycle over the YON blocks this bin have data mapping to
		for(U32 j = 0; j < possible_chunks[i].size(); ++j){
			const U32 used_bins = possible_chunks[i][j] - block_offset_start;
			std::cerr << i << "/" << possible_chunks[i].size() << "\t";
			possible_chunks[i].print(std::cerr) << std::endl;

			// Check [a, b] overlaps with [x, y] iff b > x and a < y.
			// a = this->getIndex().linear_at(contig_id)[possible_bins[i][j]].minPosition;
			// b = this->getIndex().linear_at(contig_id)[possible_bins[i][j]].maxPosition;
			// x = start_pos;
			// y = end_pos;
			if(this->getIndex().linear_at(contig_id)[used_bins].minPosition < end_pos &&
			   this->getIndex().linear_at(contig_id)[used_bins].maxPosition > start_pos)
			{
				yon_blocks.push_back(used_bins);
			}
		}
	}

	// Return nothing if all empty
	if(yon_blocks.size() == 0)
		return(std::vector<entry_type>());

	// Sort to dedupe
	std::sort(yon_blocks.begin(), yon_blocks.end());

	// Dedupe
	std::vector<entry_type> yon_blocks_deduped;
	yon_blocks_deduped.push_back(this->getIndex().linear_at(contig_id)[yon_blocks[0]]);

	for(U32 i = 1; i < yon_blocks.size(); ++i){
		if(yon_blocks[i] != yon_blocks_deduped.back().blockID){
			yon_blocks_deduped.push_back(this->getIndex().linear_at(contig_id)[yon_blocks[i]]);
		}
	}

	// Debug
	//for(U32 i = 0; i < yon_blocks_deduped.size(); ++i){
	//	yon_blocks_deduped[i].print(std::cerr);
	//	std::cerr << std::endl;
	//}

	return(yon_blocks_deduped);
}

}
}
