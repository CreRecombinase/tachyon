#ifndef CORE_BLOCKENTRY_H_
#define CORE_BLOCKENTRY_H_

#include "PermutationManager.h"
#include "../index/IndexBlockEntry.h"
#include "StreamContainer.h"
#include "../io/vcf/VCFHeader.h"
#include "BlockEntrySettings.h"

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

public:
	BlockEntry();
	~BlockEntry();

	void resize(const U32 s);
	void clear();

	inline void allocateOffsets(const U32& info, const U32& format, const U32& filter){
		this->index_entry.allocateOffsets(info, format, filter);
	}


	inline void updateContainerSet(Index::IndexBlockEntry::INDEX_BLOCK_TARGET target, hash_container_type& v, buffer_type& buffer){
		// Determine target
		switch(target){
		case(Index::IndexBlockEntry::INDEX_BLOCK_TARGET::INDEX_INFO)   :
			return(this->updateContainer(v, this->info_containers, this->index_entry.info_offsets, this->index_entry.n_info_streams, buffer));
			break;
		case(Index::IndexBlockEntry::INDEX_BLOCK_TARGET::INDEX_FORMAT) :
			return(this->updateContainer(v, this->format_containers, this->index_entry.format_offsets, this->index_entry.n_format_streams, buffer));
			break;
		default: std::cerr << "unknown target type" << std::endl; exit(1);
		}
	}

	inline void updateFilterOffsets(hash_container_type& v){
		for(U32 i = 0; i < this->index_entry.n_filter_streams; ++i){
			this->index_entry.filter_offsets[i].key = v[i];
		}
	}

	void updateBaseContainers(buffer_type& buffer);
	void updateOffsets(void);

	bool read(std::ifstream& stream, const settings_type& settings);

private:
	void updateContainer(hash_container_type& v, stream_container* container, offset_minimal_type* offset, const U32& length, buffer_type& buffer);
	void updateContainer(stream_container& container, offset_minimal_type& offset, buffer_type& buffer, const U32& key);

	friend std::ofstream& operator<<(std::ofstream& stream, const self_type& entry){
		stream << entry.index_entry;

		if(entry.index_entry.controller.hasGTPermuted)
			stream << entry.ppa_manager;

		stream << entry.meta_hot_container;
		stream << entry.meta_cold_container;
		stream << entry.gt_rle_container;
		stream << entry.gt_simple_container;

		for(U32 i = 0; i < entry.index_entry.n_info_streams; ++i){
			//std::cerr << i << "->" << entry.index_entry.info_offsets[i].key << '\t' << entry.info_containers[i].buffer_data.pointer << std::endl;
			assert(entry.index_entry.info_offsets[i].key != 0);
			stream << entry.info_containers[i];
		}

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
	stream_container  meta_cold_container;
	stream_container  gt_rle_container;
	stream_container  gt_simple_container;
	stream_container* info_containers;
	stream_container* format_containers;
};

}
}

#endif /* CORE_BLOCKENTRY_H_ */
