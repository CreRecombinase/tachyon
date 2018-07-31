#ifndef INDEX_VARIANT_INDEX_H_
#define INDEX_VARIANT_INDEX_H_

#include <cstring>
#include <cmath>
#include <vector>

#include "variant_index_bin.h"
#include "variant_index_contig.h"
#include "variant_index_linear.h"
#include "core/header/variant_header.h"
#include "io/htslib_integration.h"

namespace tachyon{
namespace index{

class VariantIndex{
private:
	typedef VariantIndex       self_type;
    typedef std::size_t        size_type;
    typedef VariantIndexContig value_type;
    typedef VariantIndexLinear linear_type;
    typedef value_type&        reference;
    typedef const value_type&  const_reference;
    typedef value_type*        pointer;
    typedef const value_type*  const_pointer;
    typedef IndexEntry         linear_entry_type;
    typedef YonContig      contig_type;

public:
	VariantIndex();
	VariantIndex(const self_type& other);
	~VariantIndex();;

	class iterator{
	private:
		typedef iterator self_type;
		typedef std::forward_iterator_tag iterator_category;

	public:
		iterator(pointer ptr) : ptr_(ptr) { }
		void operator++() { ptr_++; }
		void operator++(int junk) { ptr_++; }
		reference operator*() const{ return *ptr_; }
		pointer operator->() const{ return ptr_; }
		bool operator==(const self_type& rhs) const{ return ptr_ == rhs.ptr_; }
		bool operator!=(const self_type& rhs) const{ return ptr_ != rhs.ptr_; }
	private:
		pointer ptr_;
	};

	class const_iterator{
	private:
		typedef const_iterator self_type;
		typedef std::forward_iterator_tag iterator_category;

	public:
		const_iterator(pointer ptr) : ptr_(ptr) { }
		void operator++() { ptr_++; }
		void operator++(int junk) { ptr_++; }
		const_reference operator*() const{ return *ptr_; }
		const_pointer operator->() const{ return ptr_; }
		bool operator==(const self_type& rhs) const{ return ptr_ == rhs.ptr_; }
		bool operator!=(const self_type& rhs) const{ return ptr_ != rhs.ptr_; }
	private:
		pointer ptr_;
	};

	// Element access
	inline reference at(const size_type& position){ return(this->contigs_[position]); }
	inline const_reference at(const size_type& position) const{ return(this->contigs_[position]); }
	inline reference operator[](const size_type& position){ return(this->contigs_[position]); }
	inline const_reference operator[](const size_type& position) const{ return(this->contigs_[position]); }
	inline pointer data(void){ return(this->contigs_); }
	inline const_pointer data(void) const{ return(this->contigs_); }
	inline reference front(void){ return(this->contigs_[0]); }
	inline const_reference front(void) const{ return(this->contigs_[0]); }
	inline reference back(void){ return(this->contigs_[this->n_contigs_ - 1]); }
	inline const_reference back(void) const{ return(this->contigs_[this->n_contigs_ - 1]); }

	inline linear_type& linear_at(const size_type& contig_id){ return(this->linear_[contig_id]); }
	inline const linear_type& linear_at(const size_type& contig_id) const{ return(this->linear_[contig_id]); }

	// Capacity
	inline bool empty(void) const{ return(this->n_contigs_ == 0); }
	inline const size_type& size(void) const{ return(this->n_contigs_); }
	inline const size_type& capacity(void) const{ return(this->n_capacity_); }

	// Iterator
	inline iterator begin(){ return iterator(&this->contigs_[0]); }
	inline iterator end(){ return iterator(&this->contigs_[this->n_contigs_]); }
	inline const_iterator begin() const{ return const_iterator(&this->contigs_[0]); }
	inline const_iterator end() const{ return const_iterator(&this->contigs_[this->n_contigs_]); }
	inline const_iterator cbegin() const{ return const_iterator(&this->contigs_[0]); }
	inline const_iterator cend() const{ return const_iterator(&this->contigs_[this->n_contigs_]); }

	self_type& Add(const std::vector<io::VcfContig>& contigs){
		while(this->size() + contigs.size() + 1 >= this->n_capacity_)
			this->resize();

		for(U32 i = 0; i < contigs.size(); ++i){
			const U64 contig_length = contigs[i].n_bases;
			BYTE n_levels = 7;
			U64 bins_lowest = pow(4,n_levels);
			double used = ( bins_lowest - (contig_length % bins_lowest) ) + contig_length;

			if(used / bins_lowest < 2500){
				for(S32 i = n_levels; i != 0; --i){
					if(used/pow(4,i) > 2500){
						n_levels = i;
						break;
					}
				}
			}

			this->Add(i, contig_length, n_levels);
			//std::cerr << "contig: " << this->header->contigs[i].name << "(" << i << ")" << " -> " << contig_length << " levels: " << (int)n_levels << std::endl;
			//std::cerr << "idx size:" << idx.size() << " at " << this->writer->index.variant_index_[i].size() << std::endl;
			//std::cerr << i << "->" << this->header->contigs[i].name << ":" << contig_length << " up to " << (U64)used << " width (bp) lowest level: " << used/pow(4,n_levels) << "@level: " << (int)n_levels << std::endl;
		}
		return(*this);
	}

	/**<
	 * Add a contig with n_levels to the chain
	 * @param l_contig Length of contig
	 * @param n_levels Number of desired 4^N levels
	 * @return         Returns a reference of self
	 */
	inline self_type& Add(const U32& contigID, const U64& l_contig, const BYTE& n_levels){
		if(this->size() + 1 >= this->n_capacity_)
			this->resize();

		new( &this->contigs_[this->n_contigs_] ) value_type( contigID, l_contig, n_levels );
		new( &this->linear_[this->n_contigs_] ) linear_type( contigID );
		++this->n_contigs_;
		return(*this);
	}

	/**<
	 * Add index entry to the linear index given a contig id
	 * @param contigID Target linear index at position contigID
	 * @param entry    Target index entry to push back onto the linear index vector
	 * @return         Returns a reference of self
	 */
	inline self_type& Add(const U32& contigID, const linear_entry_type& entry){
		this->linear_[contigID] += entry;
		return(*this);
	}

	/**<
	 * Resizes the index to accept more contigs than currently allocated
	 * memory for. Resizes for the quad-tree index and the linear index
	 */
	void resize(void);

private:
	friend std::ostream& operator<<(std::ostream& stream, const self_type& index);
	friend std::istream& operator>>(std::istream& stream, self_type& index);

public:
	size_type    n_contigs_; // number of contigs
	size_type    n_capacity_;
	pointer      contigs_;
	linear_type* linear_;
};

}
}



#endif /* INDEX_VARIANT_INDEX_H_ */
