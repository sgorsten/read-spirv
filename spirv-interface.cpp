#include "spirv-interface.h"

numeric_type convert_numeric_type(const spv::module & mod, const spv::instruction & inst, uint32_t matrix_stride)
{
    numeric_type element_type;
    switch(inst.op_code)
    {
    case spv::Op::OpTypeFloat: return {numeric_type::float_, inst.nums[0], 1, 1, 0, 0};
    case spv::Op::OpTypeInt: return {inst.nums[1] ? numeric_type::int_ : numeric_type::uint_, inst.nums[0], 1, 1};
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

size_t decode_array_length(const spv::module & mod, const spv::instruction & inst)
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

type convert_type(const spv::module & mod, const spv::instruction & inst, uint32_t matrix_stride)
{
    if(inst.op_code == spv::Op::OpTypeStruct)
    {
        struct_type r {mod.get_name(inst.result_id)};
        for(size_t i=0; i<inst.var_ids.size(); ++i)
        {
            // Note: Input/output structs might not have a physical layout, so Offset may not always be present
            std::optional<size_t> opt_offset; uint32_t offset;
            if(mod.get_member_decoration(inst.result_id, i, spv::Decoration::Offset, sizeof(offset), &offset)) opt_offset = offset;

            // MatrixStride decorations can be applied to struct members, so make sure to check for their presence
            mod.get_member_decoration(inst.result_id, i, spv::Decoration::MatrixStride, sizeof(matrix_stride), &matrix_stride);
            r.members.push_back({mod.get_member_name(inst.result_id, i), convert_type(mod, mod.get_instruction(inst.var_ids[i]), matrix_stride), opt_offset});
        }
        return r;    
    }

    if(inst.op_code == spv::Op::OpTypeArray)
    {
        // Note: Input/output arrays might not have a physical layout, so ArrayStride may not always be present
        std::optional<size_t> opt_stride; uint32_t stride;
        if(mod.get_decoration(inst.result_id, spv::Decoration::ArrayStride, sizeof(stride), &stride)) opt_stride = stride;
        return array_type{std::make_unique<type>(convert_type(mod, mod.get_instruction(inst.ids[0]), matrix_stride)), decode_array_length(mod, mod.get_instruction(inst.ids[1])), opt_stride};
    }

    if(inst.op_code == spv::Op::OpTypeSampledImage)
    {
        auto image_inst = mod.get_instruction(inst.ids[0]);
        if(image_inst.op_code != spv::Op::OpTypeImage) throw std::logic_error("not an image type");

        sampler_type s {};
        s.type = convert_numeric_type(mod, mod.get_instruction(image_inst.ids[0]), 0);
        s.dim = image_inst.dim;
        if(image_inst.nums[2] == 1) s.is_multisampled = true;
        if(image_inst.nums[1] == 1) s.is_array = true;
        if(image_inst.nums[0] == 1) s.is_shadow = true;
        s.access = image_inst.access_qualifier;
        return s;
    }

    return convert_numeric_type(mod, inst, matrix_stride);
}

module_interface get_module_interface(const spv::module & mod)
{
    size_t unnamed_count = 0;
    module_interface iface;
    for(const auto & inst : mod.instructions)
    {
        // Uniform blocks have storage class Uniform and samplers have storage class UniformConstant
        if(inst.op_code == spv::Op::OpVariable && (inst.storage_class == spv::StorageClass::Uniform || inst.storage_class == spv::StorageClass::UniformConstant))
        {
            auto type_inst = mod.get_instruction(inst.ids[0]);
            if(type_inst.op_code != spv::Op::OpTypePointer) throw std::logic_error("uniform variable type is not a pointer");

            uniform_info u {mod.get_name(inst.result_id), 0, 0, convert_type(mod, mod.get_instruction(type_inst.ids[0]), 0)};
            if(u.name.empty()) u.name = "$" + std::to_string(unnamed_count++);
            if(!mod.get_decoration(inst.result_id, spv::Decoration::DescriptorSet, sizeof(u.set), &u.set)) throw std::logic_error("missing set qualifier");
            if(!mod.get_decoration(inst.result_id, spv::Decoration::Binding, sizeof(u.binding), &u.binding)) throw std::logic_error("missing binding qualifier");
            iface.uniforms.push_back(std::move(u));
        }

        if(inst.op_code == spv::Op::OpEntryPoint)
        {
            entry_point_info e {inst.string};
            for(auto id : inst.var_ids)
            {
                auto iface = mod.get_instruction(id);

                auto type_inst = mod.get_instruction(iface.ids[0]);
                if(type_inst.op_code != spv::Op::OpTypePointer) throw std::logic_error("interface variable type is not a pointer");

                interface_info info {mod.get_name(id), 0, convert_type(mod, mod.get_instruction(type_inst.ids[0]), 0)};
                if(!mod.get_decoration(iface.result_id, spv::Decoration::Location, sizeof(info.location), &info.location)) continue; // throw std::logic_error("missing location qualifier");
                switch(iface.storage_class)
                {
                case spv::StorageClass::Input: e.inputs.push_back(std::move(info)); break;
                case spv::StorageClass::Output: e.outputs.push_back(std::move(info)); break;
                default: throw std::logic_error("bad storage class");
                }
            }
            iface.entry_points.push_back(std::move(e));
        }
    }
    return iface;
}