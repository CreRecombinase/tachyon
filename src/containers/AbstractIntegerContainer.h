#ifndef CONTAINERS_ABSTRACTINTEGERCONTAINER_H_
#define CONTAINERS_ABSTRACTINTEGERCONTAINER_H_

#include "../iterator/IteratorIntegerReference.h"

namespace Tachyon{
namespace Core{

template <class return_primitive>
class AbstractIntegerContainer{
private:
	typedef AbstractIntegerContainer self_type;
	typedef Iterator::IteratorIntegerReference<return_primitive> value_type;
    typedef std::size_t       size_type;
    typedef value_type&       reference;
    typedef const value_type& const_reference;
    typedef value_type*       pointer;
    typedef const value_type* const_pointer;
    typedef std::ptrdiff_t    difference_type;
    typedef IO::BasicBuffer   buffer_type;
    // Function pointers
    typedef const U32 (self_type::*nextStrideFunction)(const buffer_type& buffer, const U32 position) const;

public:
    AbstractIntegerContainer(const Container& container);
    ~AbstractIntegerContainer(){ ::operator delete[](static_cast<void*>(this->__iterators)); delete [] this->__buffer; }

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
	inline reference at(const size_type& position){ return(this->__iterators[position]); }
	inline const_reference at(const size_type& position) const{ return(this->__iterators[position]); }
	inline reference operator[](const size_type& position){ return(this->__iterators[position]); }
	inline const_reference operator[](const size_type& position) const{ return(this->__iterators[position]); }
	inline pointer data(void){ return(this->__iterators); }
	inline const_pointer data(void) const{ return(this->__iterators); }
	inline reference front(void){ return(this->__iterators[0]); }
	inline const_reference front(void) const{ return(this->__iterators[0]); }
	inline reference back(void){ return(this->__iterators[this->n_entries - 1]); }
	inline const_reference back(void) const{ return(this->__iterators[this->n_entries - 1]); }

	// Capacity
	inline const bool empty(void) const{ return(this->n_entries == 0); }
	inline const size_type& size(void) const{ return(this->n_entries); }

	// Iterator
	inline iterator begin(){ return iterator(&this->__iterators[0]); }
	inline iterator end(){ return iterator(&this->__iterators[this->n_entries - 1]); }
	inline const_iterator begin() const{ return const_iterator(&this->__iterators[0]); }
	inline const_iterator end() const{ return const_iterator(&this->__iterators[this->n_entries - 1]); }
	inline const_iterator cbegin() const{ return const_iterator(&this->__iterators[0]); }
	inline const_iterator cend() const{ return const_iterator(&this->__iterators[this->n_entries - 1]); }

private:
	template <class actual_primitive> void __setup(const Container& container, nextStrideFunction func){
		if(container.buffer_strides_uncompressed.size() == 0)
			return;

		if(this->n_entries == 0)
			return;

		this->__iterators = static_cast<pointer>(::operator new[](this->n_entries*sizeof(value_type)));


		U32 current_offset = 0;
		for(U32 i = 0; i < this->n_entries; ++i){
			//std::cerr << i << '\t' << (this->*func)(container.buffer_strides_uncompressed, i) << std::endl;
			new( &this->__iterators[i] ) Iterator::IteratorIntegerReferenceImpl<actual_primitive, return_primitive>( &container.buffer_data_uncompressed.data[current_offset], (this->*func)(container.buffer_strides_uncompressed, i) );
			current_offset += (this->*func)(container.buffer_strides_uncompressed, i) * sizeof(actual_primitive);
		}
		//std::cerr << current_offset << '\t' << container.buffer_data_uncompressed.size() << '\t' << this->n_entries << std::endl;
		assert(current_offset == container.buffer_data_uncompressed.size());

	}

	// Access function
	template <class stride_primitive> inline const U32 __getNextStride(const buffer_type& buffer, const U32 position) const{
		return(*reinterpret_cast<const stride_primitive* const>(&buffer.data[position*sizeof(stride_primitive)]));
	}


private:
    size_t      n_entries;   // number of iterators = number of stride primitives
    char*       __buffer;    // copy data
    value_type* __iterators; // iterators
};

template <class return_primitive>
AbstractIntegerContainer<return_primitive>::AbstractIntegerContainer(const Container& container) :
	n_entries(0),
	__buffer(new char[container.buffer_data_uncompressed.size()]),
	__iterators(nullptr)
{
	if(container.buffer_data_uncompressed.size() == 0)
		return;

	memcpy(this->__buffer, container.buffer_data_uncompressed.data, container.buffer_data_uncompressed.pointer);

	nextStrideFunction func = nullptr;

	switch(container.header_stride.controller.type){
	case(Core::YON_TYPE_8B):  func = &self_type::__getNextStride<BYTE>; this->n_entries = container.buffer_strides_uncompressed.size() / sizeof(BYTE); break;
	case(Core::YON_TYPE_16B): func = &self_type::__getNextStride<U16>;  this->n_entries = container.buffer_strides_uncompressed.size() / sizeof(U16);  break;
	case(Core::YON_TYPE_32B): func = &self_type::__getNextStride<U32>;  this->n_entries = container.buffer_strides_uncompressed.size() / sizeof(U32);  break;
	case(Core::YON_TYPE_64B): func = &self_type::__getNextStride<U64>;  this->n_entries = container.buffer_strides_uncompressed.size() / sizeof(U64);  break;
	default: std::cerr << "Disallowed" << std::endl; return;
	}

	// Assuming there is stride data
	switch(container.header.controller.type){
	case(Core::YON_TYPE_8B):  (this->__setup<BYTE>(container, func)); break;
	case(Core::YON_TYPE_16B): (this->__setup<U16>(container, func));  break;
	case(Core::YON_TYPE_32B): (this->__setup<U32>(container, func));  break;
	case(Core::YON_TYPE_64B): (this->__setup<U64>(container, func));  break;
	default: std::cerr << "Disallowed" << std::endl; return;
	}
}


}
}



#endif /* CONTAINERS_ABSTRACTINTEGERCONTAINER_H_ */
