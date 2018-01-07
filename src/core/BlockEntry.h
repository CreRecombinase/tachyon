#ifndef CORE_BLOCKENTRY_H_
#define CORE_BLOCKENTRY_H_

#include "PermutationManager.h"
#include "../index/IndexBlockEntry.h"
#include "StreamContainer.h"
#include "../io/vcf/VCFHeader.h"
#include "BlockEntrySettings.h"
#include "ImporterStats.h"
#include "iterator/MetaIterator.h"

namespace Tachyon{
namespace Core{

class BlockEntry{
	typedef BlockEntry self_type;
	typedef Core::StreamContainer stream_container;
	typedef Core::PermutationManager permutation_type;
	typedef Index::IndexBlockEntry index_entry_type;
	typedef Core::Support::HashContainer hash_container_type;
	typedef Core::Support::HashVectorContainer hash_vector_container_type;
	typedef Index::IndexBlockEntryOffsets offset_type;
	typedef Index::IndexBlockEntryHeaderOffsets offset_minimal_type;
	typedef IO::BasicBuffer buffer_type;
	typedef BlockEntrySettings settings_type;
	typedef Tachyon::Support::ImporterStats import_stats_type;
	typedef Iterator::MetaIterator meta_iterator_type;

public:
	BlockEntry();
	~BlockEntry();

	void resize(const U32 s);
	void clear();

	inline void allocateOffsets(const U32& info, const U32& format, const U32& filter){
		this->index_entry.allocateOffsets(info, format, filter);
	}


	inline void updateContainerSet(Index::IndexBlockEntry::INDEX_BLOCK_TARGET target){
		// Determine target
		switch(target){
		case(Index::IndexBlockEntry::INDEX_BLOCK_TARGET::INDEX_INFO)   :
			return(this->updateContainer(this->info_containers, this->index_entry.n_info_streams));
			break;
		case(Index::IndexBlockEntry::INDEX_BLOCK_TARGET::INDEX_FORMAT) :
			return(this->updateContainer(this->format_containers, this->index_entry.n_format_streams));
			break;
		default: std::cerr << "unknown target type" << std::endl; exit(1);
		}
	}

	void updateBaseContainers(void);
	void updateOffsets(void);

	bool read(std::ifstream& stream, settings_type& settings);

	bool write(std::ofstream& stream, import_stats_type& stats, import_stats_type& stats_uncompressed){
		U64 last_pos = stream.tellp();
		stream << this->index_entry;
		stats.total_header_cost += (U64)stream.tellp() - last_pos;
		last_pos = stream.tellp();

		if(this->index_entry.controller.hasGTPermuted){
			stream << this->ppa_manager;
			stats.total_ppa_cost += (U64)stream.tellp() - last_pos;
			stats_uncompressed.total_ppa_cost += this->ppa_manager.PPA.pointer;
			last_pos = stream.tellp();
		}

		stream << this->meta_hot_container;
		stream << this->meta_cold_container;
		stats.total_meta_cost += (U64)stream.tellp() - last_pos;
		stats_uncompressed.total_meta_cost += this->meta_hot_container.buffer_data_uncompressed.pointer;
		stats_uncompressed.total_meta_cost += this->meta_cold_container.buffer_data_uncompressed.pointer;
		last_pos = stream.tellp();

		stream << this->gt_rle_container;
		stream << this->gt_simple_container;
		stream << this->gt_support_data_container;
		stats.total_gt_cost += (U64)stream.tellp() - last_pos;
		stats_uncompressed.total_gt_cost += this->gt_rle_container.buffer_data_uncompressed.pointer;
		stats_uncompressed.total_gt_cost += this->gt_simple_container.buffer_data_uncompressed.pointer;
		stats_uncompressed.total_gt_cost += this->gt_support_data_container.buffer_data_uncompressed.pointer;
		last_pos = stream.tellp();

		stream << this->meta_info_map_ids;
		stream << this->meta_filter_map_ids;
		stream << this->meta_format_map_ids;
		stats.total_meta_cost += (U64)stream.tellp() - last_pos;
		stats_uncompressed.total_meta_cost += this->meta_info_map_ids.buffer_data_uncompressed.pointer;
		stats_uncompressed.total_meta_cost += this->meta_filter_map_ids.buffer_data_uncompressed.pointer;
		stats_uncompressed.total_meta_cost += this->meta_format_map_ids.buffer_data_uncompressed.pointer;
		last_pos = stream.tellp();

		for(U32 i = 0; i < this->index_entry.n_info_streams; ++i){
			stream << this->info_containers[i];
			stats_uncompressed.total_info_cost += this->info_containers[i].buffer_data_uncompressed.pointer;
		}

		stats.total_info_cost += (U64)stream.tellp() - last_pos;
		last_pos = stream.tellp();

		for(U32 i = 0; i < this->index_entry.n_format_streams; ++i){
			stream << this->format_containers[i];
			stats_uncompressed.total_format_cost += this->format_containers[i].buffer_data_uncompressed.pointer;
		}

		stats.total_format_cost += (U64)stream.tellp() - last_pos;
		last_pos = stream.tellp();

		stream.write(reinterpret_cast<const char*>(&Constants::TACHYON_BLOCK_EOF), sizeof(U64));

		return(true);
	}

	meta_iterator_type* getMetaIterator(void){
		meta_iterator_type* it;
		if(this->meta_cold_container.buffer_data_uncompressed.size())
			it = new meta_iterator_type(this->meta_hot_container, this->meta_cold_container);
		else
			it = new meta_iterator_type(this->meta_hot_container);

		if(this->meta_info_map_ids.buffer_data_uncompressed.size())
			it->setInfoIDContainer(this->meta_info_map_ids);

		if(this->meta_filter_map_ids.buffer_data_uncompressed.size())
			it->setFilterIDContainer(this->meta_filter_map_ids);

		if(this->meta_format_map_ids.buffer_data_uncompressed.size())
			it->setFormatIDContainer(this->meta_format_map_ids);

		return(it);
	}

private:
	void updateContainer(stream_container* container, const U32& length);
	void updateContainer(stream_container& container, bool reformat = true);

	friend std::ofstream& operator<<(std::ofstream& stream, const self_type& entry){
		stream << entry.index_entry;

		if(entry.index_entry.controller.hasGTPermuted)
			stream << entry.ppa_manager;

		stream << entry.meta_hot_container;
		stream << entry.meta_cold_container;
		stream << entry.gt_rle_container;
		stream << entry.gt_simple_container;
		stream << entry.gt_support_data_container;
		stream << entry.meta_info_map_ids;
		stream << entry.meta_filter_map_ids;
		stream << entry.meta_format_map_ids;

		for(U32 i = 0; i < entry.index_entry.n_info_streams; ++i)
			stream << entry.info_containers[i];


		for(U32 i = 0; i < entry.index_entry.n_format_streams; ++i)
			stream << entry.format_containers[i];

		stream.write(reinterpret_cast<const char*>(&Constants::TACHYON_BLOCK_EOF), sizeof(U64));

		return(stream);
	}

	// Read everything
	friend std::ifstream& operator>>(std::ifstream& stream, self_type& entry){
		stream >> entry.index_entry;

		if(entry.index_entry.controller.hasGTPermuted) stream >> entry.ppa_manager;
		stream >> entry.meta_hot_container;
		stream >> entry.meta_cold_container;
		stream >> entry.gt_rle_container;
		stream >> entry.gt_simple_container;

		for(U32 i = 0; i < entry.index_entry.n_info_streams; ++i)
			stream >> entry.info_containers[i];


		for(U32 i = 0; i < entry.index_entry.n_format_streams; ++i)
			stream >> entry.format_containers[i];

		U64 eof_marker;
		stream.read(reinterpret_cast<char*>(&eof_marker), sizeof(U64));
		assert(eof_marker == Constants::TACHYON_BLOCK_EOF);

		return(stream);
	}

public:
	index_entry_type  index_entry;
	permutation_type  ppa_manager;
	stream_container  meta_hot_container;


	stream_container  meta_info_map_ids;
	stream_container  meta_format_map_ids;
	stream_container  meta_filter_map_ids;

	stream_container  meta_cold_container;

	stream_container  gt_support_data_container; // data (0: rle, 1: simple), strides (n_objects)

	stream_container  gt_rle_container;
	stream_container  gt_simple_container;
	stream_container* info_containers;
	stream_container* format_containers;
};

}
}

#endif /* CORE_BLOCKENTRY_H_ */
