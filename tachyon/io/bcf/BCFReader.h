#ifndef BCF_BCFREADER_H_
#define BCF_BCFREADER_H_

#include <cassert>

#include "BCFEntry.h"
#include "../compression/BGZFController.h"


namespace tachyon {
namespace bcf {

class BCFReader{
	typedef BCFReader self_type;
	typedef io::BasicBuffer buffer_type;
	typedef io::BGZFController bgzf_controller_type;
	typedef io::BGZFHeader bgzf_type;
	typedef vcf::VCFHeader header_type;
	typedef core::HeaderContig contig_type;
	typedef BCFEntry entry_type;

public:
	enum bcf_reader_state{BCF_INIT, BCF_OK, BCF_ERROR, BCF_EOF, BCF_STREAM_ERROR};

public:
	BCFReader();
	~BCFReader();

	bool basicStats(const entry_type& entry);

	bool nextBlock(void);
	bool nextVariant(BCFEntry& entry);
	bool parseHeader(void);
	bool open(const std::string input);

	bool getVariants(const U32 n_variants, const double bp_window, bool across_contigs = false); // get N number of variants into buffer

	// Iterator functions
	inline entry_type& operator[](const U32& p){ return(*this->entries[p]); }
	inline const entry_type& operator[](const U32& p) const{return(*this->entries[p]); }
	inline const entry_type* pat(const U32& p){ return(this->entries[p]); }
	inline const U32& size(void) const{ return(this->n_entries); }
	inline const U32& capacity(void) const{ return(this->n_capacity); }
	void resize(void);
	void resize(const U32 new_size);

	inline const bool hasCarryOver(void) const{ return(this->n_carry_over); }

	inline entry_type& first(void){ return((*this)[0]); }
	inline entry_type& last(void){ return((*this)[this->n_entries - 1]); }

public:
	std::ifstream stream;
	U64 filesize;
	U32 current_pointer;
	buffer_type buffer;
	buffer_type header_buffer;
	bgzf_controller_type bgzf_controller;
	header_type header;

	bcf_reader_state state;
	U32 n_entries;
	U32 n_capacity;
	U32 n_carry_over;
	entry_type** entries;
};

}
}

#endif /* BCF_BCFREADER_H_ */