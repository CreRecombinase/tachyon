#ifndef ALGORITHM_PARALLEL_VARIANT_BASE_SLAVE_H_
#define ALGORITHM_PARALLEL_VARIANT_BASE_SLAVE_H_

#include "containers/variant_block_container.h"
#include "core/ts_tv_object.h"

namespace tachyon {

class VariantBaseSlaveInterface {
public:
	VariantBaseSlaveInterface(){}
	virtual ~VariantBaseSlaveInterface(){}

	virtual bool Unpack(containers::VariantBlock*& dc) =0;

public:

};

class VariantBaseSlave : public VariantBaseSlaveInterface {
public:
	VariantBaseSlave(void){}
	virtual ~VariantBaseSlave(){}

	/**<
	 * Basic unpacking function for VariantBlock data. Attempts
	 * to move the data to the local VariantBlockContainer followed
	 * by decryption and decompression.
	 * @param dc Src VariantBlock pointer reference as provided by the shared data pool generated by a producer.
	 * @return   Returns TRUE upon success or FALSE otherwise.
	 */
	virtual bool Unpack(containers::VariantBlock*& dc){
		vc.GetBlock() = std::move(*dc); // copy test
		delete dc;
		dc = nullptr;

		/*
		if(vc.AnyEncrypted()){
			if(this->keychain.size() == 0){
				std::cerr << utility::timestamp("ERROR", "DECRYPTION") << "Data is encrypted but no keychain was provided!" << std::endl;
				return false;
			}

			encryption_manager_type encryption_manager;
			if(!encryption_manager.Decrypt(vc.GetBlock(), this->keychain)){
				std::cerr << utility::timestamp("ERROR", "DECRYPTION") << "Failed decryption!" << std::endl;
				return false;
			}
		}
		*/

		// Internally decompress available data
		if(!this->codec_manager.Decompress(vc.GetBlock())){
			std::cerr << utility::timestamp("ERROR", "COMPRESSION") << "Failed decompression!" << std::endl;
			return false;
		}

		return(true);
	}

public:
	containers::VariantBlockContainer vc;
	DataBlockSettings settings;
	algorithm::CompressionManager codec_manager;
};

class VariantSlaveTsTv : public VariantBaseSlave {
protected:
	typedef VariantBaseSlave parent_type;

public:
	VariantSlaveTsTv(void){}
	~VariantSlaveTsTv(){}

	bool GatherGenotypeStatistics(containers::VariantBlock*& dc){
		if(parent_type::Unpack(dc) == false){
			std::cerr << "returning becuase of eerror" << std::endl;
			return false;
		}

		VariantReaderObjects* objects = this->vc.LoadObjects(this->settings);
		yon1_t* entries = this->vc.LazyEvaluate(*objects);

		for(uint32_t i = 0; i < objects->meta_container->size(); ++i){
			if(entries[i].is_loaded_gt){
				entries[i].gt->ExpandExternal(this->vc.GetAllocatedGenotypeMemory());
				s.Update(entries[i], this->vc.GetAllocatedGenotypeMemory());
				//s.Update(entries[i]);
			}
		}


		delete [] entries;
		delete objects;
		return true;
	}

public:
	yon_stats_tstv s;
};

}




#endif /* ALGORITHM_PARALLEL_VARIANT_BASE_SLAVE_H_ */
