#ifndef CONTAINERS_META_CONTAINER_H_
#define CONTAINERS_META_CONTAINER_H_

#include "../core/meta_entry.h"
#include "variantblock.h"

namespace tachyon{
namespace containers{

class MetaContainer {
private:
	typedef MetaContainer            self_type;
    typedef std::size_t              size_type;
    typedef tachyon::core::MetaEntry value_type;
    typedef value_type&              reference;
    typedef const value_type&        const_reference;
    typedef value_type*              pointer;
    typedef const value_type*        const_pointer;
    typedef VariantBlock             block_type;
    typedef VariantBlockHeader          block_header_type;

public:
	MetaContainer(const block_type& block);
	~MetaContainer(void);

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
    inline reference at(const size_type& position){ return(this->__entries[position]); }
    inline const_reference at(const size_type& position) const{ return(this->__entries[position]); }
    inline reference operator[](const size_type& position){ return(this->__entries[position]); }
    inline const_reference operator[](const size_type& position) const{ return(this->__entries[position]); }
    inline pointer data(void){ return(this->__entries); }
    inline const_pointer data(void) const{ return(this->__entries); }
    inline reference front(void){ return(this->__entries[0]); }
    inline const_reference front(void) const{ return(this->__entries[0]); }
    inline reference back(void){ return(this->__entries[this->n_entries - 1]); }
    inline const_reference back(void) const{ return(this->__entries[this->n_entries - 1]); }

    // Capacity
    inline const bool empty(void) const{ return(this->n_entries == 0); }
    inline const size_type& size(void) const{ return(this->n_entries); }

    // Iterator
    inline iterator begin(){ return iterator(&this->__entries[0]); }
    inline iterator end(){ return iterator(&this->__entries[this->n_entries]); }
    inline const_iterator begin() const{ return const_iterator(&this->__entries[0]); }
    inline const_iterator end() const{ return const_iterator(&this->__entries[this->n_entries]); }
    inline const_iterator cbegin() const{ return const_iterator(&this->__entries[0]); }
    inline const_iterator cend() const{ return const_iterator(&this->__entries[this->n_entries]); }

    // special
    bool correctRelativePositions(const block_header_type& header){
    	for(U32 i = 0; i < this->size(); ++i){
    		this->at(i).position += header.minPosition;
    	}
    	return true;
    }

private:
    void __ctor_setup(const block_type& block);

private:
    size_t  n_entries;
    pointer __entries;
};

}
}

#endif /* CONTAINERS_META_CONTAINER_H_ */
