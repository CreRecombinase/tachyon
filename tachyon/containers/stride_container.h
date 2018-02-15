#ifndef CONTAINERS_STRIDE_CONTAINER_H_
#define CONTAINERS_STRIDE_CONTAINER_H_

#include "datacontainer.h"

namespace tachyon{
namespace containers{

/**<
 * Primary container to handle stride data from data containers
 * This class should be considered for internal use only
 */
template <class return_primitive = U32>
class StrideContainer{
private:
    typedef std::size_t       size_type;
    typedef return_primitive  value_type;
    typedef value_type&       reference;
    typedef const value_type& const_reference;
    typedef value_type*       pointer;
    typedef const value_type* const_pointer;
    typedef std::ptrdiff_t    difference_type;
    typedef DataContainer     data_container_type;

public:
    StrideContainer() :
    	isUniform_(false),
		n_entries(0),
		__entries(nullptr)
	{
    }

    StrideContainer(const data_container_type& container) :
    	isUniform_(false),
		n_entries(0),
		__entries(nullptr)
    {
    	this->__setup(container);
    }

    ~StrideContainer(void){
    	delete [] this->__entries;
    }

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

private:
    void __setup(const data_container_type& container){
    	switch(container.header_stride.controller.type){
		case(tachyon::core::YON_TYPE_8B):  (this->__allocate<SBYTE>(container));  break;
		case(tachyon::core::YON_TYPE_16B): (this->__allocate<S16>(container));    break;
		case(tachyon::core::YON_TYPE_32B): (this->__allocate<S32>(container));    break;
		case(tachyon::core::YON_TYPE_64B): (this->__allocate<S64>(container));    break;
		default: std::cerr << utility::timestamp("ERROR") << "Illegal stride primitive!" << std::endl; exit(1);
		}
    }

    template <class intrinsic_type>
    void __allocate(const data_container_type& container){
    	this->n_entries = container.buffer_strides_uncompressed.size() / sizeof(intrinsic_type);
    	this->__entries = new value_type[this->size()];
    	const intrinsic_type* const strides = reinterpret_cast<const intrinsic_type* const>(container.buffer_strides_uncompressed.data());
    	for(size_type i = 0; i < this->size(); ++i)
    		this->__entries[i] = strides[i];

    	if(container.header_stride.controller.uniform)
    		this->isUniform_ = true;
    }

private:
    bool       isUniform_;
    size_type  n_entries;
    pointer    __entries;
};

}
}



#endif /* CONTAINERS_STRIDE_CONTAINER_H_ */
