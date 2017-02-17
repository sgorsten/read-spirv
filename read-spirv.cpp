#include "spirv.h"
#include <iostream>

std::vector<uint32_t> load_spirv_binary(const char * path)
{
    FILE * f = fopen(path, "rb");
    fseek(f, 0, SEEK_END);
    auto len = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::vector<uint32_t> words(len/4);
    fread(words.data(), sizeof(uint32_t), words.size(), f);
    fclose(f);
    return words;
}

struct const_printer { const spv::module & mod; const spv::instruction & inst; };
struct type_printer { const spv::module & mod; const spv::instruction & inst; size_t indent; };

std::ostream & operator << (std::ostream & out, const_printer t)
{
    switch(t.inst.op_code)
    {
    case spv::OpConstantTrue: return out << "true";
    case spv::OpConstantFalse: return out << "false";
    case spv::OpConstant:
        auto type = t.mod.get_instruction(t.inst.ids[0]);
        switch(type.op_code)
        {
        case spv::OpTypeInt:
            switch(type.nums[1]) // signedness
            {
            case 0:
                switch(type.nums[0]) // width
                {
                case 32: return out << reinterpret_cast<const int32_t &>(t.inst.words[0]);
                case 64: return out << reinterpret_cast<const int64_t &>(t.inst.words[0]);
                default: throw std::logic_error("unsupported width");
                }
            case 1:
                switch(type.nums[0]) // width
                {
                case 32: return out << reinterpret_cast<const uint32_t &>(t.inst.words[0]);
                case 64: return out << reinterpret_cast<const uint64_t &>(t.inst.words[0]);
                default: throw std::logic_error("unsupported width");
                }
            default: throw std::logic_error("unsupported signedness");
            }
        case spv::OpTypeFloat:
            switch(type.nums[0]) // width
            {
            case 32: return out << reinterpret_cast<const float &>(t.inst.words[0]);
            case 64: return out << reinterpret_cast<const double &>(t.inst.words[0]);
            default: throw std::logic_error("unsupported width");
            }
        default: throw std::logic_error("unsupported constant type");
        }
    }
    return out << "?";
};

std::ostream & operator << (std::ostream & out, type_printer t)
{
    uint32_t stride;
    switch(t.inst.op_code)
    {
    case spv::OpTypeFloat: return out << "float<" << t.inst.nums[0] << '>';
    case spv::OpTypeInt: return out << (t.inst.nums[1] ? "int<" : "uint<") << t.inst.nums[0] << '>';
    case spv::OpTypeVector: return out << "vec<"  << type_printer{t.mod, t.mod.get_instruction(t.inst.ids[0]), t.indent} << ',' << t.inst.nums[0] << '>';
    case spv::OpTypeMatrix: return out << "mat<"  << type_printer{t.mod, t.mod.get_instruction(t.inst.ids[0]), t.indent} << ',' << t.inst.nums[0] << '>';
    case spv::OpTypeArray: 
        if(t.mod.get_decoration(t.inst.result_id, spv::Decoration::ArrayStride, sizeof(stride), &stride)) out << "[[stride=" << stride << "]] ";
        return out << "arr<"  << type_printer{t.mod, t.mod.get_instruction(t.inst.ids[0]), t.indent} << ',' << const_printer{t.mod, t.mod.get_instruction(t.inst.ids[1])} << '>';
    case spv::OpTypePointer: return out << "ptr<" << type_printer{t.mod, t.mod.get_instruction(t.inst.ids[0]), t.indent} << '>';
    case spv::OpTypeSampledImage: return out << "sampled<" << type_printer{t.mod, t.mod.get_instruction(t.inst.ids[0]), t.indent} << '>';
    case spv::OpTypeImage: 
        out << "image" << get_string(t.inst.dim);
        if(t.inst.nums[2] == 1) out << "MS";
        if(t.inst.nums[1] == 1) out << "Array";
        if(t.inst.nums[0] == 1) out << "Shadow";
        out << '<' << type_printer{t.mod, t.mod.get_instruction(t.inst.ids[0]), t.indent};
        if(t.inst.access_qualifier) out << ',' << get_string(*t.inst.access_qualifier);
        return out << '>';
    case spv::OpTypeStruct:
        out << "struct\n" << std::string(t.indent*2,' ') << "{\n";
        for(size_t i=0; i<t.inst.var_ids.size(); ++i)
        {
            spv::BuiltIn built_in;
            uint32_t offset, matrix_stride;            
            out << std::string(t.indent*2+2,' ');
            if(t.mod.get_member_decoration(t.inst.result_id, i, spv::Decoration::BuiltIn, sizeof(built_in), &built_in)) out << "[[" << get_string(built_in) << "]] ";
            if(t.mod.get_member_decoration(t.inst.result_id, i, spv::Decoration::Offset, sizeof(offset), &offset)) out << "[[offset(" << offset << ")]] " ;
            if(t.mod.get_member_decoration(t.inst.result_id, i, spv::Decoration::MatrixStride, sizeof(matrix_stride), &matrix_stride)) out << "[[matrix_stride(" << matrix_stride << ")]] ";
            out << t.mod.get_member_name(t.inst.result_id, i) << " : " << type_printer{t.mod, t.mod.get_instruction(t.inst.var_ids[i]), t.indent+1} << std::endl;
        }
        return out << std::string(t.indent*2,' ') << "}";
    default: return out << t.inst;
    }
}

int main() try
{
    for(auto file : {"test.vert.spv", "test.frag.spv"})
    {
        std::cout << "Information for " << file << ":\n\n";
        auto words = load_spirv_binary(file);
        auto mod = spv::load_module(words.data(), words.size());

        std::cout << "Uniforms:\n";
        for(const auto & inst : mod.instructions)
        {
            if(inst.op_code == spv::OpVariable && (inst.storage_class == spv::StorageClass::Uniform || inst.storage_class == spv::StorageClass::UniformConstant))
            {
                uint32_t set, binding;
                std::cout << "  ";
                if(mod.get_decoration(inst.result_id, spv::Decoration::DescriptorSet, sizeof(set), &set)) std::cout << "[[set(" << set << ")]] ";
                if(mod.get_decoration(inst.result_id, spv::Decoration::Binding, sizeof(binding), &binding))  std::cout << "[[binding(" << binding << ")]] ";
                auto name = mod.get_name(inst.result_id);
                std::cout << (name[0] ? name : "$unnamed") << " : " << type_printer{mod, mod.get_instruction(inst.ids[0]), 1} << std::endl;
            }
        }

        for(const auto & inst : mod.instructions)
        {
            if(inst.op_code == spv::OpEntryPoint)
            {
                std::cout << "\nEntry point " << inst.string << "(...): " << std::endl;
                for(auto id : inst.var_ids)
                {
                    auto iface = mod.get_instruction(id);
                    std::cout << "  ";
                    uint32_t location;
                    if(mod.get_decoration(iface.result_id, spv::Decoration::Location, sizeof(location), &location)) std::cout << "[[location(" << location << ")]] ";
                    switch(iface.storage_class)
                    {
                    case spv::StorageClass::Input: std::cout << "[[in]] "; break;
                    case spv::StorageClass::Output: std::cout << "[[out]] "; break;
                    default: std::cout << "[[" << get_string(iface.storage_class) << "]] "; break;
                    }
                    auto name = mod.get_name(id);
                    std::cout << (name[0] ? name : "$unnamed") << " : " << type_printer{mod, mod.get_instruction(iface.ids[0]), 1} << std::endl;
                }
            }
        }
        std::cout << std::endl;
    }
	return EXIT_SUCCESS;
}
catch (const std::exception & e)
{
	std::cerr << e.what() << std::endl;
	return EXIT_FAILURE;
}