#include "spirv-interface.h"
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

std::ostream & operator << (std::ostream & out, const type & t);

std::ostream & operator << (std::ostream & out, const numeric_type & type)
{
    if(type.row_count == 1 && type.column_count == 1)
    {
        if(type.elem_kind == numeric_type::float_ && type.elem_width == 32) return out << "float";
        if(type.elem_kind == numeric_type::float_ && type.elem_width == 64) return out << "double";
        if(type.elem_kind == numeric_type::int_ && type.elem_width == 32) return out << "int";
        if(type.elem_kind == numeric_type::uint_ && type.elem_width == 32) return out << "unsigned int";
        throw std::logic_error("unsupported type");
    }

    if(type.elem_kind == numeric_type::float_ && type.elem_width == 32) out << "";
    else if(type.elem_kind == numeric_type::float_ && type.elem_width == 64) out << "d";
    else if(type.elem_kind == numeric_type::int_ && type.elem_width == 32) out << "i";
    else if(type.elem_kind == numeric_type::uint_ && type.elem_width == 32) out << "u";
    else throw std::logic_error("unsupported type");

    if(type.column_count == 1) return out << "vec" << type.row_count;
    if(type.column_count == type.row_count) return out << "mat" << type.row_count;
    return out << "mat" << type.column_count << 'x' << type.row_count;
}

std::ostream & operator << (std::ostream & out, const array_type & type)
{
    if(type.stride) out << "layout(stride=" << *type.stride << ") ";
    return out << *type.elem_type << '[' << type.elem_count << ']';
}

std::ostream & operator << (std::ostream & out, const struct_type & type)
{
    out << "struct " << type.name << " {\n";
    for(auto & m : type.members) 
    {
        out << "  ";
        if(m.offset) out << "layout(offset=" << *m.offset << ") ";
        out << m.name << " : " << m.member_type << std::endl;
    }
    return out << "}";
}

std::ostream & operator << (std::ostream & out, const sampler_type & type)
{
    //if(type.access) out << "[[" << get_string(*type.access) << "]] ";
    switch(type.type.elem_kind)
    {
    case numeric_type::int_: out << 'i'; break;
    case numeric_type::uint_: out << 'u'; break;
    }
    switch(type.dim)
    {
    case Dim::Dim1D: out << "sampler1D"; break;
    case Dim::Dim2D: out << "sampler2D"; break;
    case Dim::Dim3D: out << "sampler3D"; break;
    case Dim::Cube: out << "samplerCube"; break;
    case Dim::Rect: out << "sampler2DRect"; break;
    case Dim::Buffer: out << "samplerBuffer"; break;
    case Dim::SubpassData: out << "samplerSubpassData"; break;
    }
    if(type.is_multisampled) out << "MS";
    if(type.is_array) out << "Array";
    if(type.is_shadow) out << "Shadow";
    return out;
}

std::ostream & operator << (std::ostream & out, const type & t)
{
    return std::visit([&out](const auto & x) -> std::ostream & { return out << x; }, (const std::variant<numeric_type, array_type, struct_type, sampler_type> &)t);
}

int main() try
{
    for(auto file : {"test.vert.spv", "test.frag.spv"})
    {
        std::cout << "Information for " << file << ":\n\n";
        auto words = load_spirv_binary(file);
        auto interface = get_module_interface(words.data(), words.size());

        for(auto & u : interface.uniforms)
        {           
            std::cout << "layout(set = " << u.set << ", binding = " << u.binding << ") uniform " << u.name << " : " << u.uniform_type << std::endl;
        }

        for(auto & e : interface.entry_points)
        {
            std::cout << "\nEntry point " << e.name << "(...):\n";
            for(auto & i : e.inputs) std::cout << "  layout(location = " << i.location << ") in " << i.name << " : " << i.interface_type << std::endl;
            for(auto & i : e.outputs) std::cout << "  layout(location = " << i.location << ") out " << i.name << " : " << i.interface_type << std::endl;
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