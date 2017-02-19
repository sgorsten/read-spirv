#include "spirv-interface.h"
#include <vulkan/spirv.hpp11>
#include <unordered_map>
#include <algorithm>

////////////////////////////////////
// DOM for the SPIR-V file format //
////////////////////////////////////

namespace
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

        const instruction & get_instruction(uint32_t result_id) const 
        { 
            for(auto & i : instructions) if(i.result_id == result_id) return i; 
            throw std::logic_error("bad id"); 
        }

        const char * get_name(uint32_t result_id) const 
        { 
            for(auto & i : instructions) if(i.op_code == spv::Op::OpName && i.ids[0] == result_id) return i.string.c_str(); 
            throw std::logic_error("no name");
        }

        const char * get_member_name(uint32_t result_id, size_t index) const
        {
            for(auto & i : instructions) if(i.op_code == spv::Op::OpMemberName && i.ids[0] == result_id && i.nums[0] == index) return i.string.c_str(); 
            throw std::logic_error("no name");
        }

        bool get_decoration(uint32_t result_id, spv::Decoration decoration, size_t size, void * data) const
        {
            for(auto & i : instructions) 
            {
                if(i.op_code == spv::Op::OpDecorate && i.ids[0] == result_id && i.decoration == decoration)
                {
                    if(size != i.words.size()*4) throw std::logic_error("insufficient decoration data");
                    memcpy(data, i.words.data(), size);
                    return true;
                }
            }
            return false;
        }

        bool get_member_decoration(uint32_t result_id, size_t index, spv::Decoration decoration, size_t size, void * data) const
        {
            for(auto & i : instructions) 
            {
                if(i.op_code == spv::Op::OpMemberDecorate && i.ids[0] == result_id && i.nums[0] == index && i.decoration == decoration)
                {
                    if(size != i.words.size()*4) throw std::logic_error("insufficient decoration data");
                    memcpy(data, i.words.data(), size);
                    return true;
                }
            }
            return false;
        }
    };

    enum class part
    {
        result_id,   // Used only for the operation which defines a value
        id,          // (Indexed) Argument to an operation, or the target of a name/decoration
        optional_id, // 0 or 1 IDs
        id_list,     // 0 or more IDs
        num,         // (Indexed) Integral arguments to an operation
        string,      // A null terminated string
        word_list,   // Arbitrary-length binary data

        // Single word literal of enum type
        execution_model, storage_class, dim, access_qualifier, decoration, image_format, function_control,

        // Optional single word literal of enum type
        opt_access_qualifier,
    };

    struct part_info 
    { 
        part p; int i; 
        part_info(part p) : p{p}, i{0} {}
        part_info(part p, int i) : p{p}, i{i} {}
    };
    const std::unordered_map<spv::Op, std::vector<part_info>> op_code_infos
    {
        {spv::Op::OpName, {{part::id,0}, part::string}},
        {spv::Op::OpMemberName, {{part::id,0}, {part::num,0}, part::string}}, // type, member, name
        {spv::Op::OpEntryPoint, {part::execution_model, {part::id,0}, part::string, part::id_list}}, //id0=function, id_list=interfaces
        {spv::Op::OpTypeVoid, {part::result_id}},
        {spv::Op::OpTypeBool, {part::result_id}},
        {spv::Op::OpTypeInt, {part::result_id, {part::num,0}, {part::num,1}}},
        {spv::Op::OpTypeFloat, {part::result_id, {part::num,0}}}, // result, width
        {spv::Op::OpTypeVector, {part::result_id, {part::id,0}, {part::num,0}}}, 
        {spv::Op::OpTypeMatrix, {part::result_id, {part::id,0}, {part::num,0}}},
        {spv::Op::OpTypeImage, {part::result_id, {part::id,0}, part::dim, {part::num,0}, {part::num,1}, {part::num,2}, {part::num,3}, part::image_format, part::opt_access_qualifier}},
        {spv::Op::OpTypeSampler, {part::result_id}},
        {spv::Op::OpTypeSampledImage, {part::result_id, {part::id,0}}},
        {spv::Op::OpTypeArray, {part::result_id, {part::id,0}, {part::id,1}}},
        {spv::Op::OpTypeRuntimeArray, {part::result_id, {part::id,0}}},
        {spv::Op::OpTypeStruct, {part::result_id, part::id_list}},
        {spv::Op::OpTypeOpaque, {part::result_id, part::string}},
        {spv::Op::OpTypePointer, {part::result_id, part::storage_class, {part::id,0}}},
        {spv::Op::OpConstant, {{part::id,0}, part::result_id, part::word_list}},
        {spv::Op::OpVariable, {{part::id,0}, part::result_id, part::storage_class, part::optional_id}},
        {spv::Op::OpDecorate, {{part::id,0}, part::decoration, part::word_list}},
        {spv::Op::OpMemberDecorate, {{part::id,0}, {part::num,0}, part::decoration, part::word_list}},
    };

    module load_module(const uint32_t * words, size_t word_count)
    {
        if(word_count < 5) throw std::runtime_error("not SPIR-V");
        if(words[0] != 0x07230203) throw std::runtime_error("not SPIR-V");    

        module m;
        m.version_number = words[1];
        m.generator_id = words[2];
        m.schema_id = words[4];

        const uint32_t * it = words + 5, * binary_end = words + word_count;
        while(it != binary_end)
        {
            instruction inst {};
            inst.op_code = static_cast<spv::Op>(*it & spv::OpCodeMask);
            inst.result_id = 0xFFFFFFFF;
            for(auto & id : inst.ids) id = 0xFFFFFFFF;

            const uint32_t op_code_length = *it >> 16;
            const uint32_t * op_code_end = it + op_code_length;
            if(op_code_end > binary_end) throw std::runtime_error("incomplete opcode");

            auto it_info = op_code_infos.find(inst.op_code);
            if(it_info != op_code_infos.end())
            {
                ++it;
                for(const part_info & p : it_info->second) switch(p.p)
                {
                default:                            throw std::logic_error("unsupported instruction part");
                case part::result_id:               inst.result_id = *it++; break;
                case part::id:                      inst.ids[p.i] = *it++; break;
                case part::id_list:                 while(it != op_code_end) inst.var_ids.push_back(*it++); break;
                case part::optional_id:             if(it != op_code_end) inst.var_ids.push_back(*it++); break;
                case part::num:                     inst.nums[p.i] = *it++; break;
                case part::word_list:               while(it != op_code_end) inst.words.push_back(*it++); break;
                case part::dim:                     inst.dim = static_cast<spv::Dim>(*it++); break;
                case part::storage_class:           inst.storage_class = static_cast<spv::StorageClass>(*it++); break;
                case part::decoration:              inst.decoration = static_cast<spv::Decoration>(*it++); break;
                case part::execution_model:         inst.execution_model = static_cast<spv::ExecutionModel>(*it++); break;
                case part::image_format:            inst.image_format = static_cast<spv::ImageFormat>(*it++); break;
                case part::access_qualifier:        inst.access_qualifier = static_cast<spv::AccessQualifier>(*it++); break;
                case part::opt_access_qualifier:    if(it != op_code_end) inst.access_qualifier = static_cast<spv::AccessQualifier>(*it++); break;
                case part::function_control:        inst.function_control = static_cast<int>(*it++); break;
                case part::string:
                    const char * s = reinterpret_cast<const char *>(it);
                    const size_t max_length = (op_code_end - it) * 4, length = strnlen(s, max_length);
                    if(length == max_length) throw std::runtime_error("missing null terminator");
                    inst.string.assign(s, s+length);
                    it += length/4+1;
                    break;
                }
                if(it != op_code_end) throw std::logic_error("instruction contains extra data");
            }
            m.instructions.push_back(inst);
            it = op_code_end;
        }

        return m;
    }
}

//////////////
// Analysis //
//////////////

static spvi::type::numeric convert_numeric_type(const module & mod, const instruction & inst, uint32_t matrix_stride)
{
    spvi::type::numeric element_type;
    switch(inst.op_code)
    {
    case spv::Op::OpTypeFloat: return {spvi::type::float_, inst.nums[0], 1, 1, 0, 0};
    case spv::Op::OpTypeInt: return {inst.nums[1] ? spvi::type::int_ : spvi::type::uint_, inst.nums[0], 1, 1};
    case spv::Op::OpTypeVector: 
        element_type = convert_numeric_type(mod, mod.get_instruction(inst.ids[0]), matrix_stride);
        element_type.row_count = inst.nums[0];
        element_type.row_stride = element_type.elem_width/8;
        return element_type;
    case spv::Op::OpTypeMatrix: 
        element_type = convert_numeric_type(mod, mod.get_instruction(inst.ids[0]), matrix_stride);
        element_type.column_count = inst.nums[0];
        element_type.column_stride = matrix_stride;
        return element_type;
    default: throw std::logic_error("wrong type");
    }
}

static size_t decode_array_length(const module & mod, const instruction & inst)
{
    if(inst.op_code != spv::Op::OpConstant) throw std::logic_error("array length not a constant value");
    auto type = mod.get_instruction(inst.ids[0]);
    if(type.op_code != spv::Op::OpTypeInt) throw std::logic_error("array length not an integer constant");
    if(type.nums[1]) // signed
    {
        switch(type.nums[0]) // width
        {
        case 32: return static_cast<size_t>(reinterpret_cast<const int32_t &>(inst.words[0]));
        case 64: return static_cast<size_t>(reinterpret_cast<const int64_t &>(inst.words[0]));
        default: throw std::logic_error("unsupported width");
        }
    }
    else // unsigned
    {
        switch(type.nums[0]) // width
        {
        case 32: return static_cast<size_t>(reinterpret_cast<const uint32_t &>(inst.words[0]));
        case 64: return static_cast<size_t>(reinterpret_cast<const uint64_t &>(inst.words[0]));
        default: throw std::logic_error("unsupported width");
        }
    }
}

static spvi::type convert_type(const module & mod, const instruction & inst, uint32_t matrix_stride)
{
    if(inst.op_code == spv::Op::OpTypeStruct)
    {
        spvi::type::structure r {mod.get_name(inst.result_id)};
        for(size_t i=0; i<inst.var_ids.size(); ++i)
        {
            // Note: Input/output structs might not have a physical layout, so Offset may not always be present
            std::optional<size_t> opt_offset; uint32_t offset;
            if(mod.get_member_decoration(inst.result_id, i, spv::Decoration::Offset, sizeof(offset), &offset)) opt_offset = offset;

            // MatrixStride decorations can be applied to struct members, so make sure to check for their presence
            mod.get_member_decoration(inst.result_id, i, spv::Decoration::MatrixStride, sizeof(matrix_stride), &matrix_stride);
            r.members.push_back({mod.get_member_name(inst.result_id, i), convert_type(mod, mod.get_instruction(inst.var_ids[i]), matrix_stride), opt_offset});
        }
        return {r};    
    }

    if(inst.op_code == spv::Op::OpTypeArray)
    {
        // Note: Input/output arrays might not have a physical layout, so ArrayStride may not always be present
        std::optional<size_t> opt_stride; uint32_t stride;
        if(mod.get_decoration(inst.result_id, spv::Decoration::ArrayStride, sizeof(stride), &stride)) opt_stride = stride;
        return {spvi::type::array{convert_type(mod, mod.get_instruction(inst.ids[0]), matrix_stride), decode_array_length(mod, mod.get_instruction(inst.ids[1])), opt_stride}};
    }

    if(inst.op_code == spv::Op::OpTypeSampledImage)
    {
        auto image_inst = mod.get_instruction(inst.ids[0]);
        if(image_inst.op_code != spv::Op::OpTypeImage) throw std::logic_error("not an image type");

        spvi::type::sampler s {};
        s.channel_kind = convert_numeric_type(mod, mod.get_instruction(image_inst.ids[0]), 0).elem_kind;
        const bool is_array = image_inst.nums[1] == 1;
        switch(image_inst.dim)
        {
        case spv::Dim::Dim1D: s.view_type = is_array ? VK_IMAGE_VIEW_TYPE_1D_ARRAY : VK_IMAGE_VIEW_TYPE_1D; break;
        case spv::Dim::Dim2D: s.view_type = is_array ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D; break;
        case spv::Dim::Dim3D: s.view_type = VK_IMAGE_VIEW_TYPE_3D; break;
        case spv::Dim::Cube: s.view_type = is_array ? VK_IMAGE_VIEW_TYPE_CUBE_ARRAY : VK_IMAGE_VIEW_TYPE_CUBE; break;
        default: throw std::logic_error("unsupported image Dim"); break;
        }
        if(image_inst.nums[2] == 1) s.is_multisampled = true;
        if(image_inst.nums[0] == 1) s.is_shadow = true;
        return {s};
    }

    return {convert_numeric_type(mod, inst, matrix_stride)};
}

static spvi::descriptor_set_info & get_set(std::vector<spvi::descriptor_set_info> & sets, uint32_t index)
{
    for(auto & set : sets) if(set.set == index) return set;
    sets.push_back({index});
    return sets.back();
}

spvi::module_info::module_info(const uint32_t * words, size_t word_count)
{
    module mod = load_module(words, word_count);

    for(const auto & inst : mod.instructions)
    {
        // Uniform blocks have storage class Uniform and samplers have storage class UniformConstant
        if(inst.op_code == spv::Op::OpVariable && (inst.storage_class == spv::StorageClass::Uniform || inst.storage_class == spv::StorageClass::UniformConstant))
        {
            uint32_t set, binding;
            if(!mod.get_decoration(inst.result_id, spv::Decoration::DescriptorSet, sizeof(set), &set)) throw std::logic_error("missing set qualifier");
            if(!mod.get_decoration(inst.result_id, spv::Decoration::Binding, sizeof(binding), &binding)) throw std::logic_error("missing binding qualifier");

            auto type_inst = mod.get_instruction(inst.ids[0]);
            if(type_inst.op_code != spv::Op::OpTypePointer) throw std::logic_error("uniform variable type is not a pointer");           
            get_set(descriptor_sets, set).descriptors.push_back({binding, convert_type(mod, mod.get_instruction(type_inst.ids[0]), 0), mod.get_name(inst.result_id)});
        }

        if(inst.op_code == spv::Op::OpEntryPoint)
        {
            entry_point_info e;
            switch(inst.execution_model)
            {
            case spv::ExecutionModel::Vertex: e.stage = VK_SHADER_STAGE_VERTEX_BIT; break;
            case spv::ExecutionModel::TessellationControl: e.stage = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT; break;
            case spv::ExecutionModel::TessellationEvaluation: e.stage = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT; break;
            case spv::ExecutionModel::Geometry: e.stage = VK_SHADER_STAGE_GEOMETRY_BIT; break;
            case spv::ExecutionModel::Fragment: e.stage = VK_SHADER_STAGE_FRAGMENT_BIT; break;
            case spv::ExecutionModel::GLCompute: e.stage = VK_SHADER_STAGE_COMPUTE_BIT; break;
            default: throw std::logic_error("bad ExecutionModel");
            }
            e.name = inst.string;
            for(auto id : inst.var_ids)
            {
                // Skip over inputs/outputs without an explicit location (such as the BuiltIn block)
                uint32_t location;
                if(!mod.get_decoration(id, spv::Decoration::Location, sizeof(location), &location)) continue;

                auto iface = mod.get_instruction(id);
                auto type_inst = mod.get_instruction(iface.ids[0]);
                if(type_inst.op_code != spv::Op::OpTypePointer) throw std::logic_error("interface variable type is not a pointer");

                variable_info info {location, convert_type(mod, mod.get_instruction(type_inst.ids[0]), 0), mod.get_name(id)};
                switch(iface.storage_class)
                {
                case spv::StorageClass::Input: e.inputs.push_back(info); break;
                case spv::StorageClass::Output: e.outputs.push_back(info); break;
                default: throw std::logic_error("bad storage class");
                }
            }
            entry_points.push_back(std::move(e));
        }
    }

    std::sort(begin(descriptor_sets), end(descriptor_sets), [](auto & l, auto & r) { return l.set < r.set; });
    for(auto & set : descriptor_sets) std::sort(begin(set.descriptors), end(set.descriptors), [](auto & l, auto & r) { return l.index < r.index; });

    std::sort(begin(entry_points), end(entry_points), [](auto & l, auto & r) { return std::tie(l.stage, l.name) < std::tie(r.stage, r.name); });
    for(auto & e : entry_points)
    {
        std::sort(begin(e.inputs), end(e.inputs), [](auto & l, auto & r) { return l.index < r.index; });
        std::sort(begin(e.outputs), end(e.outputs), [](auto & l, auto & r) { return l.index < r.index; });
    }
}
