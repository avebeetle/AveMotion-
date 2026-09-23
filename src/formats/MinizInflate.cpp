#include "MinizInflate.hpp"

// Samsung rlottie vendors the same single-header miniz implementation.  This
// translation unit intentionally prefixes every externally visible miniz
// symbol that can be emitted by the reduced build below.  Relying on hidden
// visibility or MINIZ_EXPORT is not sufficient because this historical miniz
// version defines several low-level tinfl/tdefl entry points without that
// decoration, which causes duplicate definitions when both static libraries
// are linked into one executable.
#define miniz_def_alloc_func avemotion_miniz_def_alloc_func
#define miniz_def_free_func avemotion_miniz_def_free_func
#define miniz_def_realloc_func avemotion_miniz_def_realloc_func
#define mz_adler32 avemotion_miniz_mz_adler32
#define mz_crc32 avemotion_miniz_mz_crc32
#define mz_free avemotion_miniz_mz_free
#define mz_version avemotion_miniz_mz_version
#define tdefl_compress avemotion_miniz_tdefl_compress
#define tdefl_compress_buffer avemotion_miniz_tdefl_compress_buffer
#define tdefl_compress_mem_to_heap avemotion_miniz_tdefl_compress_mem_to_heap
#define tdefl_compress_mem_to_mem avemotion_miniz_tdefl_compress_mem_to_mem
#define tdefl_compress_mem_to_output avemotion_miniz_tdefl_compress_mem_to_output
#define tdefl_compressor_alloc avemotion_miniz_tdefl_compressor_alloc
#define tdefl_compressor_free avemotion_miniz_tdefl_compressor_free
#define tdefl_create_comp_flags_from_zip_params avemotion_miniz_tdefl_create_comp_flags_from_zip_params
#define tdefl_get_adler32 avemotion_miniz_tdefl_get_adler32
#define tdefl_get_prev_return_status avemotion_miniz_tdefl_get_prev_return_status
#define tdefl_init avemotion_miniz_tdefl_init
#define tdefl_write_image_to_png_file_in_memory avemotion_miniz_tdefl_write_image_to_png_file_in_memory
#define tdefl_write_image_to_png_file_in_memory_ex avemotion_miniz_tdefl_write_image_to_png_file_in_memory_ex
#define tinfl_decompress avemotion_miniz_tinfl_decompress
#define tinfl_decompress_mem_to_callback avemotion_miniz_tinfl_decompress_mem_to_callback
#define tinfl_decompress_mem_to_heap avemotion_miniz_tinfl_decompress_mem_to_heap
#define tinfl_decompress_mem_to_mem avemotion_miniz_tinfl_decompress_mem_to_mem
#define tinfl_decompressor_alloc avemotion_miniz_tinfl_decompressor_alloc
#define tinfl_decompressor_free avemotion_miniz_tinfl_decompressor_free

#define MINIZ_NO_ARCHIVE_APIS
#define MINIZ_NO_ZLIB_APIS
#define MINIZ_NO_STDIO
#define MINIZ_NO_TIME
#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#include "miniz.h"

namespace avemotion::formats::detail {

RawInflateResult inflateRawDeflate(
    std::span<const std::byte> input,
    std::span<std::byte> output) noexcept {
    RawInflateResult result;
    if (input.empty() || output.empty()) {
        result.status = RawInflateStatus::BadParameter;
        return result;
    }

    tinfl_decompressor decompressor{};
    tinfl_init(&decompressor);

    auto inputBytes = input.size();
    auto outputBytes = output.size();
    const auto status = tinfl_decompress(
        &decompressor,
        reinterpret_cast<const mz_uint8*>(input.data()),
        &inputBytes,
        reinterpret_cast<mz_uint8*>(output.data()),
        reinterpret_cast<mz_uint8*>(output.data()),
        &outputBytes,
        TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);

    result.inputConsumed = inputBytes;
    result.outputWritten = outputBytes;
    switch (status) {
    case TINFL_STATUS_DONE:
        result.status = RawInflateStatus::Done;
        break;
    case TINFL_STATUS_NEEDS_MORE_INPUT:
    case TINFL_STATUS_FAILED_CANNOT_MAKE_PROGRESS:
        result.status = RawInflateStatus::NeedsMoreInput;
        break;
    case TINFL_STATUS_HAS_MORE_OUTPUT:
        result.status = RawInflateStatus::HasMoreOutput;
        break;
    case TINFL_STATUS_BAD_PARAM:
        result.status = RawInflateStatus::BadParameter;
        break;
    default:
        result.status = RawInflateStatus::Failed;
        break;
    }
    return result;
}

std::uint32_t crc32(std::span<const std::byte> bytes) noexcept {
    return static_cast<std::uint32_t>(mz_crc32(
        MZ_CRC32_INIT,
        reinterpret_cast<const mz_uint8*>(bytes.data()),
        bytes.size()));
}

} // namespace avemotion::formats::detail
