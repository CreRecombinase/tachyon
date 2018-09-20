#ifndef UTILITY_SUPPORT_VCF_H_
#define UTILITY_SUPPORT_VCF_H_

#include <iostream>
#include <cmath>

#include "core/data_block_settings.h"

namespace tachyon{

#define YON_BYTE_MISSING        INT8_MIN
#define YON_BYTE_EOV            (INT8_MIN+1)
#define YON_SHORT_MISSING       INT16_MIN
#define YON_SHORT_EOV           (INT16_MIN+1)
#define YON_INT_MISSING         INT32_MIN
#define YON_INT_EOV             (INT32_MIN+1)
#define YON_FLOAT_NAN           0x7FC00000
#define YON_FLOAT_MISSING       0x7F800001
#define YON_FLOAT_EOV           0x7F800002

namespace utility{

/**<
 * Helper functions for converting a given encoded tachyon primitive value
 * into a valid Vcf string.
 * @param stream Dst buffer reference.
 * @param data   Src pointer to data.
 * @param n_data Number of entries the src pointer is pointing to.
 * @return       Returns a reference to the dst buffer reference.
 */
io::BasicBuffer& ToVcfString(io::BasicBuffer& stream, const uint8_t* const data, const size_t n_data);
io::BasicBuffer& ToVcfString(io::BasicBuffer& stream, const uint16_t* const data, const size_t n_data);
io::BasicBuffer& ToVcfString(io::BasicBuffer& stream, const uint32_t* const data, const size_t n_data);
io::BasicBuffer& ToVcfString(io::BasicBuffer& stream, const uint64_t* const data, const size_t n_data);
io::BasicBuffer& ToVcfString(io::BasicBuffer& stream, const int8_t* const data, const size_t n_data);
io::BasicBuffer& ToVcfString(io::BasicBuffer& stream, const int16_t* const data, const size_t n_data);
io::BasicBuffer& ToVcfString(io::BasicBuffer& stream, const int32_t* const data, const size_t n_data);
io::BasicBuffer& ToVcfString(io::BasicBuffer& stream, const int64_t* const data, const size_t n_data);
io::BasicBuffer& ToVcfString(io::BasicBuffer& stream, const char* const data, const size_t n_data);
io::BasicBuffer& ToVcfString(io::BasicBuffer& stream, const float* const data, const size_t n_data);
io::BasicBuffer& ToVcfString(io::BasicBuffer& stream, const double* const data, const size_t n_data);
io::BasicBuffer& ToVcfString(io::BasicBuffer& stream, const std::string& string);

/**<
 * Helper functions for adding raw primitive data into a htslib bcf1_t info field.
 * @param rec       Dst htslib bcf1_t record pointer.
 * @param hdr       Pointer reference to htslib bcf header.
 * @param tag       Tag string telling us where to append data in the record.
 * @param data      Src data pointer.
 * @param n_entries Number of entries the src data pointer is pointing to.
 * @return          Returns the (possibly) updated htslib bcf1_t record pointer.
 */
bcf1_t* UpdateHtslibVcfRecordInfo(bcf1_t* rec, bcf_hdr_t* hdr, const std::string& tag, const uint8_t* const data, const size_t n_entries);
bcf1_t* UpdateHtslibVcfRecordInfo(bcf1_t* rec, bcf_hdr_t* hdr, const std::string& tag, const uint16_t* const data, const size_t n_entries);
bcf1_t* UpdateHtslibVcfRecordInfo(bcf1_t* rec, bcf_hdr_t* hdr, const std::string& tag, const uint32_t* const data, const size_t n_entries);
bcf1_t* UpdateHtslibVcfRecordInfo(bcf1_t* rec, bcf_hdr_t* hdr, const std::string& tag, const uint64_t* const data, const size_t n_entries);
bcf1_t* UpdateHtslibVcfRecordInfo(bcf1_t* rec, bcf_hdr_t* hdr, const std::string& tag, const int8_t* const data, const size_t n_entries);
bcf1_t* UpdateHtslibVcfRecordInfo(bcf1_t* rec, bcf_hdr_t* hdr, const std::string& tag, const int16_t* const data, const size_t n_entries);
bcf1_t* UpdateHtslibVcfRecordInfo(bcf1_t* rec, bcf_hdr_t* hdr, const std::string& tag, const int32_t* const data, const size_t n_entries);
bcf1_t* UpdateHtslibVcfRecordInfo(bcf1_t* rec, bcf_hdr_t* hdr, const std::string& tag, const float* const data, const size_t n_entries);
bcf1_t* UpdateHtslibVcfRecordInfo(bcf1_t* rec, bcf_hdr_t* hdr, const std::string& tag, const double* const data, const size_t n_entries);
bcf1_t* UpdateHtslibVcfRecordInfo(bcf1_t* rec, bcf_hdr_t* hdr, const std::string& tag, const std::string& data);

/**<
 * Conversion functions for a given primitive into the accepted primitive types
 * of htslib bcf1_t. Pre-processing steps involves the narrowing or expansion of
 * integers/floats and their corresponding missing/NAN/EOV values.
 * @param src       Src data pointer.
 * @param dst       Dst primitive pointer.
 * @param n_entries Number of entries the src data pointer is pointing to.
 * @return          Returns the (possibly) converted primitive.
 */
int32_t* FormatDataHtslib(const uint8_t* const src, int32_t* dst, const size_t n_entries);
int32_t* FormatDataHtslib(const uint16_t* const src, int32_t* dst, const size_t n_entries);
int32_t* FormatDataHtslib(const uint32_t* const src, int32_t* dst, const size_t n_entries);
int32_t* FormatDataHtslib(const uint64_t* const src, int32_t* dst, const size_t n_entries);
int32_t* FormatDataHtslib(const int8_t* const src, int32_t* dst, const size_t n_entries);
int32_t* FormatDataHtslib(const int16_t* const src, int32_t* dst, const size_t n_entries);
int32_t* FormatDataHtslib(const int32_t* const src, int32_t* dst, const size_t n_entries);
int32_t* FormatDataHtslib(const int64_t* const src, int32_t* dst, const size_t n_entries);
int32_t* FormatDataHtslib(const float* const src, int32_t* dst, const size_t n_entries);
int32_t* FormatDataHtslib(const double* const src, int32_t* dst, const size_t n_entries);

template <class T>
float* FormatDataHtslib(const T* const src, float* dst, const size_t n_entries){
	for(uint32_t i = 0; i < n_entries; ++i) dst[i] = src[i];
	return(dst);
}


}
}

#endif /* UTILITY_SUPPORT_VCF_H_ */
