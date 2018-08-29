#include "vcf_importer_slave.h"

namespace tachyon{

bool VcfImporterSlave::Add(vcf_container_type& container, const uint32_t block_id){
	// Clear current data.
	this->block.clear();
	this->index_entry.reset();

	// Assign new block id. This is provided by the producer.
	this->block_id = block_id;
	this->index_entry.blockID = block_id;

	if(this->n_blocks_processed++ == 0){
		// Allocate containers and offsets for this file.
		// This is not strictly necessary but prevents nasty resize
		// calls in most cases.
		this->block.Allocate(this->vcf_header_->info_fields_.size(),
							 this->vcf_header_->format_fields_.size(),
							 this->vcf_header_->filter_fields_.size());

		// Resize containers
		const uint32_t resize_to = this->settings_.checkpoint_n_snps * sizeof(uint32_t) * 2; // small initial allocation
		this->block.resize(resize_to);
	}

	// This pointer here is borrowed from the PPA manager
	// during import stages. Do not destroy the target block
	// before finishing with this.
	this->block.gt_ppa = &this->permutator.permutation_array;

	if(this->GT_available_ && this->settings_.permute_genotypes){
		// Only store the permutation array if the number of samples
		// are greater then one (1).
		if(this->vcf_header_->GetNumberSamples() > 1){
			if(this->permutator.Build(container, *this->vcf_header_) == false)
				return false;

			this->block.header.controller.hasGTPermuted = true;
		}
	}

	if(this->AddRecords(container) == false) return false;

	this->block.header.controller.hasGT  = this->GT_available_;
	this->block.header.n_variants        = container.sizeWithoutCarryOver();
	this->block.UpdateContainers();

	// Perform compression using standard parameters.
	if(!this->compression_manager.Compress(this->block, this->settings_.compression_level, 6)){
		std::cerr << utility::timestamp("ERROR","COMPRESSION") << "Failed to compress..." << std::endl;
		return false;
	}

	// Encrypt the variant block if desired.
	if(this->settings_.encrypt_data){
		// Generate field-unique identifiers.
		//this->GenerateIdentifiers();

		// Start encryption.
		//this->block.header.controller.anyEncrypted = true;
		//if(!encryption_manager.encrypt(this->block, keychain, YON_ENCRYPTION_AES_256_GCM)){
		//	std::cerr << utility::timestamp("ERROR","COMPRESSION") << "Failed to encrypt..." << std::endl;
		//}
	}

	this->block.Finalize();
	// After all compression and writing is finished the header
	// offsets are themselves compressed and stored in the block.
	this->block.PackFooter(); // Pack footer into buffer.
	this->compression_manager.zstd_codec.Compress(block.footer_support);

	return true;
}

bool VcfImporterSlave::AddRecords(const vcf_container_type& container){
	// Allocate memory for the meta entries.
	meta_type* meta_entries = static_cast<meta_type*>(::operator new[](container.sizeWithoutCarryOver() * sizeof(meta_type)));

	// Iterate over the Vcf container and invoke the meta entry
	// ctor with the target htslib bcf1_t entry provided. Internally
	// converts the bcf1_t data members into the allelic structure
	// that tachyon uses.
	for(uint32_t i = 0; i < container.sizeWithoutCarryOver(); ++i){
		// Transmute a bcf record into a meta structure
		new( meta_entries + i ) meta_type( container[i], this->block.header.minPosition );

		// Add the record data from the target bcf1_t entry to the
		// block byte streams.
		if(this->AddRecord(container, i, meta_entries[i]) == false)
			return false;
	}

	// Add FORMAT:GT field data.
	this->encoder.Encode(container, meta_entries, this->block, this->permutator.permutation_array);

	// Interleave meta records out to the destination block byte
	// streams.
	for(uint32_t i = 0; i < container.sizeWithoutCarryOver(); ++i) this->block += meta_entries[i];

	// Invoke destructor for meta entries.
	for(std::size_t i = 0; i < container.sizeWithoutCarryOver(); ++i) (meta_entries + i)->~MetaEntry();
	::operator delete[](static_cast<void*>(meta_entries));

	return true;
}

bool VcfImporterSlave::AddRecord(const vcf_container_type& container, const uint32_t position, meta_type& meta){
	// Ascertain that the provided position does not exceed the maximum
	// reported length of the target contig.
	if(container.at(position)->pos > this->vcf_header_->GetContig(container.at(position)->rid)->n_bases){
		std::cerr << utility::timestamp("ERROR", "IMPORT") <<
				this->vcf_header_->GetContig(container.at(position)->rid)->name << ':' << container.at(position)->pos + 1 <<
				" > reported max size of contig (" << this->vcf_header_->GetContig(container.at(position)->rid)->n_bases + 1 << ")..." << std::endl;
		return false;
	}

	// Add Filter and Info data.
	if(this->AddVcfFilterInfo(container.at(position), meta) == false) return false;
	if(this->AddVcfInfo(container.at(position), meta) == false) return false;

	// Add Format data.
	if(container.at(position)->n_fmt){
		if(this->AddVcfFormatInfo(container.at(position), meta) == false) return false;

		// Perform these actions if FORMAT:GT data is available.
		const int& hts_format_key = container.at(position)->d.fmt[0].id; // htslib IDX value
		if(this->vcf_header_->GetFormat(hts_format_key)->id != "GT"){
			meta.controller.gt_available = false;
		} else
			meta.controller.gt_available = true;
	}

	// Update the tachyon index.
	if(this->IndexRecord(container.at(position), meta) == false) return false;
	return true;
}

bool VcfImporterSlave::AddVcfFilterInfo(const bcf1_t* record, meta_type& meta){
	// Add FILTER id list to the block. Filter information is unique in that the
	// data is not stored as (key,value)-tuples but as a key id. Because no data
	// is stored in the block, only the unique vectors of ids and their unique
	// ids are collected here. These keys are used to construct bit-vectors for
	// set-membership tests.
	std::vector<int> filter_ids;
	const int& n_filter_fields = record->d.n_flt;

	// Iterate over available Filter fields.
	for(uint32_t i = 0; i < n_filter_fields; ++i){
		const int& hts_filter_key = record->d.flt[i]; // htslib IDX value
		const uint32_t global_key = this->filter_reorder_map_[hts_filter_key]; // tachyon global IDX value
		const uint32_t target_container = this->block.AddFilter(global_key);
		assert(target_container < 65536);
		filter_ids.push_back(global_key);
	}

	return(this->AddVcfFilterPattern(filter_ids, meta));
}

bool VcfImporterSlave::AddVcfInfo(const bcf1_t* record, meta_type& meta){
	// Add INFO fields to the block
	std::vector<int> info_ids;
	const int n_info_fields = record->n_info;

	// Iterate over available Info fields.
	for(uint32_t i = 0; i < n_info_fields; ++i){
		const int& hts_info_key = record->d.info[i].key; // htslib IDX value
		const uint32_t global_key = this->info_reorder_map_[hts_info_key]; // tachyon global IDX value
		const uint32_t target_container = this->block.AddInfo(global_key);
		assert(target_container < 65536);
		info_ids.push_back(global_key);

		stream_container& destination_container = this->block.info_containers[target_container];
		const int& info_primitive_type = record->d.info[i].type;
		const int& stride_size         = record->d.info[i].len;
		const uint32_t& data_length    = record->d.info[i].vptr_len;
		const uint8_t* data            = record->d.info[i].vptr;
		int element_stride_size        = 0;

		this->stats_info[global_key].cost_bcf += data_length;

		if(info_primitive_type == BCF_BT_INT8){
			element_stride_size = sizeof(int8_t);
			assert(data_length % element_stride_size == 0);
			const int8_t* data_local = reinterpret_cast<const int8_t*>(data);
			for(uint32_t j = 0; j < data_length/element_stride_size; ++j)
				destination_container.Add(data_local[j]);
		} else if(info_primitive_type == BCF_BT_INT16){
			element_stride_size = sizeof(int16_t);
			assert(data_length % element_stride_size == 0);
			const int16_t* data_local = reinterpret_cast<const int16_t*>(data);
			for(uint32_t j = 0; j < data_length/element_stride_size; ++j)
				destination_container.Add(data_local[j]);
		} else if(info_primitive_type == BCF_BT_INT32){
			element_stride_size = sizeof(int32_t);
			assert(data_length % element_stride_size == 0);
			const int32_t* data_local = reinterpret_cast<const int32_t*>(data);
			for(uint32_t j = 0; j < data_length/element_stride_size; ++j)
				destination_container.Add(data_local[j]);
		} else if(info_primitive_type == BCF_BT_FLOAT){
			element_stride_size = sizeof(float);
			assert(data_length % element_stride_size == 0);
			const float* data_local = reinterpret_cast<const float*>(data);
			for(uint32_t j = 0; j < data_length/element_stride_size; ++j)
				destination_container.Add(data_local[j]);
		} else if(info_primitive_type == BCF_BT_CHAR){
			element_stride_size = sizeof(char);
			const char* data_local = reinterpret_cast<const char*>(data);
			destination_container.AddCharacter(data_local, data_length);
		} else if(info_primitive_type == BCF_BT_NULL){
			element_stride_size = 0;
			assert(data_length == 0 && stride_size == 0);
		} else {
			std::cerr << utility::timestamp("ERROR","VCF") << "Unknown case: " << (int)info_primitive_type << std::endl;
			exit(1);
		}

		++destination_container;
		destination_container.AddStride(stride_size);
	}

	return(this->AddVcfInfoPattern(info_ids, meta));
}

bool VcfImporterSlave::AddVcfFormatInfo(const bcf1_t* record, meta_type& meta){
	std::vector<int> format_ids;
	const int n_format_fields = record->n_fmt;

	// Iterate over available Format fields.
	for(uint32_t i = 0; i < n_format_fields; ++i){
		const int& hts_format_key = record->d.fmt[i].id;; // htslib IDX value
		const uint32_t global_key = this->format_reorder_map_[hts_format_key]; // tachyon global IDX value
		const uint32_t target_container = this->block.AddFormat(global_key);
		assert(target_container < 65536);
		format_ids.push_back(global_key);

		// Genotypes are a special case and are treated completely differently.
		// Because of this we simply skip that data here if it is available.
		if(this->vcf_header_->GetFormat(hts_format_key)->id == "GT"){
			continue;
		}

		stream_container& destination_container = this->block.format_containers[target_container];

		const int& format_primitive_type = record->d.fmt[i].type;
		const int& stride_size           = record->d.fmt[i].n;
		const uint32_t& data_length      = record->d.fmt[i].p_len;
		const uint8_t* data              = record->d.fmt[i].p;
		int element_stride_size          = 0;

		this->stats_format[global_key].cost_bcf += data_length;

		if(format_primitive_type == BCF_BT_INT8){
			element_stride_size = sizeof(int8_t);
			assert(data_length % element_stride_size == 0);
			const int8_t* data_local = reinterpret_cast<const int8_t*>(data);
			for(uint32_t j = 0; j < data_length/element_stride_size; ++j)
				destination_container.Add(data_local[j]);
			assert(stride_size * element_stride_size * this->vcf_header_->GetNumberSamples() == data_length);
		} else if(format_primitive_type == BCF_BT_INT16){
			element_stride_size = sizeof(int16_t);
			assert(data_length % element_stride_size == 0);
			const int16_t* data_local = reinterpret_cast<const int16_t*>(data);
			for(uint32_t j = 0; j < data_length/element_stride_size; ++j)
				destination_container.Add(data_local[j]);
			assert(stride_size * element_stride_size * this->vcf_header_->GetNumberSamples() == data_length);
		} else if(format_primitive_type == BCF_BT_INT32){
			element_stride_size = sizeof(int32_t);
			assert(data_length % element_stride_size == 0);
			const int32_t* data_local = reinterpret_cast<const int32_t*>(data);
			for(uint32_t j = 0; j < data_length/element_stride_size; ++j)
				destination_container.Add(data_local[j]);
			assert(stride_size * element_stride_size * this->vcf_header_->GetNumberSamples() == data_length);
		} else if(format_primitive_type == BCF_BT_FLOAT){
			element_stride_size = sizeof(float);
			assert(data_length % element_stride_size == 0);
			const float* data_local = reinterpret_cast<const float*>(data);
			for(uint32_t j = 0; j < data_length/element_stride_size; ++j)
				destination_container.Add(data_local[j]);
			assert(stride_size * element_stride_size * this->vcf_header_->GetNumberSamples() == data_length);
		} else if(format_primitive_type == BCF_BT_CHAR){
			element_stride_size = sizeof(char);
			const char* data_local = reinterpret_cast<const char*>(data);
			destination_container.AddCharacter(data_local, data_length);
		} else {
			std::cerr << utility::timestamp("ERROR","VCF") << "Unknown case: " << (int)format_primitive_type << std::endl;
			exit(1);
		}

		++destination_container;
		destination_container.AddStride(stride_size);
	}

	return(this->AddVcfFormatPattern(format_ids, meta));
}

bool VcfImporterSlave::AddVcfInfoPattern(const std::vector<int>& pattern, meta_type& meta){
	if(pattern.size())
		meta.info_pattern_id = this->block.AddInfoPattern(pattern);

	return true;
}

bool VcfImporterSlave::AddVcfFormatPattern(const std::vector<int>& pattern, meta_type& meta){
	if(pattern.size())
		meta.format_pattern_id = this->block.AddFormatPattern(pattern);

	return true;
}

bool VcfImporterSlave::AddVcfFilterPattern(const std::vector<int>& pattern, meta_type& meta){
	if(pattern.size())
		meta.filter_pattern_id = this->block.AddFilterPattern(pattern);

	return true;
}

bool VcfImporterSlave::IndexRecord(const bcf1_t* record, const meta_type& meta){
	int32_t index_bin = -1;

	// Ascertain that the meta entry has been evaluated
	// prior to executing this function.
	if(meta.n_alleles == 0){
		std::cerr << utility::timestamp("ERROR","IMPORT") << "The target meta record must be parsed prior to executing indexing functions..." << std::endl;
		return false;
	}

	int64_t end_position_used = record->pos;

	// The Info field END is used as the end position of an internal if it is available. This field
	// is usually only set for non-standard variants such as SVs or other special meaning records.
	if(this->settings_.info_end_key != -1){
		// Linear search for the END key: this is not optimal but is probably faster
		// than first constructing a hash table for each record.
		const int n_info_fields = record->n_info;
		for(uint32_t i = 0; i < n_info_fields; ++i){
			if(record->d.info[i].key == this->settings_.info_end_key){
				uint32_t end = 0;
				switch(record->d.info[i].type){
				case(BCF_BT_INT8):  end = *reinterpret_cast<int8_t*> (record->d.info[i].vptr); break;
				case(BCF_BT_INT16): end = *reinterpret_cast<int16_t*>(record->d.info[i].vptr); break;
				case(BCF_BT_INT32): end = *reinterpret_cast<int32_t*>(record->d.info[i].vptr); break;
				default:
					std::cerr << utility::timestamp("ERROR","INDEX") << "Illegal END primitive type: " << io::BCF_TYPE_LOOKUP[record->d.info[i].type] << std::endl;
					return false;
				}
				//std::cerr << "Found END at " << i << ".  END=" << end << " POS=" << record->pos + 1 << std::endl;
				index_bin = this->index.index_[meta.contigID].Add(record->pos, end, this->block_id);
				//index_bin = 0;
				break;
			}
		}
	}

	// If the END field cannot be found then we check if the variant is a
	if(index_bin == -1){
		int32_t longest = -1;
		// Iterate over available allele information and find the longest
		// SNV/indel length. The regex pattern ^[ATGC]{1,}$ searches for
		// simple SNV/indels.
		for(uint32_t i = 0; i < meta.n_alleles; ++i){
			if(std::regex_match(meta.alleles[i].allele, utility::YON_VARIANT_STANDARD)){
				if(meta.alleles[i].l_allele > longest)
					longest = meta.alleles[i].l_allele;
			}
		}

		// Update the variant index with the target bin(s) found.
		if(longest > 1){
			index_bin = this->index.index_[meta.contigID].Add(record->pos,
			                                                  record->pos + longest,
															  this->block_id);
			//index_bin = 0;
			end_position_used = record->pos + longest;
		}
		// In the cases of special-meaning alleles such as copy-number (e.g. <CN>)
		// or SV (e.g. A[B)) they are index according to their left-most value only.
		// This has the implication that they cannot be found by means of interval
		// intersection searches. If special-meaning variants were to be supported
		// in the index then many more blocks would have to be searched for each
		// query as the few special cases will dominate the many general cases. For
		// this reason special-meaning alleles are not completely indexed.
		else {
			index_bin = this->index.index_[meta.contigID].Add(record->pos,
			                                                  record->pos,
			                                                  this->block_id);
			//index_bin = 0;
			//std::cerr << "fallback: " << record->pos+1 << std::endl;
		}

		//std::cerr << "End pos used: " << end_position_used << std::endl;
	}
	if(index_bin > this->index_entry.maxBin) this->index_entry.maxBin = index_bin;
	if(index_bin < this->index_entry.minBin) this->index_entry.minBin = index_bin;
	if(end_position_used > this->index_entry.maxPosition)
		this->index_entry.maxPosition = end_position_used;

	// Update number of entries in block
	++this->index_entry.n_variants;

	return true;
}

/*
bool VcfImporterSlave::GenerateIdentifiers(void){
	uint8_t RANDOM_BYTES[32];
	for(uint32_t i = 0; i < this->vcf_container_.sizeWithoutCarryOver(); ++i){
		uint64_t b_hash;
		while(true){
			RAND_bytes(&RANDOM_BYTES[0], 32);
			b_hash = XXH64(&RANDOM_BYTES[0], 32, 1337);
			hash_map_type::const_iterator it = this->block_hash_map.find(b_hash);
			if(it == this->block_hash_map.end()){
				this->block_hash_map[b_hash] = 0; // Number doesn't matter.
				break;
			}
		}
	}
	return true;
}
*/

}
