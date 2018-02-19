#ifndef CORE_HEADER_HEADER_MAGIC_H_
#define CORE_HEADER_HEADER_MAGIC_H_

#include <cstring>

#include "../../support/MagicConstants.h"

namespace tachyon{
namespace core{

struct HeaderMagic{
public:
	typedef HeaderMagic self_type;

public:
	HeaderMagic() :
		major_version(tachyon::constants::TACHYON_VERSION_MAJOR),
		minor_version(tachyon::constants::TACHYON_VERSION_MINOR),
		release_version(tachyon::constants::TACHYON_VERSION_RELEASE),
		controller(0),
		n_samples(0),
		n_contigs(0),
		n_declarations(0),
		l_literals(0),
		l_header(0),
		l_header_uncompressed(0)
	{
		memcpy(&this->magic_string[0],
               &tachyon::constants::FILE_HEADER[0],
                tachyon::constants::FILE_HEADER_LENGTH);
	}

	HeaderMagic(const self_type& other) :
		major_version(other.major_version),
		minor_version(other.minor_version),
		release_version(other.release_version),
		controller(other.controller),
		n_samples(other.n_samples),
		n_contigs(other.n_contigs),
		n_declarations(other.n_declarations),
		l_literals(other.l_literals),
		l_header(other.l_header),
		l_header_uncompressed(other.l_header_uncompressed)
	{
		memcpy(&this->magic_string[0],
			   &other.magic_string[0],
				tachyon::constants::FILE_HEADER_LENGTH);
	}

	~HeaderMagic() = default;

	inline const U64& getNumberSamples(void) const{ return(this->n_samples); }
	inline U64& getNumberSamples(void){ return(this->n_samples); }
	inline const U32& getNumberContigs(void) const{ return(this->n_contigs); }
	inline U32& getNumberContigs(void){ return(this->n_contigs); }

	inline const U16& getController(void) const{ return(this->controller); }
	inline U16& getController(void){ return(this->controller); }

	inline bool validateMagic(void) const{ return(strncmp(&this->magic_string[0], &tachyon::constants::FILE_HEADER[0], tachyon::constants::FILE_HEADER_LENGTH) == 0); }
	inline bool validate(void) const{
		return(this->validateMagic() && this->n_samples > 0 && this->n_contigs > 0 && (this->major_version > 0 || this->minor_version > 0) && this->l_header > 0 && this->l_header_uncompressed > 0);
	}

	inline const bool operator!=(const self_type& other) const{ return(!(*this == other)); }
	inline const bool operator==(const self_type& other) const{
		if(strncmp(&this->magic_string[0], &other.magic_string[0], tachyon::constants::FILE_HEADER_LENGTH) != 0) return false;
		if(this->n_samples != other.n_samples) return false;
		if(this->n_contigs != other.n_contigs) return false;
		return true;
	}

private:
	friend std::ostream& operator<<(std::ostream& stream, const self_type& header){
		stream.write(header.magic_string, tachyon::constants::FILE_HEADER_LENGTH);
		stream.write(reinterpret_cast<const char*>(&tachyon::constants::TACHYON_VERSION_MAJOR), sizeof(S32));
		stream.write(reinterpret_cast<const char*>(&tachyon::constants::TACHYON_VERSION_MINOR), sizeof(S32));
		stream.write(reinterpret_cast<const char*>(&tachyon::constants::TACHYON_VERSION_RELEASE), sizeof(S32));
		stream.write(reinterpret_cast<const char*>(&header.controller), sizeof(U16));
		stream.write(reinterpret_cast<const char*>(&header.n_samples),  sizeof(U64));
		stream.write(reinterpret_cast<const char*>(&header.n_contigs),  sizeof(U32));
		stream.write(reinterpret_cast<const char*>(&header.n_declarations),  sizeof(U32));
		stream.write(reinterpret_cast<const char*>(&header.l_literals),   sizeof(U32));
		stream.write(reinterpret_cast<const char*>(&header.l_header),   sizeof(U32));
		stream.write(reinterpret_cast<const char*>(&header.l_header_uncompressed), sizeof(U32));
		return stream;
	}

	friend std::istream& operator>>(std::istream& stream, self_type& header){
		stream.read(header.magic_string, tachyon::constants::FILE_HEADER_LENGTH);
		stream.read(reinterpret_cast<char*>(&header.major_version), sizeof(S32));
		stream.read(reinterpret_cast<char*>(&header.minor_version), sizeof(S32));
		stream.read(reinterpret_cast<char*>(&header.release_version), sizeof(S32));
		stream.read(reinterpret_cast<char*>(&header.controller),    sizeof(U16));
		stream.read(reinterpret_cast<char*>(&header.n_samples),     sizeof(U64));
		stream.read(reinterpret_cast<char*>(&header.n_contigs),     sizeof(U32));
		stream.read(reinterpret_cast<char*>(&header.n_declarations),     sizeof(U32));
		stream.read(reinterpret_cast<char*>(&header.l_literals),      sizeof(U32));
		stream.read(reinterpret_cast<char*>(&header.l_header),      sizeof(U32));
		stream.read(reinterpret_cast<char*>(&header.l_header_uncompressed), sizeof(U32));
		return(stream);
	}

public:
	char magic_string[tachyon::constants::FILE_HEADER_LENGTH];
	S32  major_version;
	S32  minor_version;
	S32  release_version;
	U16  controller;            // controller
	U64  n_samples;             // number of samples
	U32  n_contigs;             // number of contigs
	U32  n_declarations;        // number of map entries
	U32  l_literals;            // literals length
	U32  l_header;              // compressed length
	U32  l_header_uncompressed; // uncompressed length
};

}
}

#endif /* CORE_HEADER_HEADER_MAGIC_H_ */