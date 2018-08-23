#ifndef INDEX_VARIANT_INDEX_CONTIG_H_
#define INDEX_VARIANT_INDEX_CONTIG_H_

#include <fstream>
#include <cmath>
#include <vector>

#include "containers/components/generic_iterator.h"
#include "variant_index_bin.h"

namespace tachyon{
namespace index{

class VariantIndexContig{
public:
	typedef VariantIndexContig self_type;
    typedef std::size_t        size_type;
    typedef VariantIndexBin    value_type;
    typedef value_type&        reference;
    typedef const value_type&  const_reference;
    typedef value_type*        pointer;
    typedef const value_type*  const_pointer;

    typedef yonRawIterator<value_type>       iterator;
   	typedef yonRawIterator<const value_type> const_iterator;

public:
    VariantIndexContig();
    VariantIndexContig(const uint32_t contigID, const uint64_t l_contig, const uint8_t n_levels);
    VariantIndexContig(const self_type& other);
    void operator=(const self_type& other);
    ~VariantIndexContig();

	// Element access
	inline reference at(const size_type& position){ return(this->bins_[position]); }
	inline const_reference at(const size_type& position) const{ return(this->bins_[position]); }
	inline reference operator[](const size_type& position){ return(this->bins_[position]); }
	inline const_reference operator[](const size_type& position) const{ return(this->bins_[position]); }
	inline pointer data(void){ return(this->bins_); }
	inline const_pointer data(void) const{ return(this->bins_); }
	inline reference front(void){ return(this->bins_[0]); }
	inline const_reference front(void) const{ return(this->bins_[0]); }
	inline reference back(void){ return(this->bins_[this->n_bins_ - 1]); }
	inline const_reference back(void) const{ return(this->bins_[this->n_bins_ - 1]); }

	// Capacity
	inline bool empty(void) const{ return(this->n_bins_ == 0); }
	inline const size_type& size(void) const{ return(this->n_bins_); }
	inline const size_type& capacity(void) const{ return(this->n_capacity_); }
	inline const size_type& size_sites(void) const{ return(this->n_sites_); }

	// Iterator
	inline iterator begin(){ return iterator(&this->bins_[0]); }
	inline iterator end(){ return iterator(&this->bins_[this->n_bins_]); }
	inline const_iterator begin() const{ return const_iterator(&this->bins_[0]); }
	inline const_iterator end() const{ return const_iterator(&this->bins_[this->n_bins_]); }
	inline const_iterator cbegin() const{ return const_iterator(&this->bins_[0]); }
	inline const_iterator cend() const{ return const_iterator(&this->bins_[this->n_bins_]); }

	// Accessor
	inline uint32_t& GetContigID(void){ return(this->contigID_); }
	inline const uint32_t& GetContigID(void) const{ return(this->contigID_); }

	/**<
	 * Add a target interval tuple (from,to,block_ID)
	 * @param fromPosition From position of interval
	 * @param toPosition   To position of interval
	 * @param yon_block_id Tachyon block ID (generally a cumulative integer)
	 * @return
	 */
	int32_t Add(const uint64_t& fromPosition, const uint64_t& toPosition, const uint32_t& yon_block_id);

	/**<
	 * Computes the possible bins an interval might overlap
	 * @param from_position From position of interval
	 * @param to_position   To position of interval
	 * @return              Returns a vector of viable overlapping bins
	 */
	std::vector<value_type> PossibleBins(const uint64_t& from_position, const uint64_t& to_position, const bool filter = true) const;

private:
	/**<
	 * Round target integer up to the closest number divisible by 4
	 * @param length Input integer start value
	 * @return       Return a target integer divisible by 4
	 */
    inline uint64_t RoundLengthClosestBase4(const uint64_t& length) const{
		return( ( pow(4,this->n_levels_) - (length % (uint64_t)pow(4,this->n_levels_)) ) + length );
    }

    /**<
     * Pre-calculate the cumulative distribution of 4^(0:levels-1).
     * These values are used to find the array offset for levels > 0
     */
    void CalculateCumulativeSums(void){
    	if(this->n_levels_ == 0) return;

    	delete [] this->bins_cumsum_;
    	this->bins_cumsum_ = new uint32_t[this->n_levels_ + 1]; // inclusive last

    	uint32_t total = 0;
    	for(uint32_t i = 0; i <= this->n_levels_; ++i){
    		total += pow(4,i);
    		this->bins_cumsum_[i] = total - 1; // remove 0 to start relative zero
    	}
    }

    friend std::ostream& operator<<(std::ostream& stream, const self_type& contig){
    	stream.write(reinterpret_cast<const char*>(&contig.contigID_),         sizeof(uint32_t));
    	stream.write(reinterpret_cast<const char*>(&contig.l_contig_),         sizeof(uint64_t));
    	stream.write(reinterpret_cast<const char*>(&contig.l_contig_rounded_), sizeof(uint64_t));
    	stream.write(reinterpret_cast<const char*>(&contig.n_bins_),           sizeof(size_type));
    	stream.write(reinterpret_cast<const char*>(&contig.n_levels_),         sizeof(uint8_t));
    	stream.write(reinterpret_cast<const char*>(&contig.n_sites_),          sizeof(size_type));

    	size_type n_items_written = 0;
    	for(uint32_t i = 0; i < contig.size(); ++i){
    		if(contig.bins_[i].size()) ++n_items_written;
    	}
    	stream.write(reinterpret_cast<const char*>(&n_items_written), sizeof(size_type));

    	for(uint32_t i = 0; i < contig.size(); ++i){
    		// If bins[i] contains data
    		if(contig.bins_[i].size())
    			stream << contig.bins_[i];
    	}
    	return(stream);
    }

    friend std::istream& operator>>(std::istream& stream, self_type& contig){
    	// Clear old data
    	if(contig.size()){
			for(std::size_t i = 0; i < contig.size(); ++i)
				(contig.bins_ + i)->~VariantIndexBin();

			::operator delete[](static_cast<void*>(contig.bins_));
    	}

		delete [] contig.bins_cumsum_;
		contig.bins_cumsum_ = nullptr;

		stream.read(reinterpret_cast<char*>(&contig.contigID_),         sizeof(uint32_t));
		stream.read(reinterpret_cast<char*>(&contig.l_contig_),         sizeof(uint64_t));
		stream.read(reinterpret_cast<char*>(&contig.l_contig_rounded_), sizeof(uint64_t));
		stream.read(reinterpret_cast<char*>(&contig.n_bins_),           sizeof(size_type));
		stream.read(reinterpret_cast<char*>(&contig.n_levels_),         sizeof(uint8_t));
		stream.read(reinterpret_cast<char*>(&contig.n_sites_),          sizeof(size_type));
		contig.n_capacity_ = contig.n_bins_ + 64;
		size_type n_items_written = 0;
		stream.read(reinterpret_cast<char*>(&n_items_written), sizeof(size_type));

		// Allocate new
		contig.bins_ = static_cast<pointer>(::operator new[](contig.capacity()*sizeof(value_type)));
		for(uint32_t i = 0; i < contig.size(); ++i){
			new( &contig.bins_[i] ) value_type(  );
			contig.bins_[i].binID_ = i;
		}
		contig.CalculateCumulativeSums();

		// Load data accordingly
		for(uint32_t i = 0; i < n_items_written; ++i){
			value_type temp;
			stream >> temp;
			//std::cerr << "loading: " << temp.size() << " entries" << std::endl;
			contig.bins_[temp.binID_] = temp;
		}
		//std::cerr << std::endl;
		return(stream);
	}

private:
    uint32_t       contigID_;
	uint64_t       l_contig_;         // as described in header
	uint64_t       l_contig_rounded_; // rounded up to next base-4
	size_type n_bins_;
	size_type n_capacity_;
	uint8_t      n_levels_;    // 7 by default
	size_type n_sites_;
	uint32_t*      bins_cumsum_; // 1, 1+4, 1+4+16, 1+4+16+64, ...
	pointer   bins_;        // bin information
};

}
}



#endif /* INDEX_VARIANT_INDEX_CONTIG_H_ */
