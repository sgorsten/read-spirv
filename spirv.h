#pragma once
#include <string>
#include <vector>
#include <optional>

#include <vulkan/spirv.hpp11>

namespace spv
{
    struct instruction
    {
        spv::Op                             op_code;
        uint32_t                            result_id;          // The unique ID of the value created by this instruction's single static assignment
                                                            
        uint32_t                            ids[4];             // IDs of fixed instruction arguments, should match a result_id from some other instruction
        std::vector<uint32_t>               var_ids;            // IDs of variadic instruction arguments, should match a result_id from some other instruction
                                                            
        uint32_t                            nums[4];            // Literal numeric values
        std::string                         string;             // Contents of string literal value
        std::vector<uint32_t>               words;              // Contents of arbitrary-sized literal value
                                                            
    
        spv::ExecutionModel                 execution_model;    // Literal enum value
        spv::StorageClass                   storage_class;      // Literal enum value
        spv::Dim                            dim;                // Literal enum value
        std::optional<spv::AccessQualifier> access_qualifier;   // Literal enum value
        spv::Decoration                     decoration;         // Literal enum value
        spv::ImageFormat                    image_format;       // Literal enum value   
        int                                 function_control;   // Literal bitfield value
    };

    struct module
    {
        uint32_t version_number, generator_id, schema_id;
        std::vector<instruction> instructions;

        const instruction & get_instruction(uint32_t result_id) const;
        const char * get_name(uint32_t result_id) const;
        const char * get_member_name(uint32_t result_id, size_t index) const;
        bool get_decoration(uint32_t result_id, Decoration decoration, size_t size, void * data) const;
        bool get_member_decoration(uint32_t result_id, size_t index, Decoration decoration, size_t size, void * data) const;
    };

    module load_module(const uint32_t * words, size_t word_count);
}
