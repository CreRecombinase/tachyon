#include "variant_container.h"
#include "algorithm/compression/genotype_encoder.h"
#include "algorithm/permutation/genotype_sorter.h"
#include "algorithm/compression/compression_manager.h"

#include "third_party/xxhash/xxhash.h"

namespace tachyon {

void yon1_vc_t::reserve(const uint32_t new_size) {
	if (new_size < this->n_capacity_) {
		if (new_size < this->n_variants_) {
			// Invoke dtor on trailing elements.
			for (int i = new_size; i < this->n_variants_; ++i)
				((this->variants_ + i)->~yon1_vnt_t)();

			// Resize.
			this->n_variants_ = new_size;
			return;
		}
		return;
	}

	yon1_vnt_t* temp = this->variants_;
	this->variants_ = static_cast<yon1_vnt_t*>(::operator new[](new_size*sizeof(yon1_vnt_t)));

	for (uint32_t i = 0; i < this->n_variants_; ++i) {
		new( &this->variants_[i] ) yon1_vnt_t( std::move(temp[i]) );
		((temp + i)->~yon1_vnt_t)(); // invoke dtor.
	}

	// Clear previous data.
	::operator delete[](static_cast<void*>(temp));

	this->n_capacity_ = new_size;
}

bool yon1_vc_t::Build(yon1_vb_t& variant_block, const yon_vnt_hdr_t& header) {
	// Interlace meta streams into the variant records.
	this->AddController(variant_block.base_containers[YON_BLK_CONTROLLER]);
	this->AddContigs(variant_block.base_containers[YON_BLK_CONTIG]);
	this->AddPositions(variant_block.base_containers[YON_BLK_POSITION]);
	this->AddQuality(variant_block.base_containers[YON_BLK_QUALITY]);
	this->AddFilterIds(variant_block.base_containers[YON_BLK_ID_FILTER]);
	this->AddFormatIds(variant_block.base_containers[YON_BLK_ID_FORMAT]);
	this->AddInfoIds(variant_block.base_containers[YON_BLK_ID_INFO]);
	this->AddPloidy(variant_block.base_containers[YON_BLK_GT_PLOIDY]);
	this->AddAlleles(variant_block.base_containers[YON_BLK_ALLELES]);
	this->AddNames(variant_block.base_containers[YON_BLK_NAMES]);
	this->AddRefAlt(variant_block.base_containers[YON_BLK_REFALT]);

	// Allocate memory to support upcoming data to overload into the
	// containers. After memory allocation we provide the list of local
	// offsets that provide data.
	for (uint32_t i = 0; i < this->n_variants_; ++i) {
		this->variants_[i].info   = new PrimitiveContainerInterface*[variant_block.footer.n_info_streams];
		this->variants_[i].m_info = variant_block.footer.n_info_streams;
		this->variants_[i].fmt    = new PrimitiveGroupContainerInterface*[variant_block.footer.n_format_streams];
		this->variants_[i].m_fmt  = variant_block.footer.n_format_streams;
	}

	this->AddFilter(variant_block, header);
	this->AddInfo(variant_block, header);
	this->AddFormat(variant_block, header);
	this->PermuteOrder(variant_block);

	return true;
}

bool yon1_vc_t::PermuteOrder(const yon1_vb_t& variant_block) {
	// Map local in block to local in variant.
	// Data is added to variants in the order they appear in the raw data block.
	// This is not necessarily corect as data may have been intended to be read in
	// a different order. We have to map from these global identifiers to the per-variant
	// local offsets by permuting the global to local order.
	std::vector< std::vector<int> > local_info_patterns(variant_block.load_settings->info_patterns_local.size());
	for (int i = 0; i < local_info_patterns.size(); ++i) {
		std::vector<std::pair<int,int>> internal(variant_block.load_settings->info_patterns_local[i].size());
		for (int j = 0 ; j < internal.size(); ++j) {
			internal[j] = std::pair<int,int>((*variant_block.footer.info_map)[variant_block.load_settings->info_patterns_local[i][j]], j);
		}
		std::sort(internal.begin(), internal.end());

		local_info_patterns[i].resize(internal.size());
		for (int j = 0; j < internal.size(); ++j) {
			local_info_patterns[i][internal[j].second] = j;
			//std::cerr << "local info now: " << internal[j].second << "=" << j << std::endl;
		}
	}

	std::vector< std::vector<int> > local_format_patterns(variant_block.load_settings->format_patterns_local.size());
	for (int i = 0; i < local_format_patterns.size(); ++i) {
		std::vector<std::pair<int,int>> internal(variant_block.load_settings->format_patterns_local[i].size());
		for (int j = 0 ; j < internal.size(); ++j) {
			internal[j] = std::pair<int,int>((*variant_block.footer.format_map)[variant_block.load_settings->format_patterns_local[i][j]], j);
		}
		std::sort(internal.begin(), internal.end());

		local_format_patterns[i].resize(internal.size());
		for (int j = 0; j < internal.size(); ++j) {
			local_format_patterns[i][internal[j].second] = j;
			//std::cerr << "local format now: " << internal[j].second << "=" << j << std::endl;
		}
	}

	// Permute Info and Format fields back into original ordering
	// NOT the order they were loaded in (FILO-stack order).
	for (uint32_t i = 0; i < this->n_variants_; ++i) {
		if (this->variants_[i].info_pid >= 0) {
			//std::cerr << "info pid=" << this->variants_[i].info_pid << " : " << this->variants_[i].n_info << std::endl;
			PrimitiveContainerInterface** old = this->variants_[i].info;
			std::vector<const YonInfo*> old_hdr = this->variants_[i].info_hdr;

			this->variants_[i].info = new PrimitiveContainerInterface*[this->variants_[i].m_info];
			for (int k = 0; k < this->variants_[i].n_info; ++k) {
				//std::cerr << "info " << k << "->" << local_info_patterns[this->variants_[i].info_pid][k] << std::endl;
				this->variants_[i].info[k] = old[local_info_patterns[this->variants_[i].info_pid][k]];
				this->variants_[i].info_hdr[k] = old_hdr[local_info_patterns[this->variants_[i].info_pid][k]];
				this->variants_[i].info_map[this->variants_[i].info_hdr[k]->id] = k;
			}
			delete [] old;
		}

		if (this->variants_[i].fmt_pid >= 0) {
			PrimitiveGroupContainerInterface** old = this->variants_[i].fmt;
			std::vector<const YonFormat*> old_hdr = this->variants_[i].fmt_hdr;

			this->variants_[i].fmt = new PrimitiveGroupContainerInterface*[this->variants_[i].m_fmt];
			for (int k = 0; k < this->variants_[i].n_fmt; ++k) {
				this->variants_[i].fmt[k] = old[local_format_patterns[this->variants_[i].fmt_pid][k]];
				this->variants_[i].fmt_hdr[k] = old_hdr[local_format_patterns[this->variants_[i].fmt_pid][k]];
				this->variants_[i].fmt_map[this->variants_[i].fmt_hdr[k]->id] = k;
			}
			delete [] old;
		}
	}

	return true;
}

bool yon1_vc_t::AddInfo(yon1_vb_t& variant_block, const yon_vnt_hdr_t& header) {
	for (int i = 0; i < variant_block.load_settings->info_id_local_loaded.size(); ++i) {
		// Evaluate the set-membership of a given global key in the available Info patterns
		// described in the data container footer.
		std::vector<bool> matches = variant_block.InfoPatternSetMembership(variant_block.info_containers[variant_block.load_settings->info_id_local_loaded[i]].header.GetGlobalKey());
		// Add Info data.
		//std::cerr << "Adding info: " << variant_block.load_settings->info_id_local_loaded[i] << "->" << variant_block.info_containers[variant_block.load_settings->info_id_local_loaded[i]].GetIdx() << "->" << header.info_fields_[variant_block.info_containers[variant_block.load_settings->info_id_local_loaded[i]].GetIdx()].id << std::endl;
		this->AddInfoWrapper(variant_block.info_containers[variant_block.load_settings->info_id_local_loaded[i]], header, matches);
	}
	return true;
}

bool yon1_vc_t::AddFilter(yon1_vb_t& variant_block, const yon_vnt_hdr_t& header) {
	for (uint32_t i = 0; i < this->n_variants_; ++i) {
		if (this->variants_[i].flt_pid >= 0) {
			const yon_blk_bv_pair& filter_patterns = variant_block.footer.filter_patterns[this->variants_[i].flt_pid];
			for (int j = 0; j < filter_patterns.pattern.size(); ++j) {
				this->variants_[i].flt_hdr.push_back(&header.filter_fields_[filter_patterns.pattern[j]]);
				++this->variants_[i].n_flt;
			}
		}
	}
	return true;
}

bool yon1_vc_t::AddFormat(yon1_vb_t& variant_block, const yon_vnt_hdr_t& header) {
	if (variant_block.load_settings->format_id_local_loaded.size() == 0 && variant_block.load_settings->loaded_genotypes) {
		//std::cerr << "adding genotypes when no other fmt is loaded" << std::endl;
		// Add genotypes.
		this->AddGenotypes(variant_block, header);

		for (int j = 0; j < this->n_variants_; ++j) {
			if (this->variants_[j].controller.gt_available) {
				// Format:Gt field has to appear first in order. This is
				// a restriction imposed by htslib bcf.
				assert(this->variants_[j].fmt_hdr.size() == 0);
				this->variants_[j].is_loaded_gt = true;
				// Do not store header pointer and allocate a new format container
				// because this data is hidden.

				this->variants_[j].gt->Evaluate();
				//records[i].gt->Expand();
			}
		}
	}

	for (int i = 0; i < variant_block.load_settings->format_id_local_loaded.size(); ++i) {
		// If genotype data has been loaded.
		yon1_dc_t& dc = variant_block.format_containers[variant_block.load_settings->format_id_local_loaded[i]];
		if (header.format_fields_[dc.GetGlobalKey()].id == "GT") {
			//std::cerr << "adding format" << std::endl;
			assert(variant_block.load_settings->loaded_genotypes);
			std::vector<bool> matches = variant_block.FormatPatternSetMembership(dc.GetGlobalKey());

			// Add genotypes.
			this->AddGenotypes(variant_block, header);

			for (int j = 0; j < this->n_variants_; ++j) {
				if (this->variants_[j].fmt_pid == -1) {
					continue;
				} else if (matches[this->variants_[j].fmt_pid]) {
					// Format:Gt field has to appear first in order. This is
					// a restriction imposed by htslib bcf.
					assert(this->variants_[j].fmt_hdr.size() == 0);
					this->variants_[j].is_loaded_gt = true;
					this->variants_[j].fmt[this->variants_[j].n_fmt++] = new PrimitiveGroupContainer<int32_t>();
					this->variants_[j].fmt_hdr.push_back(&header.format_fields_[dc.GetGlobalKey()]);

					// Lazy-evaluate minimum genotypes.
					this->variants_[j].gt->Evaluate();
					//records[i].gt->Expand();
				}
			}
			continue;
		}

		// Evaluate the set-membership of a given global key in the available Format patterns
		// described in the data container footer.
		std::vector<bool> matches = variant_block.FormatPatternSetMembership(dc.GetGlobalKey());
		// Add Format data.
		//std::cerr << "Adding format: " << variant_block.load_settings->format_id_local_loaded[i] << "->" << dc.GetIdx() << "->" << header.format_fields_[dc.GetIdx()].id << std::endl;
		this->AddFormatWrapper(dc, header, matches);
	}
	return true;
}

bool yon1_vc_t::AddInfoWrapper(dc_type& container,
                                      const yon_vnt_hdr_t& header,
                                      const std::vector<bool>& matches)
{
	if ((container.data_uncompressed.size() == 0 &&
	   header.info_fields_[container.header.GetGlobalKey()].yon_type == YON_VCF_HEADER_FLAG) || container.header.data_header.GetPrimitiveType() == YON_TYPE_BOOLEAN)
	{
		for (uint32_t i = 0; i < this->n_variants_; ++i) {
			if (this->variants_[i].info_pid == -1) {
				continue;
			} else if (matches[this->variants_[i].info_pid]) {
				this->variants_[i].info[this->variants_[i].n_info++] = new PrimitiveContainer<int32_t>(0);
				this->variants_[i].info_hdr.push_back(&header.info_fields_[container.header.GetGlobalKey()]);
			}
		}
		return false;
	}

	if (container.data_uncompressed.size() == 0) {
		//std::cerr << "info no data return" << std::endl;
		return false;
	}

	if (container.header.data_header.HasMixedStride()) {
		if (container.header.data_header.IsSigned()) {
			switch(container.header.data_header.GetPrimitiveType()) {
			case(YON_TYPE_8B):     (this->InfoSetup<int32_t, int8_t>(container, header,  matches));  break;
			case(YON_TYPE_16B):    (this->InfoSetup<int32_t, int16_t>(container, header,  matches));    break;
			case(YON_TYPE_32B):    (this->InfoSetup<int32_t, int32_t>(container, header,  matches));    break;
			//case(YON_TYPE_64B):    (this->InfoSetup<int64_t>(container, header,  matches));    break;
			case(YON_TYPE_FLOAT):  (this->InfoSetup<float, float>(container, header,  matches));  break;
			case(YON_TYPE_DOUBLE): (this->InfoSetup<double, double>(container, header,  matches)); break;
			case(YON_TYPE_CHAR):   (this->InfoSetupString(container, header,  matches)); break;
			case(YON_TYPE_BOOLEAN):
			case(YON_TYPE_STRUCT):
			case(YON_TYPE_UNKNOWN):
			default: std::cerr << "Disallowed type: " << (int)container.header.data_header.controller.type << std::endl; return false;
			}
		} else {
			switch(container.header.data_header.GetPrimitiveType()) {
			case(YON_TYPE_8B):     (this->InfoSetup<int32_t, uint8_t>(container, header,  matches));   break;
			case(YON_TYPE_16B):    (this->InfoSetup<int32_t, uint16_t>(container, header,  matches));    break;
			case(YON_TYPE_32B):    (this->InfoSetup<int32_t, uint32_t>(container, header,  matches));    break;
			//case(YON_TYPE_64B):    (this->InfoSetup<uint64_t>(container, header,  matches));    break;
			case(YON_TYPE_FLOAT):  (this->InfoSetup<float, float>(container, header,  matches));  break;
			case(YON_TYPE_DOUBLE): (this->InfoSetup<double, double>(container, header,  matches)); break;
			case(YON_TYPE_CHAR):   (this->InfoSetupString(container, header,  matches)); break;
			case(YON_TYPE_BOOLEAN):
			case(YON_TYPE_STRUCT):
			case(YON_TYPE_UNKNOWN):
			default: std::cerr << "Disallowed type: " << (int)container.header.data_header.controller.type << std::endl; return false;
			}
		}
	} else {
		if (container.header.data_header.IsSigned()) {
			switch(container.header.data_header.GetPrimitiveType()) {
			case(YON_TYPE_8B):     (this->InfoSetup<int32_t, int8_t>(container, header,  matches, container.header.data_header.stride));  break;
			case(YON_TYPE_16B):    (this->InfoSetup<int32_t, int16_t>(container, header,  matches, container.header.data_header.stride));    break;
			case(YON_TYPE_32B):    (this->InfoSetup<int32_t, int32_t>(container, header,  matches, container.header.data_header.stride));    break;
			//case(YON_TYPE_64B):    (this->InfoSetup<int64_t>(container, header,  matches, container.header.data_header.stride));    break;
			case(YON_TYPE_FLOAT):  (this->InfoSetup<float, float>(container, header,  matches, container.header.data_header.stride));  break;
			case(YON_TYPE_DOUBLE): (this->InfoSetup<double, double>(container, header,  matches, container.header.data_header.stride)); break;
			case(YON_TYPE_CHAR):   (this->InfoSetupString(container, header,  matches, container.header.data_header.stride)); break;
			case(YON_TYPE_BOOLEAN):
			case(YON_TYPE_STRUCT):
			case(YON_TYPE_UNKNOWN):
			default: std::cerr << "Disallowed type: " << (int)container.header.data_header.controller.type << std::endl; return false;
			}
		} else {
			switch(container.header.data_header.GetPrimitiveType()) {
			case(YON_TYPE_8B):     (this->InfoSetup<int32_t, uint8_t>(container, header,  matches, container.header.data_header.stride));   break;
			case(YON_TYPE_16B):    (this->InfoSetup<int32_t, uint16_t>(container, header,  matches, container.header.data_header.stride));    break;
			case(YON_TYPE_32B):    (this->InfoSetup<int32_t, uint32_t>(container, header,  matches, container.header.data_header.stride));    break;
			//case(YON_TYPE_64B):    (this->InfoSetup<uint64_t>(container, header,  matches, container.header.data_header.stride));    break;
			case(YON_TYPE_FLOAT):  (this->InfoSetup<float, float>(container, header,  matches, container.header.data_header.stride));  break;
			case(YON_TYPE_DOUBLE): (this->InfoSetup<double, double>(container, header,  matches, container.header.data_header.stride)); break;
			case(YON_TYPE_CHAR):   (this->InfoSetupString(container, header,  matches, container.header.data_header.stride)); break;
			case(YON_TYPE_BOOLEAN):
			case(YON_TYPE_STRUCT):
			case(YON_TYPE_UNKNOWN):
			default: std::cerr << "Disallowed type: " << (int)container.header.data_header.controller.type << std::endl; return false;

			}
		}
	}
	return true;
}

bool yon1_vc_t::InfoSetupString(dc_type& container,
                                       const yon_vnt_hdr_t& header,
                                       const std::vector<bool>& matches)
{
	if (container.strides_uncompressed.size() == 0)
		return false;

	yon_cont_ref_iface* it = nullptr;
	switch(container.header.stride_header.controller.type) {
	case(YON_TYPE_8B):  it = new yon_cont_ref<uint8_t>(container.strides_uncompressed.data(), container.strides_uncompressed.size()); break;
	case(YON_TYPE_16B): it = new yon_cont_ref<uint16_t>(container.strides_uncompressed.data(), container.strides_uncompressed.size()); break;
	case(YON_TYPE_32B): it = new yon_cont_ref<uint32_t>(container.strides_uncompressed.data(), container.strides_uncompressed.size()); break;
	case(YON_TYPE_64B): it = new yon_cont_ref<uint64_t>(container.strides_uncompressed.data(), container.strides_uncompressed.size()); break;
	}
	assert(it != nullptr);

	uint32_t current_offset = 0;
	uint32_t stride_offset = 0;

	for (uint32_t i = 0; i < this->n_variants_; ++i) {
		if (this->variants_[i].info_pid == -1) {
			continue;
		} else if (matches[this->variants_[i].info_pid]) {
			this->variants_[i].info[this->variants_[i].n_info++] = new PrimitiveContainer<std::string>(&container.data_uncompressed[current_offset], it->GetInt32(stride_offset));
			this->variants_[i].info_hdr.push_back(&header.info_fields_[container.header.GetGlobalKey()]);
			current_offset += it->GetInt32(stride_offset) * sizeof(char);
			++stride_offset;
		}
	}
	assert(current_offset == container.data_uncompressed.size());
	delete it;
	return true;
}

bool yon1_vc_t::InfoSetupString(dc_type& container,
                                       const yon_vnt_hdr_t& header,
                                       const std::vector<bool>& matches,
                                       const uint32_t stride)
{
	if (container.header.data_header.IsUniform()) {
		for (uint32_t i = 0; i < this->n_variants_; ++i) {
			if (this->variants_[i].info_pid == -1) {
				continue;
			} else if (matches[this->variants_[i].info_pid]) {
				this->variants_[i].info[this->variants_[i].n_info++] = new PrimitiveContainer<std::string>(&container.data_uncompressed[0], stride);
				this->variants_[i].info_hdr.push_back(&header.info_fields_[container.header.GetGlobalKey()]);
			}
		}
	} else {
		uint32_t current_offset = 0;
		for (uint32_t i = 0; i < this->n_variants_; ++i) {
			if (this->variants_[i].info_pid == -1) {
				continue;
			} else if (matches[this->variants_[i].info_pid]) {
				this->variants_[i].info[this->variants_[i].n_info++] = new PrimitiveContainer<std::string>(&container.data_uncompressed[current_offset], stride);
				this->variants_[i].info_hdr.push_back(&header.info_fields_[container.header.GetGlobalKey()]);
				current_offset += stride;
			}
		}
		assert(current_offset == container.data_uncompressed.size());
	}
	return true;
}

bool yon1_vc_t::AddFormatWrapper(dc_type& container, const yon_vnt_hdr_t& header, const std::vector<bool>& matches) {
	if (container.data_uncompressed.size() == 0) {
		return false;
	}

	if (container.header.data_header.HasMixedStride()) {
		if (container.header.data_header.IsSigned()) {
			switch(container.header.data_header.GetPrimitiveType()) {
			case(YON_TYPE_8B):     (this->FormatSetup<int32_t, int8_t>(container, header,  matches));  break;
			case(YON_TYPE_16B):    (this->FormatSetup<int32_t, int16_t>(container, header,  matches));    break;
			case(YON_TYPE_32B):    (this->FormatSetup<int32_t, int32_t>(container, header,  matches));    break;
			//case(YON_TYPE_64B):    (this->FormatSetup<int64_t>(container, header,  matches));    break;
			case(YON_TYPE_FLOAT):  (this->FormatSetup<float, float>(container, header,  matches));  break;
			case(YON_TYPE_DOUBLE): (this->FormatSetup<double, double>(container, header,  matches)); break;
			case(YON_TYPE_CHAR):   (this->FormatSetupString(container, header,  matches)); break;
			case(YON_TYPE_BOOLEAN):
			case(YON_TYPE_STRUCT):
			case(YON_TYPE_UNKNOWN):
			default: std::cerr << "Disallowed type in fmt: " << (int)container.header.data_header.controller.type << std::endl; return false;
			}
		} else {
			switch(container.header.data_header.GetPrimitiveType()) {
			case(YON_TYPE_8B):     (this->FormatSetup<int32_t, uint8_t>(container, header,  matches));   break;
			case(YON_TYPE_16B):    (this->FormatSetup<int32_t, uint16_t>(container, header,  matches));    break;
			case(YON_TYPE_32B):    (this->FormatSetup<int32_t, uint32_t>(container, header,  matches));    break;
			//case(YON_TYPE_64B):    (this->FormatSetup<uint64_t>(container, header,  matches));    break;
			case(YON_TYPE_FLOAT):  (this->FormatSetup<float, float>(container, header,  matches));  break;
			case(YON_TYPE_DOUBLE): (this->FormatSetup<double, double>(container, header,  matches)); break;
			case(YON_TYPE_CHAR):   (this->FormatSetupString(container, header,  matches)); break;
			case(YON_TYPE_BOOLEAN):
			case(YON_TYPE_STRUCT):
			case(YON_TYPE_UNKNOWN):
			default: std::cerr << "Disallowed type in fmt: " << (int)container.header.data_header.controller.type << std::endl; return false;
			}
		}
	} else {
		if (container.header.data_header.IsSigned()) {
			switch(container.header.data_header.GetPrimitiveType()) {
			case(YON_TYPE_8B):     (this->FormatSetup<int32_t, int8_t>(container, header,  matches, container.header.data_header.stride));  break;
			case(YON_TYPE_16B):    (this->FormatSetup<int32_t, int16_t>(container, header,  matches, container.header.data_header.stride));    break;
			case(YON_TYPE_32B):    (this->FormatSetup<int32_t, int32_t>(container, header,  matches, container.header.data_header.stride));    break;
			//case(YON_TYPE_64B):    (this->FormatSetup<int64_t>(container, header,  matches, container.header.data_header.stride));    break;
			case(YON_TYPE_FLOAT):  (this->FormatSetup<float, float>(container, header,  matches, container.header.data_header.stride));  break;
			case(YON_TYPE_DOUBLE): (this->FormatSetup<double, double>(container, header,  matches, container.header.data_header.stride)); break;
			case(YON_TYPE_CHAR):   (this->FormatSetupString(container, header,  matches, container.header.data_header.stride)); break;
			case(YON_TYPE_BOOLEAN):
			case(YON_TYPE_STRUCT):
			case(YON_TYPE_UNKNOWN):
			default: std::cerr << "Disallowed type in fmt: " << (int)container.header.data_header.controller.type << std::endl; return false;
			}
		} else {
			switch(container.header.data_header.GetPrimitiveType()) {
			case(YON_TYPE_8B):     (this->FormatSetup<int32_t, uint8_t>(container, header,  matches, container.header.data_header.stride));   break;
			case(YON_TYPE_16B):    (this->FormatSetup<int32_t, uint16_t>(container, header,  matches, container.header.data_header.stride));    break;
			case(YON_TYPE_32B):    (this->FormatSetup<int32_t, uint32_t>(container, header,  matches, container.header.data_header.stride));    break;
			//case(YON_TYPE_64B):    (this->FormatSetup<uint64_t>(container, header,  matches, container.header.data_header.stride));    break;
			case(YON_TYPE_FLOAT):  (this->FormatSetup<float, float>(container, header,  matches, container.header.data_header.stride));  break;
			case(YON_TYPE_DOUBLE): (this->FormatSetup<double, double>(container, header,  matches, container.header.data_header.stride)); break;
			case(YON_TYPE_CHAR):   (this->FormatSetupString(container, header,  matches, container.header.data_header.stride)); break;
			case(YON_TYPE_BOOLEAN):
			case(YON_TYPE_STRUCT):
			case(YON_TYPE_UNKNOWN):
			default: std::cerr << "Disallowed type in fmt: " << (int)container.header.data_header.controller.type << std::endl; return false;

			}
		}
	}
	return true;
}

bool yon1_vc_t::AddGenotypes(yon1_vb_t& block, const yon_vnt_hdr_t& header) {
	const bool uniform_stride = block.base_containers[YON_BLK_GT_SUPPORT].header.data_header.IsUniform();
	PrimitiveContainer<uint32_t> lengths(block.base_containers[YON_BLK_GT_SUPPORT]); // n_runs / objects size

	uint64_t offset_rle8     = 0; const char* const rle8     = block.base_containers[YON_BLK_GT_INT8].data_uncompressed.data();
	uint64_t offset_rle16    = 0; const char* const rle16    = block.base_containers[YON_BLK_GT_INT16].data_uncompressed.data();
	uint64_t offset_rle32    = 0; const char* const rle32    = block.base_containers[YON_BLK_GT_INT32].data_uncompressed.data();
	uint64_t offset_rle64    = 0; const char* const rle64    = block.base_containers[YON_BLK_GT_INT64].data_uncompressed.data();

	uint64_t offset_simple8  = 0; const char* const simple8  = block.base_containers[YON_BLK_GT_S_INT8].data_uncompressed.data();
	uint64_t offset_simple16 = 0; const char* const simple16 = block.base_containers[YON_BLK_GT_S_INT16].data_uncompressed.data();
	uint64_t offset_simple32 = 0; const char* const simple32 = block.base_containers[YON_BLK_GT_S_INT32].data_uncompressed.data();
	uint64_t offset_simple64 = 0; const char* const simple64 = block.base_containers[YON_BLK_GT_S_INT64].data_uncompressed.data();

	uint64_t offset_nploid8  = 0; const char* const nploid8  = block.base_containers[YON_BLK_GT_N_INT8].data_uncompressed.data();
	uint64_t offset_nploid16 = 0; const char* const nploid16 = block.base_containers[YON_BLK_GT_N_INT16].data_uncompressed.data();
	uint64_t offset_nploid32 = 0; const char* const nploid32 = block.base_containers[YON_BLK_GT_N_INT32].data_uncompressed.data();
	uint64_t offset_nploid64 = 0; const char* const nploid64 = block.base_containers[YON_BLK_GT_N_INT64].data_uncompressed.data();

	assert(block.base_containers[YON_BLK_GT_INT8].data_uncompressed.size()    % sizeof(uint8_t) == 0);
	assert(block.base_containers[YON_BLK_GT_INT16].data_uncompressed.size()   % sizeof(uint16_t)  == 0);
	assert(block.base_containers[YON_BLK_GT_INT32].data_uncompressed.size()   % sizeof(uint32_t)  == 0);
	assert(block.base_containers[YON_BLK_GT_INT64].data_uncompressed.size()   % sizeof(uint64_t)  == 0);
	assert(block.base_containers[YON_BLK_GT_S_INT8].data_uncompressed.size()  % sizeof(uint8_t) == 0);
	assert(block.base_containers[YON_BLK_GT_S_INT16].data_uncompressed.size() % sizeof(uint16_t)  == 0);
	assert(block.base_containers[YON_BLK_GT_S_INT32].data_uncompressed.size() % sizeof(uint32_t)  == 0);
	assert(block.base_containers[YON_BLK_GT_S_INT64].data_uncompressed.size() % sizeof(uint64_t)  == 0);

	uint64_t gt_offset = 0;
	uint8_t incrementor = 1;
	if (uniform_stride) incrementor = 0;

	for (uint32_t i = 0; i < this->n_variants_; ++i) {
		if (this->variants_[i].controller.gt_available) {
			// Case run-length encoding diploid and biallelic and no missing
			if (this->variants_[i].controller.gt_compression_type == TACHYON_GT_ENCODING::YON_GT_RLE_DIPLOID_BIALLELIC) {
				if (this->variants_[i].controller.gt_primtive_type == TACHYON_GT_PRIMITIVE_TYPE::YON_GT_BYTE) {
					//this->variants_[i].gt_raw = new GenotypeContainerDiploidRLE<uint8_t>( &rle8[offset_rle8], lengths[gt_offset], this->variants_[i].n_alleles, this->variants_[i].n_base_ploidy, this->variants_[i].controller.ToValue());

					this->variants_[i].gt = GetGenotypeDiploidRLE<uint8_t>(&rle8[offset_rle8], lengths[gt_offset], header.GetNumberSamples(), this->variants_[i].n_alleles, this->variants_[i].n_base_ploidy, this->variants_[i].controller.ToValue(), block.gt_ppa);
					offset_rle8 += lengths[gt_offset]*sizeof(uint8_t);
				} else if (this->variants_[i].controller.gt_primtive_type == TACHYON_GT_PRIMITIVE_TYPE::YON_GT_U16) {
					//this->variants_[i].gt_raw = new GenotypeContainerDiploidRLE<uint16_t>( &rle16[offset_rle16], lengths[gt_offset], this->variants_[i].n_alleles, this->variants_[i].n_base_ploidy, this->variants_[i].controller.ToValue());
					this->variants_[i].gt = GetGenotypeDiploidRLE<uint16_t>(&rle16[offset_rle16], lengths[gt_offset], header.GetNumberSamples(), this->variants_[i].n_alleles, this->variants_[i].n_base_ploidy, this->variants_[i].controller.ToValue(), block.gt_ppa);
					offset_rle16 += lengths[gt_offset]*sizeof(uint16_t);
				} else if (this->variants_[i].controller.gt_primtive_type == TACHYON_GT_PRIMITIVE_TYPE::YON_GT_U32) {
					//this->variants_[i].gt_raw = new GenotypeContainerDiploidRLE<uint32_t>( &rle32[offset_rle32], lengths[gt_offset], this->variants_[i].n_alleles, this->variants_[i].n_base_ploidy, this->variants_[i].controller.ToValue());
					this->variants_[i].gt = GetGenotypeDiploidRLE<uint32_t>(&rle32[offset_rle32], lengths[gt_offset], header.GetNumberSamples(), this->variants_[i].n_alleles, this->variants_[i].n_base_ploidy, this->variants_[i].controller.ToValue(), block.gt_ppa);
					offset_rle32 += lengths[gt_offset]*sizeof(uint32_t);
				} else if (this->variants_[i].controller.gt_primtive_type == TACHYON_GT_PRIMITIVE_TYPE::YON_GT_U64) {
					//this->variants_[i].gt_raw = new GenotypeContainerDiploidRLE<uint64_t>( &rle64[offset_rle64], lengths[gt_offset], this->variants_[i].n_alleles, this->variants_[i].n_base_ploidy, this->variants_[i].controller.ToValue());
					this->variants_[i].gt = GetGenotypeDiploidRLE<uint64_t>(&rle64[offset_rle64], lengths[gt_offset], header.GetNumberSamples(), this->variants_[i].n_alleles, this->variants_[i].n_base_ploidy, this->variants_[i].controller.ToValue(), block.gt_ppa);
					offset_rle64 += lengths[gt_offset]*sizeof(uint64_t);
				} else {
					std::cerr << utility::timestamp("ERROR","GT") << "Unknown GT encoding primitive..." << std::endl;
					exit(1);
				}

			}
			// Case run-length encoding diploid and biallelic/EOV or n-allelic
			else if (this->variants_[i].controller.gt_compression_type == TACHYON_GT_ENCODING::YON_GT_RLE_DIPLOID_NALLELIC) {
				if (this->variants_[i].controller.gt_primtive_type == TACHYON_GT_PRIMITIVE_TYPE::YON_GT_BYTE) {
					//this->variants_[i].gt_raw = new GenotypeContainerDiploidSimple<uint8_t>( &simple8[offset_simple8], lengths[gt_offset], this->variants_[i].n_alleles, this->variants_[i].n_base_ploidy, this->variants_[i].controller.ToValue());
					this->variants_[i].gt = GetGenotypeDiploidSimple<uint8_t>(&simple8[offset_simple8], lengths[gt_offset], header.GetNumberSamples(), this->variants_[i].n_alleles, this->variants_[i].n_base_ploidy, this->variants_[i].controller.ToValue(), block.gt_ppa);
					offset_simple8 += lengths[gt_offset]*sizeof(uint8_t);
				} else if (this->variants_[i].controller.gt_primtive_type == TACHYON_GT_PRIMITIVE_TYPE::YON_GT_U16) {
					//this->variants_[i].gt_raw = new GenotypeContainerDiploidSimple<uint16_t>( &simple16[offset_simple16], lengths[gt_offset], this->variants_[i].n_alleles, this->variants_[i].n_base_ploidy, this->variants_[i].controller.ToValue());
					this->variants_[i].gt = GetGenotypeDiploidSimple<uint16_t>(&simple16[offset_simple16], lengths[gt_offset], header.GetNumberSamples(), this->variants_[i].n_alleles, this->variants_[i].n_base_ploidy, this->variants_[i].controller.ToValue(), block.gt_ppa);
					offset_simple16 += lengths[gt_offset]*sizeof(uint16_t);
				} else if (this->variants_[i].controller.gt_primtive_type == TACHYON_GT_PRIMITIVE_TYPE::YON_GT_U32) {
					//this->variants_[i].gt_raw = new GenotypeContainerDiploidSimple<uint32_t>( &simple32[offset_simple32], lengths[gt_offset], this->variants_[i].n_alleles, this->variants_[i].n_base_ploidy, this->variants_[i].controller.ToValue());
					this->variants_[i].gt = GetGenotypeDiploidSimple<uint32_t>(&simple32[offset_simple32], lengths[gt_offset], header.GetNumberSamples(), this->variants_[i].n_alleles, this->variants_[i].n_base_ploidy, this->variants_[i].controller.ToValue(), block.gt_ppa);
					offset_simple32 += lengths[gt_offset]*sizeof(uint32_t);
				} else if (this->variants_[i].controller.gt_primtive_type == TACHYON_GT_PRIMITIVE_TYPE::YON_GT_U64) {
					//this->variants_[i].gt_raw = new GenotypeContainerDiploidSimple<uint64_t>( &simple64[offset_simple64], lengths[gt_offset], this->variants_[i].n_alleles, this->variants_[i].n_base_ploidy, this->variants_[i].controller.ToValue());
					this->variants_[i].gt = GetGenotypeDiploidSimple<uint64_t>(&simple64[offset_simple64], lengths[gt_offset], header.GetNumberSamples(), this->variants_[i].n_alleles, this->variants_[i].n_base_ploidy, this->variants_[i].controller.ToValue(), block.gt_ppa);
					offset_simple64 += lengths[gt_offset]*sizeof(uint64_t);
				} else {
					std::cerr << utility::timestamp("ERROR","GT") << "Unknown GT encoding primitive..." << std::endl;
					exit(1);
				}
			}
			// Case BCF-style encoding of diploids
			else if (this->variants_[i].controller.gt_compression_type == TACHYON_GT_ENCODING::YON_GT_BCF_DIPLOID) {
				std::cerr << "illegal gt type" << std::endl;
				exit(1);
				/*
				if (this->variants_[i].controller.gt_primtive_type == TACHYON_GT_PRIMITIVE_TYPE::YON_GT_BYTE) {
					this->variants_[i].gt_raw = new GenotypeContainerDiploidBCF<uint8_t>( &simple8[offset_simple8], lengths[gt_offset], this->variants_[i].n_alleles, this->variants_[i].n_base_ploidy, this->variants_[i].controller.ToValue());
					offset_simple8 += lengths[gt_offset]*sizeof(uint8_t);
				} else if (this->variants_[i].controller.gt_primtive_type == TACHYON_GT_PRIMITIVE_TYPE::YON_GT_U16) {
					this->variants_[i].gt_raw = new GenotypeContainerDiploidBCF<uint16_t>( &simple16[offset_simple16], lengths[gt_offset], this->variants_[i].n_alleles, this->variants_[i].n_base_ploidy, this->variants_[i].controller.ToValue());
					offset_simple16 += lengths[gt_offset]*sizeof(uint16_t);
				} else if (this->variants_[i].controller.gt_primtive_type == TACHYON_GT_PRIMITIVE_TYPE::YON_GT_U32) {
					this->variants_[i].gt_raw = new GenotypeContainerDiploidBCF<uint32_t>( &simple32[offset_simple32], lengths[gt_offset], this->variants_[i].n_alleles, this->variants_[i].n_base_ploidy, this->variants_[i].controller.ToValue());
					offset_simple32 += lengths[gt_offset]*sizeof(uint32_t);
				} else if (this->variants_[i].controller.gt_primtive_type == TACHYON_GT_PRIMITIVE_TYPE::YON_GT_U64) {
					this->variants_[i].gt_raw = new GenotypeContainerDiploidBCF<uint64_t>( &simple64[offset_simple64], lengths[gt_offset], this->variants_[i].n_alleles, this->variants_[i].n_base_ploidy, this->variants_[i].controller.ToValue());
					offset_simple64 += lengths[gt_offset]*sizeof(uint64_t);
				}  else {
					std::cerr << utility::timestamp("ERROR","GT") << "Unknown GT encoding primitive..." << std::endl;
					exit(1);
				}
				*/
			}
			// Case RLE-encoding of nploids
			else if (this->variants_[i].controller.gt_compression_type == TACHYON_GT_ENCODING::YON_GT_RLE_NPLOID) {
				if (this->variants_[i].controller.gt_primtive_type == TACHYON_GT_PRIMITIVE_TYPE::YON_GT_BYTE) {
					//this->variants_[i].gt_raw = new GenotypeContainerNploid<uint8_t>( &nploid8[offset_nploid8], lengths[gt_offset], this->variants_[i].n_alleles, this->variants_[i].n_base_ploidy, this->variants_[i].controller.ToValue());
					this->variants_[i].gt = GetGenotypeNploid<uint8_t>(&nploid8[offset_nploid8], lengths[gt_offset], header.GetNumberSamples(), this->variants_[i].n_alleles, this->variants_[i].n_base_ploidy, this->variants_[i].controller.ToValue(), block.gt_ppa);
					offset_nploid8 += lengths[gt_offset]*(sizeof(uint8_t) + this->variants_[i].n_base_ploidy*sizeof(uint8_t));
				} else if (this->variants_[i].controller.gt_primtive_type == TACHYON_GT_PRIMITIVE_TYPE::YON_GT_U16) {
					//this->variants_[i].gt_raw = new GenotypeContainerNploid<uint16_t>( &nploid16[offset_nploid16], lengths[gt_offset], this->variants_[i].n_alleles, this->variants_[i].n_base_ploidy, this->variants_[i].controller.ToValue());
					this->variants_[i].gt = GetGenotypeNploid<uint16_t>(&nploid16[offset_nploid16], lengths[gt_offset], header.GetNumberSamples(), this->variants_[i].n_alleles, this->variants_[i].n_base_ploidy, this->variants_[i].controller.ToValue(), block.gt_ppa);
					offset_nploid16 += lengths[gt_offset]*(sizeof(uint16_t) + this->variants_[i].n_base_ploidy*sizeof(uint8_t));
				} else if (this->variants_[i].controller.gt_primtive_type == TACHYON_GT_PRIMITIVE_TYPE::YON_GT_U32) {
					//this->variants_[i].gt_raw = new GenotypeContainerNploid<uint32_t>( &nploid32[offset_nploid32], lengths[gt_offset], this->variants_[i].n_alleles, this->variants_[i].n_base_ploidy, this->variants_[i].controller.ToValue());
					this->variants_[i].gt = GetGenotypeNploid<uint32_t>(&nploid32[offset_nploid32], lengths[gt_offset], header.GetNumberSamples(), this->variants_[i].n_alleles, this->variants_[i].n_base_ploidy, this->variants_[i].controller.ToValue(), block.gt_ppa);
					offset_nploid32 += lengths[gt_offset]*(sizeof(uint32_t) + this->variants_[i].n_base_ploidy*sizeof(uint8_t));
				} else if (this->variants_[i].controller.gt_primtive_type == TACHYON_GT_PRIMITIVE_TYPE::YON_GT_U64) {
					//this->variants_[i].gt_raw = new GenotypeContainerNploid<uint64_t>( &nploid64[offset_nploid64], lengths[gt_offset], this->variants_[i].n_alleles, this->variants_[i].n_base_ploidy, this->variants_[i].controller.ToValue());
					this->variants_[i].gt = GetGenotypeNploid<uint64_t>(&nploid64[offset_nploid64], lengths[gt_offset], header.GetNumberSamples(), this->variants_[i].n_alleles, this->variants_[i].n_base_ploidy, this->variants_[i].controller.ToValue(), block.gt_ppa);
					offset_nploid64 += lengths[gt_offset]*(sizeof(uint64_t) + this->variants_[i].n_base_ploidy*sizeof(uint8_t));
				}  else {
					std::cerr << utility::timestamp("ERROR","GT") << "Unknown GT encoding primitive..." << std::endl;
					exit(1);
				}
			}
			// Case other potential encodings
			else {
				std::cerr << utility::timestamp("ERROR","GT") << "Unknown GT encoding family..." << std::endl;
				//this->variants_[i].gt_raw = new GenotypeContainerDiploidRLE<uint8_t>( );
				exit(1);
			}

			// Increment offset
			gt_offset += incrementor;

		} else { // No GT available
			this->variants_[i].gt = new yon_gt();
		}
	}

	assert(offset_rle8     == block.base_containers[YON_BLK_GT_INT8].GetSizeUncompressed());
	assert(offset_rle16    == block.base_containers[YON_BLK_GT_INT16].GetSizeUncompressed());
	assert(offset_rle32    == block.base_containers[YON_BLK_GT_INT32].GetSizeUncompressed());
	assert(offset_rle64    == block.base_containers[YON_BLK_GT_INT64].GetSizeUncompressed());
	assert(offset_simple8  == block.base_containers[YON_BLK_GT_S_INT8].GetSizeUncompressed());
	assert(offset_simple16 == block.base_containers[YON_BLK_GT_S_INT16].GetSizeUncompressed());
	assert(offset_simple32 == block.base_containers[YON_BLK_GT_S_INT32].GetSizeUncompressed());
	assert(offset_simple64 == block.base_containers[YON_BLK_GT_S_INT64].GetSizeUncompressed());
	assert(offset_nploid8  == block.base_containers[YON_BLK_GT_N_INT8].GetSizeUncompressed());
	assert(offset_nploid16 == block.base_containers[YON_BLK_GT_N_INT16].GetSizeUncompressed());
	assert(offset_nploid32 == block.base_containers[YON_BLK_GT_N_INT32].GetSizeUncompressed());
	assert(offset_nploid64 == block.base_containers[YON_BLK_GT_N_INT64].GetSizeUncompressed());

	return true;
}

bool yon1_vc_t::FormatSetupString(dc_type& container,
                                         const yon_vnt_hdr_t& header,
                                         const std::vector<bool>& matches)
{
	if (container.strides_uncompressed.size() == 0)
		return false;

	assert(container.header.data_header.IsUniform() == false);

	yon_cont_ref_iface* it = nullptr;
	switch(container.header.stride_header.controller.type) {
	case(YON_TYPE_8B):  it = new yon_cont_ref<uint8_t>(container.strides_uncompressed.data(), container.strides_uncompressed.size()); break;
	case(YON_TYPE_16B): it = new yon_cont_ref<uint16_t>(container.strides_uncompressed.data(), container.strides_uncompressed.size()); break;
	case(YON_TYPE_32B): it = new yon_cont_ref<uint32_t>(container.strides_uncompressed.data(), container.strides_uncompressed.size()); break;
	case(YON_TYPE_64B): it = new yon_cont_ref<uint64_t>(container.strides_uncompressed.data(), container.strides_uncompressed.size()); break;
	}
	assert(it != nullptr);

	uint32_t current_offset = 0;
	uint32_t stride_offset = 0;

	for (uint32_t i = 0; i < this->n_variants_; ++i) {
		if (this->variants_[i].fmt_pid == -1) {

		} else if (matches[this->variants_[i].fmt_pid]) {
			this->variants_[i].fmt[this->variants_[i].n_fmt++] = new PrimitiveGroupContainer<std::string>(container, current_offset, header.GetNumberSamples(), it->GetInt32(stride_offset));
			this->variants_[i].fmt_hdr.push_back(&header.format_fields_[container.header.GetGlobalKey()]);
			current_offset += it->GetInt32(stride_offset) * sizeof(char) * header.GetNumberSamples();
			++stride_offset;
		}
	}
	assert(current_offset == container.data_uncompressed.size());
	delete it;
	return true;
}

bool yon1_vc_t::FormatSetupString(dc_type& container,
                                         const yon_vnt_hdr_t& header,
                                         const std::vector<bool>& matches,
                                         const uint32_t stride)
{
	assert(container.header.data_header.IsUniform() == false);

	uint32_t current_offset = 0;
	for (uint32_t i = 0; i < this->n_variants_; ++i) {
		if (this->variants_[i].fmt_pid == -1) {
			//std::cerr << "pid=" << this->variants_[i].fmt_pid << " matched in fstring: " << current_offset << " + " << stride*header.GetNumberSamples() << std::endl;

		} else if (matches[this->variants_[i].fmt_pid]) {
			this->variants_[i].fmt[this->variants_[i].n_fmt++] = new PrimitiveGroupContainer<std::string>(container, current_offset, header.GetNumberSamples(), stride);
			this->variants_[i].fmt_hdr.push_back(&header.format_fields_[container.header.GetGlobalKey()]);
			current_offset += stride * sizeof(char) * header.GetNumberSamples();
		}
	}
	assert(current_offset == container.data_uncompressed.size());
	return true;
}

bool yon1_vc_t::AddQuality(dc_type& container) {
	if (container.data_uncompressed.size() == 0)
		return false;

	yon_cont_ref<float> it(container.data_uncompressed.data(), container.data_uncompressed.size());

	// If data is uniform.
	if (container.header.data_header.controller.uniform) {
		it.is_uniform_ = true;

		for (uint32_t i = 0; i < this->n_variants_; ++i) {
			this->variants_[i].qual = it[0];
		}
	} else {
		assert(this->n_variants_ == it.n_elements_);
		for (uint32_t i = 0; i < this->n_variants_; ++i) {
			this->variants_[i].qual = it[i];
		}
	}

	return true;
}

bool yon1_vc_t::AddRefAlt(dc_type& container) {
	if (container.data_uncompressed.size() == 0)
		return false;

	uint32_t stride_add = 1;
	yon_cont_ref<uint8_t> it(container.data_uncompressed.data(), container.data_uncompressed.size());
	if (container.header.data_header.IsUniform()) {
		stride_add = 0;
	}

	uint32_t refalt_position = 0;
	for (uint32_t i = 0; i < this->n_variants_; ++i) {
		if (this->variants_[i].controller.alleles_packed) {
			// load from special packed
			// this is always diploid
			this->variants_[i].n_alleles = 2;
			this->variants_[i].alleles   = new yon_allele[2];
			this->variants_[i].m_allele  = 2;

			// If data is <non_ref> or not
			if ((it[refalt_position] & 15) != 5) {
				//assert((refalt[refalt_position] & 15) < 5);
				const char ref = YON_REFALT_LOOKUP[it[refalt_position] & 15];
				this->variants_[i].alleles[1] = ref;

			} else {
				const std::string s = "<NON_REF>";
				this->variants_[i].alleles[1] = s;
			}

			// If data is <non_ref> or not
			if (((it[refalt_position] >> 4) & 15) != 5) {
				//assert(((refalt[refalt_position] >> 4) & 15) < 5);
				const char alt = YON_REFALT_LOOKUP[(it[refalt_position] >> 4) & 15];
				this->variants_[i].alleles[0] = alt;
			} else {
				const std::string s = "<NON_REF>";
				this->variants_[i].alleles[1] = s;
			}
			// Do not increment in case this data is uniform
			if (it.is_uniform_ == false) refalt_position += stride_add;
		}
		// otherwise load from literal cold
		else {
			// number of alleles is parsed from the stride container
		}
	}

	return true;
}

bool yon1_vc_t::AddAlleles(dc_type& container) {
	if (container.data_uncompressed.size() == 0)
		return false;

	if (container.header.data_header.HasMixedStride()) {
		yon_cont_ref_iface* it = nullptr;
		switch(container.header.stride_header.controller.type) {
		case(YON_TYPE_8B):  it = new yon_cont_ref<uint8_t>(container.strides_uncompressed.data(), container.strides_uncompressed.size()); break;
		case(YON_TYPE_16B): it = new yon_cont_ref<uint16_t>(container.strides_uncompressed.data(), container.strides_uncompressed.size()); break;
		case(YON_TYPE_32B): it = new yon_cont_ref<uint32_t>(container.strides_uncompressed.data(), container.strides_uncompressed.size()); break;
		case(YON_TYPE_64B): it = new yon_cont_ref<uint64_t>(container.strides_uncompressed.data(), container.strides_uncompressed.size()); break;
		}
		assert(it != nullptr);

		uint32_t offset = 0;
		uint32_t stride_offset = 0;
		for (uint32_t i = 0; i < this->n_variants_; ++i) {
			if (this->variants_[i].controller.alleles_packed == false) {
				this->variants_[i].n_alleles = it->GetInt32(stride_offset);
				this->variants_[i].alleles   = new yon_allele[it->GetInt32(stride_offset)];
				this->variants_[i].m_allele  = it->GetInt32(stride_offset);

				for (uint32_t j = 0; j < this->variants_[i].n_alleles; ++j) {
					const uint16_t& l_string = *reinterpret_cast<const uint16_t* const>(&container.data_uncompressed[offset]);
					this->variants_[i].alleles[j].ParseFromBuffer(&container.data_uncompressed[offset]);
					offset += sizeof(uint16_t) + l_string;
				}
				++stride_offset;
			}
		}
		assert(offset == container.GetSizeUncompressed());
		delete it;
	} else {
		uint32_t offset = 0;
		const uint32_t stride = container.header.data_header.stride;
		for (uint32_t i = 0; i < this->n_variants_; ++i) {
			if (this->variants_[i].controller.alleles_packed == false) {
				this->variants_[i].n_alleles = stride;
				this->variants_[i].alleles   = new yon_allele[stride];
				this->variants_[i].m_allele  = stride;

				for (uint32_t j = 0; j < this->variants_[i].n_alleles; ++j) {
					const uint16_t& l_string = *reinterpret_cast<const uint16_t* const>(&container.data_uncompressed[offset]);
					this->variants_[i].alleles[j].ParseFromBuffer(&container.data_uncompressed[offset]);
					offset += sizeof(uint16_t) + l_string;
				}
			}
		}
		assert(offset == container.GetSizeUncompressed());
	}
	return true;
}

bool yon1_vc_t::AddNames(dc_type& container) {
	if (container.data_uncompressed.size() == 0)
		return false;

	if (container.header.data_header.HasMixedStride()) {
		yon_cont_ref_iface* it = nullptr;
		switch(container.header.stride_header.controller.type) {
		case(YON_TYPE_8B):  it = new yon_cont_ref<uint8_t>(container.strides_uncompressed.data(), container.strides_uncompressed.size()); break;
		case(YON_TYPE_16B): it = new yon_cont_ref<uint16_t>(container.strides_uncompressed.data(), container.strides_uncompressed.size()); break;
		case(YON_TYPE_32B): it = new yon_cont_ref<uint32_t>(container.strides_uncompressed.data(), container.strides_uncompressed.size()); break;
		case(YON_TYPE_64B): it = new yon_cont_ref<uint64_t>(container.strides_uncompressed.data(), container.strides_uncompressed.size()); break;
		}
		assert(it != nullptr);

		uint32_t offset = 0;
		uint32_t stride_offset = 0;

		assert(it->n_elements_ == this->n_variants_);
		for (uint32_t i = 0; i < this->n_variants_; ++i) {
			this->variants_[i].name = std::string(&container.data_uncompressed.data()[offset], it->GetInt32(i));
			offset += it->GetInt32(i);
		}
		assert(offset == container.data_uncompressed.size());
		delete it;
	} else {
		uint32_t offset = 0;
		const uint32_t stride = container.header.data_header.stride;
		for (uint32_t i = 0; i < this->n_variants_; ++i) {
			this->variants_[i].name = std::string(&container.data_uncompressed.data()[offset], stride);
			offset += stride;
		}
		assert(offset == container.data_uncompressed.size());
	}
	return true;
}

bool yon1_vc_t::PrepareWritableBlock(const uint32_t n_samples, const uint32_t compression_level) {
	// Destroy existing data.
	this->block_.clear();

	algorithm::GenotypeSorter  gts;
	gts.SetSamples(n_samples);
	algorithm::GenotypeEncoder gte(n_samples);

	this->block_.Allocate(20,20,100); // todo: fix
	//this->block_.resize(128000);

	this->block_.header.controller.has_gt = false;
	this->block_.header.controller.has_gt_permuted = false;

	for (uint32_t i = 0; i < this->n_variants_; ++i) {
		if (this->at(i).controller.gt_available) {
			this->block_.header.controller.has_gt = true;
			this->block_.header.controller.has_gt_permuted = true;
			break;
		}
	}

	// If there is data available.
	if (this->block_.header.controller.has_gt) {
		if (this->block_.header.controller.has_gt_permuted) {
			if (gts.Build(this->variants_, this->n_variants_) == false) {
				std::cerr << utility::timestamp("ERROR","PERMUTE") << "Failed to permute genotypes..." << std::endl;
				return false;
			}
		}

		if (gte.Encode(this->variants_, this->n_variants_, this->block_, gts.permutation_array) == false) {
			std::cerr << utility::timestamp("ERROR","ENCODE") << "Failed to encode genotypes..." << std::endl;
			return false;
		}

		if (this->block_.gt_ppa != nullptr) delete this->block_.gt_ppa;
		this->block_.gt_ppa = new yon_gt_ppa(std::move(gts.permutation_array));
	}

	// Add data to the data container.
	for (uint32_t i = 0; i < this->size(); ++i) {
		this->block_.AddMore(this->at(i));
		this->block_ += this->at(i);
	}

	// Update container prior to writing.
	this->block_.UpdateContainers(n_samples);

	// Compress data.
	algorithm::CompressionManager codec_manager;
	// Todo: modify compression level
	codec_manager.Compress(this->block_, compression_level ,n_samples);

	// Finalize block.
	this->block_.Finalize();

	// After all compression and writing is finished the header
	// offsets are themselves compressed and stored in the block.
	this->block_.PackFooter(); // Pack footer into buffer.
	codec_manager.zstd_codec.Compress(this->block_.footer_support);

	return(true);
}

}
