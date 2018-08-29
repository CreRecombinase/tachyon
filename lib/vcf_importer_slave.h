#ifndef VCF_IMPORTER_SLAVE_H_
#define VCF_IMPORTER_SLAVE_H_

#include "algorithm/compression/compression_manager.h"
#include "algorithm/compression/genotype_encoder.h"
#include "algorithm/permutation/genotype_sorter.h"
#include "algorithm/digest/variant_digest_manager.h"
#include "algorithm/encryption/encryption_decorator.h"
#include "containers/variant_block.h"
#include "containers/vcf_container.h"
#include "io/variant_import_writer.h"
#include "core/variant_importer_container_stats.h"
#include "core/variant_import_settings.h"
#include "index/variant_index_entry.h"
#include "index/variant_index_meta_entry.h"
#include "io/vcf_utils.h"
#include "support/helpers.h"

namespace tachyon{

/**<
 * Slave worker instantiation of VariantImporter. Used exclusively during htslib
 * Vcf importing. Data has to be provided through the cyclic queue generated by the
 * appropriate producer.
 */
class VcfImporterSlave {
public:
	typedef VcfImporterSlave                self_type;
	typedef VariantWriterInterface          writer_type;
	typedef io::BasicBuffer                 buffer_type;
	typedef index::VariantIndexEntry        index_entry_type;
	typedef io::VcfHeader                   vcf_header_type;
	typedef containers::VcfContainer        vcf_container_type;
	typedef algorithm::CompressionManager   compression_manager_type;
	typedef algorithm::GenotypeSorter       radix_sorter_type;
	typedef algorithm::GenotypeEncoder      gt_encoder_type;
	typedef containers::DataContainer       stream_container;
	typedef containers::VariantBlock        block_type;
	typedef support::VariantImporterContainerStats import_stats_type;
	typedef core::MetaEntry                 meta_type;
	typedef VariantImporterSettings         settings_type;
	typedef std::unordered_map<uint32_t, uint32_t> reorder_map_type;
	typedef std::unordered_map<uint64_t, uint32_t> hash_map_type;

public:
	VcfImporterSlave() : n_blocks_processed(0), block_id(0), vcf_header_(nullptr), GT_available_(false){}
	VcfImporterSlave(const settings_type& settings) : n_blocks_processed(0), block_id(0), vcf_header_(nullptr), GT_available_(false){}
	VcfImporterSlave(const self_type& other) = delete;
	VcfImporterSlave(self_type&& other) noexcept = delete;
	VcfImporterSlave& operator=(const self_type& other) = delete;
	VcfImporterSlave& operator=(self_type&& other) = delete;
	~VcfImporterSlave(){
		// do not delete vcf_header, it is not owned by this class
	}

	VcfImporterSlave& operator+=(const VcfImporterSlave& other){
		this->n_blocks_processed += other.n_blocks_processed;
		this->index += other.index;
		this->stats_basic += other.stats_basic;
		this->stats_format += other.stats_format;
		this->stats_info += other.stats_info;
		return(*this);
	}

	/**<
	 * Import the contents from a provided VcfContainer and target block id
	 * into the local VariantBlock while preparing it for writing.
	 * @param container
	 * @param block_id
	 * @return
	 */
	bool Add(vcf_container_type& container, const uint32_t block_id);
	bool AddRecords(const vcf_container_type& container);
	bool AddRecord(const vcf_container_type& container, const uint32_t position, meta_type& meta);
	bool AddVcfInfo(const bcf1_t* record, meta_type& meta);
	bool AddVcfFormatInfo(const bcf1_t* record, meta_type& meta);
	bool AddVcfFilterInfo(const bcf1_t* record, meta_type& meta);
	bool IndexRecord(const bcf1_t* record, const meta_type& meta);
	bool AddVcfInfoPattern(const std::vector<int>& pattern, meta_type& meta);
	bool AddVcfFormatPattern(const std::vector<int>& pattern, meta_type& meta);
	bool AddVcfFilterPattern(const std::vector<int>& pattern, meta_type& meta);
	bool AddGenotypes(const vcf_container_type& container, meta_type* meta_entries);

	inline void SetVcfHeader(vcf_header_type* header){
		this->vcf_header_ = header;
		// Setup genotype permuter and genotype encoder.
		this->permutator.SetSamples(header->GetNumberSamples());
		this->encoder.SetSamples(header->GetNumberSamples());
	}

public:
	uint32_t n_blocks_processed; // number of blocks processed
	uint32_t block_id; // local block id
	index::Index index; // local index
	vcf_header_type* vcf_header_; // global header
	settings_type settings_; // internal settings
	bool GT_available_; // genotypes available
	index_entry_type  index_entry; // streaming index entry
	radix_sorter_type permutator;  // GT permuter
	gt_encoder_type   encoder;     // RLE packer
	compression_manager_type compression_manager; // General compression manager
	block_type block; // Local data container
	// Maps from htslib vcf header to yon header.
	reorder_map_type filter_reorder_map_;
	reorder_map_type info_reorder_map_;
	reorder_map_type format_reorder_map_;
	reorder_map_type contig_reorder_map_;

	// Stats
	support::VariantImporterContainerStats stats_basic;
	support::VariantImporterContainerStats stats_info;
	support::VariantImporterContainerStats stats_format;
};

}



#endif /* VCF_IMPORTER_SLAVE_H_ */
