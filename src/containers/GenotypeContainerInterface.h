#ifndef CONTAINERS_GENOTYPECONTAINERINTERFACE_H_
#define CONTAINERS_GENOTYPECONTAINERINTERFACE_H_

#include "Container.h"
#include "../core/GTObject.h"

namespace Tachyon{
namespace Core{

class GenotypeContainerInterface{
private:
    typedef GenotypeContainerInterface  self_type;
    typedef std::size_t                 size_type;
    typedef MetaEntry                   meta_type;

protected:
    typedef GTObject                    gt_object;

public:
    GenotypeContainerInterface(void) : n_entries(0), __data(nullptr), __meta(nullptr){}
    GenotypeContainerInterface(const char* const data, const size_type& n_entries, const U32& n_bytes, const meta_type& meta) :
    	n_entries(n_entries),
	__data(new char[n_bytes]),
    __meta(&meta)
    {
    		memcpy(this->__data, data, n_bytes);
    }
    virtual ~GenotypeContainerInterface(){ delete [] this->__data; }

    // GT-specific functionality
    //virtual void getGTSummary(void) const =0;
    //virtual void getGTSummaryGroups(void) const =0;
    //virtual void std::vector<float> getAlleleFrequency(void) =0;
    //virtual void std::vector<bool> getSamplesMissingness(void) =0;
    //virtual void std::vector<U32> getSamplesPloidy(void) =0;
    //virtual void std::vector<sample_summary> getSamplesSummary(void) =0;
    //virtual void std::vector<upp_triagonal> compareSamplesPairwise(void) =0;
    virtual U32 getSum(void) const =0;
    virtual std::vector<gt_object> getObjects(void) const =0;

    // Capacity
    inline const bool empty(void) const{ return(this->n_entries == 0); }
    inline const size_type& size(void) const{ return(this->n_entries); }

protected:
    size_type        n_entries;
    char*            __data;
    const meta_type* __meta;
};

template <class T>
class GenotypeContainerDiploidRLE : public GenotypeContainerInterface{
private:
	typedef GenotypeContainerInterface    parent_type;
    typedef GenotypeContainerDiploidRLE   self_type;
    typedef T                             value_type;
    typedef value_type&                   reference;
    typedef const value_type&             const_reference;
    typedef value_type*                   pointer;
    typedef const value_type*             const_pointer;
    typedef std::ptrdiff_t                difference_type;
    typedef std::size_t                   size_type;
    typedef MetaEntry                     meta_type;
    typedef MetaHotController             hot_controller_type;

public:
    GenotypeContainerDiploidRLE(){}
    GenotypeContainerDiploidRLE(const char* const data, const U32 n_entries, const meta_type& meta_entry) :
    		parent_type(data, n_entries, n_entries*sizeof(value_type), meta_entry)
	{

	}
    ~GenotypeContainerDiploidRLE(){ }

    void operator()(const char* const data, const U32 n_entries, const meta_type& meta_entry){
    		this->n_entries = n_entries;
    		delete [] this->__data;

    		const T* const re = reinterpret_cast<const T* const>(data);
		for(U32 i = 0; i < n_entries; ++i)
			this->__data[i] = re[i];
    }

    // Element access
    inline reference at(const size_type& position){ return(*reinterpret_cast<const T* const>(&this->__data[position * sizeof(value_type)])); }
    inline const_reference at(const size_type& position) const{ return(*reinterpret_cast<const T* const>(&this->__data[position * sizeof(value_type)])); }
    inline reference operator[](const size_type& position){ return(*reinterpret_cast<const T* const>(&this->__data[position * sizeof(value_type)])); }
    inline const_reference operator[](const size_type& position) const{ return(*reinterpret_cast<const T* const>(&this->__data[position * sizeof(value_type)])); }
    inline reference front(void){ return(*reinterpret_cast<const T* const>(&this->__data[0])); }
    inline const_reference front(void) const{ return(*reinterpret_cast<const T* const>(&this->__data[0])); }
    inline reference back(void){ return(*reinterpret_cast<const T* const>(&this->__data[(this->n_entries - 1) * sizeof(value_type)])); }
    inline const_reference back(void) const{ return(*reinterpret_cast<const T* const>(&this->__data[(this->n_entries - 1) * sizeof(value_type)])); }

    // GT-specific
    U32 getSum(void) const{
    		U32 count = 0;
    		const BYTE shift = this->__meta->hot.controller.gt_anyMissing    ? 2 : 1;
    		const BYTE add   = this->__meta->hot.controller.gt_mixed_phasing ? 1 : 0;

    		for(U32 i = 0; i < this->n_entries; ++i)
			count += this->at(i) >> (2*shift + add);

    		return(count);
    }

    std::vector<gt_object> getObjects(void) const{
    		std::vector<GTObject> ret(this->n_entries);
    		GTObjectDiploidRLE* entries = reinterpret_cast<GTObjectDiploidRLE*>(&ret[0]);
    		for(U32 i = 0; i < this->n_entries; ++i)
    			entries[i](this->at(i), *this->__meta);

    		return(ret);
    }
};

template <class T>
class GenotypeContainerDiploidSimple : public GenotypeContainerInterface{
private:
	typedef GenotypeContainerInterface     parent_type;
    typedef GenotypeContainerDiploidSimple self_type;
    typedef T                              value_type;
    typedef value_type&                    reference;
    typedef const value_type&              const_reference;
    typedef value_type*                    pointer;
    typedef const value_type*              const_pointer;
    typedef std::ptrdiff_t                 difference_type;
    typedef std::size_t                    size_type;
    typedef MetaEntry                      meta_type;
    typedef MetaHotController              hot_controller_type;

public:
    GenotypeContainerDiploidSimple(){}
    GenotypeContainerDiploidSimple(const char* const data, const U32 n_entries, const meta_type& meta_entry) :
    		parent_type(data, n_entries, n_entries*sizeof(value_type), meta_entry)
    	{

    	}
    ~GenotypeContainerDiploidSimple(){  }

    void operator()(const char* const data, const U32 n_entries, const meta_type& meta_entry){
    		this->n_entries = n_entries;
    		delete [] this->__data;

    		const T* const re = reinterpret_cast<const T* const>(data);
		for(U32 i = 0; i < n_entries; ++i)
			this->__data[i] = re[i];
    }

    // Element access
   inline reference at(const size_type& position){ return(*reinterpret_cast<const T* const>(&this->__data[position * sizeof(value_type)])); }
   inline const_reference at(const size_type& position) const{ return(*reinterpret_cast<const T* const>(&this->__data[position * sizeof(value_type)])); }
   inline reference operator[](const size_type& position){ return(*reinterpret_cast<const T* const>(&this->__data[position * sizeof(value_type)])); }
   inline const_reference operator[](const size_type& position) const{ return(*reinterpret_cast<const T* const>(&this->__data[position * sizeof(value_type)])); }
   inline reference front(void){ return(*reinterpret_cast<const T* const>(&this->__data[0])); }
   inline const_reference front(void) const{ return(*reinterpret_cast<const T* const>(&this->__data[0])); }
   inline reference back(void){ return(*reinterpret_cast<const T* const>(&this->__data[(this->n_entries - 1) * sizeof(value_type)])); }
   inline const_reference back(void) const{ return(*reinterpret_cast<const T* const>(&this->__data[(this->n_entries - 1) * sizeof(value_type)])); }

    // GT-specific
	U32 getSum(void) const{
		U32 count = 0;

		const BYTE shift = ceil(log2(this->__meta->cold.n_allele + this->__meta->hot.controller.gt_anyMissing)); // Bits occupied per allele, 1 value for missing
		const BYTE add   = this->__meta->hot.controller.gt_mixed_phasing ? 1 : 0;

		for(U32 i = 0; i < this->n_entries; ++i)
			count += this->at(i) >> (2*shift + add);

		return(count);
	}

	std::vector<gt_object> getObjects(void) const{
		std::vector<GTObject> ret(this->n_entries);
		GTObjectDiploidSimple* entries = reinterpret_cast<GTObjectDiploidSimple*>(&ret[0]);
		for(U32 i = 0; i < this->n_entries; ++i){
			entries[i](this->at(i), *this->__meta);
		}
		return(ret);
	}
};


}
}

#endif /* CONTAINERS_GENOTYPECONTAINERINTERFACE_H_ */
