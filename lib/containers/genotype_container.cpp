#include "containers/genotype_container.h"
#include "containers/primitive_container.h"
#include "containers/stride_container.h"
#include "support/enums.h"

namespace tachyon{
namespace containers{

GenotypeContainer::GenotypeContainer(const block_type& block, const MetaContainer& meta) :
	n_entries(0),
	__meta_container(block),
	__iterators(nullptr)
{
	// Todo: if anything is uniform
	// Support
	const bool uniform_stride = block.base_containers[YON_BLK_GT_SUPPORT].header.data_header.IsUniform();
	PrimitiveContainer<U32> lengths(block.base_containers[YON_BLK_GT_SUPPORT]); // n_runs / objects size

	U64 offset_rle8     = 0; const char* const rle8     = block.base_containers[YON_BLK_GT_INT8].buffer_data_uncompressed.data();
	U64 offset_rle16    = 0; const char* const rle16    = block.base_containers[YON_BLK_GT_INT16].buffer_data_uncompressed.data();
	U64 offset_rle32    = 0; const char* const rle32    = block.base_containers[YON_BLK_GT_INT32].buffer_data_uncompressed.data();
	U64 offset_rle64    = 0; const char* const rle64    = block.base_containers[YON_BLK_GT_INT64].buffer_data_uncompressed.data();
	U64 offset_simple8  = 0; const char* const simple8  = block.base_containers[YON_BLK_GT_S_INT8].buffer_data_uncompressed.data();
	U64 offset_simple16 = 0; const char* const simple16 = block.base_containers[YON_BLK_GT_S_INT16].buffer_data_uncompressed.data();
	U64 offset_simple32 = 0; const char* const simple32 = block.base_containers[YON_BLK_GT_S_INT32].buffer_data_uncompressed.data();
	U64 offset_simple64 = 0; const char* const simple64 = block.base_containers[YON_BLK_GT_S_INT64].buffer_data_uncompressed.data();

	assert(block.base_containers[YON_BLK_GT_INT8].buffer_data_uncompressed.size()    % sizeof(BYTE) == 0);
	assert(block.base_containers[YON_BLK_GT_INT16].buffer_data_uncompressed.size()   % sizeof(U16)  == 0);
	assert(block.base_containers[YON_BLK_GT_INT32].buffer_data_uncompressed.size()   % sizeof(U32)  == 0);
	assert(block.base_containers[YON_BLK_GT_INT64].buffer_data_uncompressed.size()   % sizeof(U64)  == 0);
	assert(block.base_containers[YON_BLK_GT_S_INT8].buffer_data_uncompressed.size()  % sizeof(BYTE) == 0);
	assert(block.base_containers[YON_BLK_GT_S_INT16].buffer_data_uncompressed.size() % sizeof(U16)  == 0);
	assert(block.base_containers[YON_BLK_GT_S_INT32].buffer_data_uncompressed.size() % sizeof(U32)  == 0);
	assert(block.base_containers[YON_BLK_GT_S_INT64].buffer_data_uncompressed.size() % sizeof(U64)  == 0);

	this->n_entries   = meta.size();
	this->__iterators = static_cast<pointer>(::operator new[](this->size() * sizeof(value_type)));

	U64 gt_offset = 0;
	BYTE incrementor = 1;
	if(uniform_stride) incrementor = 0;

	for(U32 i = 0; i < meta.size(); ++i){
		if(meta[i].HasGT()){
			// Case run-length encoding diploid and biallelic and no missing
			if(meta[i].GetGenotypeEncoding() == TACHYON_GT_ENCODING::YON_GT_RLE_DIPLOID_BIALLELIC){
				if(meta[i].GetGenotypeType() == TACHYON_GT_PRIMITIVE_TYPE::YON_GT_BYTE){
					new( &this->__iterators[i] ) GenotypeContainerDiploidRLE<BYTE>( &rle8[offset_rle8], lengths[gt_offset], this->__meta_container[i] );
					offset_rle8 += lengths[gt_offset]*sizeof(BYTE);
				} else if(meta[i].GetGenotypeType() == TACHYON_GT_PRIMITIVE_TYPE::YON_GT_U16){
					new( &this->__iterators[i] ) GenotypeContainerDiploidRLE<U16>( &rle16[offset_rle16], lengths[gt_offset], this->__meta_container[i] );
					offset_rle16 += lengths[gt_offset]*sizeof(U16);
				} else if(meta[i].GetGenotypeType() == TACHYON_GT_PRIMITIVE_TYPE::YON_GT_U32){
					new( &this->__iterators[i] ) GenotypeContainerDiploidRLE<U32>( &rle32[offset_rle32], lengths[gt_offset], this->__meta_container[i] );
					offset_rle32 += lengths[gt_offset]*sizeof(U32);
				} else if(meta[i].GetGenotypeType() == TACHYON_GT_PRIMITIVE_TYPE::YON_GT_U64){
					new( &this->__iterators[i] ) GenotypeContainerDiploidRLE<U64>( &rle64[offset_rle64], lengths[gt_offset], this->__meta_container[i] );
					offset_rle64 += lengths[gt_offset]*sizeof(U64);
				} else {
					std::cerr << utility::timestamp("ERROR","GT") << "Unknown GT encoding primitive..." << std::endl;
					exit(1);
				}

			}
			// Case run-length encoding diploid and biallelic/EOV or n-allelic
			else if(meta[i].GetGenotypeEncoding() == TACHYON_GT_ENCODING::YON_GT_RLE_DIPLOID_NALLELIC) {
				if(meta[i].GetGenotypeType() == TACHYON_GT_PRIMITIVE_TYPE::YON_GT_BYTE){
					new( &this->__iterators[i] ) GenotypeContainerDiploidSimple<BYTE>( &simple8[offset_simple8], lengths[gt_offset], this->__meta_container[i] );
					offset_simple8 += lengths[gt_offset]*sizeof(BYTE);
				} else if(meta[i].GetGenotypeType() == TACHYON_GT_PRIMITIVE_TYPE::YON_GT_U16){
					new( &this->__iterators[i] ) GenotypeContainerDiploidSimple<U16>( &simple16[offset_simple16], lengths[gt_offset], this->__meta_container[i] );
					offset_simple16 += lengths[gt_offset]*sizeof(U16);
				} else if(meta[i].GetGenotypeType() == TACHYON_GT_PRIMITIVE_TYPE::YON_GT_U32){
					new( &this->__iterators[i] ) GenotypeContainerDiploidSimple<U32>( &simple32[offset_simple32], lengths[gt_offset], this->__meta_container[i] );
					offset_simple32 += lengths[gt_offset]*sizeof(U32);
				} else if(meta[i].GetGenotypeType() == TACHYON_GT_PRIMITIVE_TYPE::YON_GT_U64){
					new( &this->__iterators[i] ) GenotypeContainerDiploidSimple<U64>( &simple64[offset_simple64], lengths[gt_offset], this->__meta_container[i] );
					offset_simple64 += lengths[gt_offset]*sizeof(U64);
				} else {
					std::cerr << utility::timestamp("ERROR","GT") << "Unknown GT encoding primitive..." << std::endl;
					exit(1);
				}
			}
			// Case BCF-style encoding of diploids
			else if(meta[i].GetGenotypeEncoding() == TACHYON_GT_ENCODING::YON_GT_BCF_DIPLOID) {
				if(meta[i].GetGenotypeType() == TACHYON_GT_PRIMITIVE_TYPE::YON_GT_BYTE){
					new( &this->__iterators[i] ) GenotypeContainerDiploidBCF<BYTE>( &simple8[offset_simple8], lengths[gt_offset], this->__meta_container[i] );
					offset_simple8 += lengths[gt_offset]*sizeof(BYTE);
				} else if(meta[i].GetGenotypeType() == TACHYON_GT_PRIMITIVE_TYPE::YON_GT_U16){
					new( &this->__iterators[i] ) GenotypeContainerDiploidBCF<U16>( &simple16[offset_simple16], lengths[gt_offset], this->__meta_container[i] );
					offset_simple16 += lengths[gt_offset]*sizeof(U16);
				} else if(meta[i].GetGenotypeType() == TACHYON_GT_PRIMITIVE_TYPE::YON_GT_U32){
					new( &this->__iterators[i] ) GenotypeContainerDiploidBCF<U32>( &simple32[offset_simple32], lengths[gt_offset], this->__meta_container[i] );
					offset_simple32 += lengths[gt_offset]*sizeof(U32);
				} else if(meta[i].GetGenotypeType() == TACHYON_GT_PRIMITIVE_TYPE::YON_GT_U64){
					new( &this->__iterators[i] ) GenotypeContainerDiploidBCF<U64>( &simple64[offset_simple64], lengths[gt_offset], this->__meta_container[i] );
					offset_simple64 += lengths[gt_offset]*sizeof(U64);
				}  else {
					std::cerr << utility::timestamp("ERROR","GT") << "Unknown GT encoding primitive..." << std::endl;
					exit(1);
				}
			}
			// Case other potential encodings
			else {
				std::cerr << utility::timestamp("ERROR","GT") << "Unknown GT encoding family..." << std::endl;
				new( &this->__iterators[i] ) GenotypeContainerDiploidRLE<BYTE>( );
				exit(1);
			}

			// Increment offset
			gt_offset += incrementor;

		} else { // No GT available
			new( &this->__iterators[i] ) GenotypeContainerDiploidRLE<BYTE>( );
		}
	}

	assert(offset_rle8  == block.base_containers[YON_BLK_GT_INT8].GetSizeUncompressed());
	assert(offset_rle16 == block.base_containers[YON_BLK_GT_INT16].GetSizeUncompressed());
	assert(offset_rle32 == block.base_containers[YON_BLK_GT_INT32].GetSizeUncompressed());
	assert(offset_rle64 == block.base_containers[YON_BLK_GT_INT64].GetSizeUncompressed());
	assert(offset_simple8  == block.base_containers[YON_BLK_GT_S_INT8].GetSizeUncompressed());
	assert(offset_simple16 == block.base_containers[YON_BLK_GT_S_INT16].GetSizeUncompressed());
	assert(offset_simple32 == block.base_containers[YON_BLK_GT_S_INT32].GetSizeUncompressed());
	assert(offset_simple64 == block.base_containers[YON_BLK_GT_S_INT64].GetSizeUncompressed());
}

GenotypeContainer::~GenotypeContainer(){
	for(std::size_t i = 0; i < this->n_entries; ++i)
		(this->__iterators + i)->~GenotypeContainerInterface();

	::operator delete[](static_cast<void*>(this->__iterators));
}

}
}
