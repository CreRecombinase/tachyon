#ifndef CORE_BLOCKENTRY_H_
#define CORE_BLOCKENTRY_H_

namespace Tachyon{
namespace Core{

class BlockEntry{
	typedef BlockEntry self_type;
	typedef Core::StreamContainer stream_container;
	typedef Core::PermutationManager permutation_type;
	typedef Core::EntryHotMetaBase meta_type;
	typedef Core::EntryColdMeta meta_cold_type;
	typedef Index::IndexBlockEntry index_entry_type;
	typedef Core::Support::HashContainer hash_container_type;
	typedef Core::Support::HashVectorContainer hash_vector_container_type;
	typedef Index::IndexBlockEntryOffsets offset_type;

public:
	BlockEntry() :
		info_containers(new stream_container[100]),
		format_containers(new stream_container[100]),
		filter_containers(new stream_container[100])
	{
		for(U32 i = 0; i < 100; ++i){
			this->info_containers[i].resize(65536*4);
			this->format_containers[i].resize(65536*4);
			this->filter_containers[i].resize(65536*4);
		}
	}
	~BlockEntry(){
		delete [] this->info_containers;
	}

	void resize(const U32 s){
		if(s == 0) return;
		this->meta_hot_container.resize(s);
		this->meta_cold_container.resize(s);
		this->gt_rle_container.resize(s);
		this->gt_simple_container.resize(s);
	}

	void clear(){
		for(U32 i = 0; i < 100; ++i){
			this->info_containers[i].reset();
			this->format_containers[i].reset();
			this->filter_containers[i].reset();
		}
		this->meta_hot_container.reset();
		this->meta_cold_container.reset();
		this->gt_rle_container.reset();
		this->gt_simple_container.reset();
		this->index_entry.reset();
		this->ppa_manager.reset();
	}

	inline void allocateOffsets(const U32& info, const U32& format, const U32& filter){
		this->index_entry.allocateOffsets(info, format, filter);
	}


	inline void updateContainer(Index::IndexBlockEntry::INDEX_BLOCK_TARGET target, hash_container_type& v){
		// Determine target
		switch(target){
		case(Index::IndexBlockEntry::INDEX_BLOCK_TARGET::INDEX_INFO)   :
			return(this->updateContainer(v, this->info_containers, this->index_entry.info_offsets, this->index_entry.n_info_streams));
			break;
		case(Index::IndexBlockEntry::INDEX_BLOCK_TARGET::INDEX_FORMAT) :
		return(this->updateContainer(v, this->format_containers, this->index_entry.format_offsets, this->index_entry.n_format_streams));
			break;
		case(Index::IndexBlockEntry::INDEX_BLOCK_TARGET::INDEX_FILTER) :
		return(this->updateContainer(v, this->filter_containers, this->index_entry.filter_offsets, this->index_entry.n_filter_streams));
			break;
		default: std::cerr << "unknown target type" << std::endl; exit(1);
		}
	}

private:
	void updateContainer(hash_container_type& v, stream_container* container, offset_type* offset, const U32& length){
		for(U32 i = 0; i < length; ++i){
			if(container[i].buffer_data.size() == 0)
				continue;

			// Check if stream is uniform in content
			container[i].checkUniformity();
			// Reformat stream to use as small word size as possible
			container[i].reformat();
			// Add CRC32 checksum for sanity
			container[i].checkSum();

			// Set uncompressed length
			container[i].header.uLength = container[i].buffer_data.pointer;

			// Update offset value if stride is not mixed
			if(container[i].header.controller.mixedStride == false)
				offset[i].update(i, container[i].header);

			std::cerr << v[i] << '\t' << container[i].buffer_data.size() << '\t' << 0 << std::endl;

			// If we have mixed striding
			if(container[i].header.controller.mixedStride){
				std::cerr << "stride: " << container[i].header.stride << '\t' << (U32)container[i].header.controller.type << std::endl;

				// Update offset with mixed stride
				offset[i].update(0, container[i].header, container[i].header_stride);

				//container[i].header_stride.uLength = container[i].buffer_strides.pointer;
				//container[i].header_stride.cLength = cont.buffer.pointer;
				std::cerr << v[i] << "-ADD\t" << container[i].buffer_strides.size() << '\t' << 0 << std::endl;
			}
		}
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
	stream_container* filter_containers;
};

}
}



#endif /* CORE_BLOCKENTRY_H_ */