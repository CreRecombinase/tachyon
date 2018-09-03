#ifndef VCF_IMPORTER_SLAVE_H_
#define VCF_IMPORTER_SLAVE_H_

#include "support/helpers.h"
#include "algorithm/compression/compression_manager.h"
#include "algorithm/compression/genotype_encoder.h"
#include "algorithm/permutation/genotype_sorter.h"
#include "algorithm/digest/variant_digest_manager.h"
#include "algorithm/encryption/encryption_decorator.h"
#include "containers/variant_block.h"
#include "containers/vcf_container.h"
#include "core/variant_importer_container_stats.h"
#include "index/variant_index_entry.h"
#include "index/variant_index_meta_entry.h"
#include "io/vcf_utils.h"
#include "io/variant_import_writer.h"
#include "variant_importer.h"

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
	VcfImporterSlave();
	VcfImporterSlave(const settings_type& settings);
	VcfImporterSlave(const self_type& other) = delete;
	VcfImporterSlave(self_type&& other) noexcept = delete;
	VcfImporterSlave& operator=(const self_type& other) = delete;
	VcfImporterSlave& operator=(self_type&& other) = delete;
	~VcfImporterSlave();
	VcfImporterSlave& operator+=(const VcfImporterSlave& other);

	/**<
	 * Import the contents from a provided VcfContainer and target block id
	 * into the local VariantBlock while preparing it for writing.
	 * @param container Source container.
	 * @param block_id  Source block number.
	 * @return          Return TRUE upon success or FALSE otherwise.
	 */
	bool Add(vcf_container_type& container, const uint32_t block_id);

	/**<
	 * Iteratively add meta information and htslib Info/Format/Filter fields
	 * from the source container to the local VariantBlock container.
	 * @param container Destination container.
	 * @return          Returns TRUE upon success or FALSE otherwise.
	 */
	bool AddRecords(const vcf_container_type& container);

	/**<
	 * Wrapper function for adding a single variant site from the source
	 * container.
	 * @param container Source container.
	 * @param position  Source relative offset of target variant site in container.
	 * @param meta      Reference to the relevant MetaEntry object.
	 * @return          Returns TRUE upon success or FALSE otherwise.
	 */
	bool AddRecord(const vcf_container_type& container, const uint32_t position, meta_type& meta);

	/**<
	 * Add all htslib Info/Format/Filter fields from the src record to the internal VariantBlock
	 * container. Also updates the appropriate pattern id field in the dst MetaEntry.
	 * @param record Source bcf1_t record pointer.
	 * @param meta   Reference to destination MetaEntry.
	 * @return       Returns TRUE upon success or FALSE otherwise.
	 */
	bool AddVcfInfo(const bcf1_t* record, meta_type& meta);
	bool AddVcfFormatInfo(const bcf1_t* record, meta_type& meta);
	bool AddVcfFilterInfo(const bcf1_t* record, meta_type& meta);
	bool AddVcfInfoPattern(const std::vector<int>& pattern, meta_type& meta);
	bool AddVcfFormatPattern(const std::vector<int>& pattern, meta_type& meta);
	bool AddVcfFilterPattern(const std::vector<int>& pattern, meta_type& meta);

	/**<
	 * Attempts to add the target htslib bcf1_t target pointer to the sorted Tachyon index for
	 * speeding up random-access lookups.
	 * @param record Source bcf1_t record pointer.
	 * @param meta   Source reference to MetaEntry.
	 * @return       Returns TRUE upon success or FALSE otherwise.
	 */
	bool IndexRecord(const bcf1_t* record, const meta_type& meta);

	/**<
	 * Adds the Format:GT field to the local VariantBlock container.
	 * @param container    Source variant container.
	 * @param meta_entries Destination MetaEntry records.
	 * @return             Returns TRUE upon success or FALSE otherwise.
	 */
	bool AddGenotypes(const vcf_container_type& container, meta_type* meta_entries);

	inline void SetVcfHeader(std::shared_ptr<vcf_header_type>& header){
		this->vcf_header_ = header;
		// Setup genotype permuter and genotype encoder.
		this->permutator.SetSamples(header->GetNumberSamples());
		this->encoder.SetSamples(header->GetNumberSamples());
	}

public:
	uint32_t n_blocks_processed; // number of blocks processed
	uint32_t block_id; // local block id
	index::Index index; // local index for this thread
	std::shared_ptr<Keychain> keychain; // shared encryption keychain
	std::shared_ptr<vcf_header_type> vcf_header_; // global header
	std::shared_ptr<settings_type> settings_; // internal settings
	bool GT_available_; // genotypes available
	index_entry_type  index_entry; // streaming index entry
	radix_sorter_type permutator;  // GT permuter
	gt_encoder_type   encoder;     // RLE packer
	compression_manager_type compression_manager; // General compression manager
	EncryptionDecorator encryption_decorator;
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
