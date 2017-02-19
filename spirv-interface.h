#pragma once
#include <vulkan/vulkan.h>

#include <memory>
#include <string>
#include <vector>
#include <variant>
#include <optional>

namespace spvi
{
    // Value type that simply models a value of type T, but allocates on the heap, useful for breaking cycles in recursive variants
    template<class T> class indirect
    {
        std::unique_ptr<T> value;
    public:
        indirect() : value{new T} {}
        indirect(T && r) : value{new T{std::move(r)}} {}
        indirect(const T & r) : value{new T{r}} {}
        indirect(indirect && r) : value{new T{}} { *value = std::move(*r.value); }
        indirect(const indirect & r) : value{new T{*r.value}} {}

        operator const T & () const { return *value; }

        operator T & () { return *value; }
        indirect & operator = (T && r) { *value = std::move(r); return *this; }
        indirect & operator = (const T & r) { *value = r; return *this; }
        indirect & operator = (indirect && r) { *value = std::move(*r.value); return *this; }
        indirect & operator = (const indirect & r) { *value = *r.value; return *this; }       
    };

    // The type of an input, output, or uniform
    struct type
    {
        // The kinds of numbers that can make up the channels of a sampler or the elements of a matrix
        enum number_kind { float_, int_, uint_ };

        // A sampler type
        struct sampler
        {
            number_kind channel_kind;
            VkImageViewType view_type;
            bool is_multisampled;
            bool is_shadow;
        };

        // A scalar, vector, or matrix type
        struct numeric
        {
            number_kind elem_kind; 
            size_t elem_width;
            size_t row_count, column_count;
            size_t row_stride, column_stride; // TODO: Should these be optional?
        };

        // A fixed-length array type
        struct array
        {
            indirect<type> elem_type;
            size_t elem_count;
            std::optional<size_t> stride;
        };

        // A struct type
        struct structure
        {
            struct member
            {
                std::string name;
                indirect<type> member_type;
                std::optional<size_t> offset;
            };

            std::string name;
            std::vector<member> members;
        };

        std::variant<sampler, numeric, array, structure> contents;
    };

    // The metadata for a single uniform, input or output
    struct variable_info
    {
        uint32_t index;     // Binding index for a uniform within a descriptor set, or location index for a shader input/output
        type type;
        std::string name;
    };

    // The metadata for a single descriptor set
    struct descriptor_set_info
    {
        uint32_t set;
        std::vector<variable_info> descriptors;
    };

    // The metadata for a single shader stage entry point
    struct entry_point_info
    {
        VkShaderStageFlagBits stage;
        std::vector<variable_info> inputs;
        std::vector<variable_info> outputs;
        std::string name;
    };

    // The metadata for a complete SPIR-V module
    struct module_info
    {
        std::vector<descriptor_set_info> descriptor_sets;
        std::vector<entry_point_info> entry_points;

        module_info(const uint32_t * words, size_t word_count);
        module_info(const std::vector<uint32_t> & words) : module_info{words.data(), words.size()} {}
    };
}