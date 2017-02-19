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

template<class T> struct indented { const T & value; int indent; };

std::ostream & operator << (std::ostream & out, indented<spvi::type> t);

std::ostream & operator << (std::ostream & out, indented<spvi::type::numeric> t)
{
    if(t.value.row_count == 1 && t.value.column_count == 1)
    {
        if(t.value.elem_kind == spvi::type::float_ && t.value.elem_width == 32) return out << "float";
        if(t.value.elem_kind == spvi::type::float_ && t.value.elem_width == 64) return out << "double";
        if(t.value.elem_kind == spvi::type::int_ && t.value.elem_width == 32) return out << "int";
        if(t.value.elem_kind == spvi::type::uint_ && t.value.elem_width == 32) return out << "unsigned int";
        throw std::logic_error("unsupported type");
    }

    if(t.value.elem_kind == spvi::type::float_ && t.value.elem_width == 32) out << "";
    else if(t.value.elem_kind == spvi::type::float_ && t.value.elem_width == 64) out << "d";
    else if(t.value.elem_kind == spvi::type::int_ && t.value.elem_width == 32) out << "i";
    else if(t.value.elem_kind == spvi::type::uint_ && t.value.elem_width == 32) out << "u";
    else throw std::logic_error("unsupported type");

    if(t.value.column_count == 1) return out << "vec" << t.value.row_count;
    if(t.value.column_count == t.value.row_count) return out << "mat" << t.value.row_count;
    return out << "mat" << t.value.column_count << 'x' << t.value.row_count;
}

std::ostream & operator << (std::ostream & out, indented<spvi::type::array> t)
{
    out << indented<spvi::type>{t.value.elem_type,t.indent} << '[' << t.value.elem_count << ']';
    if(t.value.stride) out << " /*stride=" << *t.value.stride << "*/";
    return out;
}

std::ostream & operator << (std::ostream & out, indented<spvi::type::structure> t)
{
    out << "struct " << t.value.name << "\n" << std::string(t.indent,' ') << "{\n";
    for(auto & m : t.value.members) 
    {
        out << std::string(t.indent+2,' ');
        if(m.offset) out << "Offset " << *m.offset << " ";
        out << m.name << " : " << indented<spvi::type>{m.member_type,t.indent+2} << std::endl;
    }
    return out << std::string(t.indent,' ') << "}";
}

std::ostream & operator << (std::ostream & out, indented<spvi::type::sampler> t)
{
    switch(t.value.channel_kind)
    {
    case spvi::type::int_: out << 'i'; break;
    case spvi::type::uint_: out << 'u'; break;
    }
    switch(t.value.view_type)
    {
    case VK_IMAGE_VIEW_TYPE_1D: case VK_IMAGE_VIEW_TYPE_1D_ARRAY: out << "sampler1D"; break;
    case VK_IMAGE_VIEW_TYPE_2D: case VK_IMAGE_VIEW_TYPE_2D_ARRAY: out << "sampler2D"; break;
    case VK_IMAGE_VIEW_TYPE_3D: out << "sampler3D"; break;
    case VK_IMAGE_VIEW_TYPE_CUBE: case VK_IMAGE_VIEW_TYPE_CUBE_ARRAY: out << "samplerCube"; break;
    }
    if(t.value.is_multisampled) out << "MS";
    if(t.value.view_type == VK_IMAGE_VIEW_TYPE_1D_ARRAY) out << "Array";
    if(t.value.view_type == VK_IMAGE_VIEW_TYPE_2D_ARRAY) out << "Array";
    if(t.value.view_type == VK_IMAGE_VIEW_TYPE_CUBE_ARRAY) out << "Array";
    if(t.value.is_shadow) out << "Shadow";
    return out;
}

std::ostream & operator << (std::ostream & out, indented<spvi::type> t)
{
    std::visit([&](const auto & x) { out << indented<std::remove_cv_t<std::remove_reference_t<decltype(x)>>>{x,t.indent}; }, t.value.contents);
    return out;
}

int main() try
{
    for(auto file : {"test.vert.spv", "test.frag.spv"})
    {
        const spvi::module_info info(load_spirv_binary(file));

        std::cout << "Module " << file << ":" << std::endl;
        for(auto & desc_set : info.descriptor_sets)
        {           
            std::cout << "  Descriptor set " << desc_set.set << ":" << std::endl;
            for(auto & desc : desc_set.descriptors)
            {
                std::cout << "    Descriptor " << desc.index << " " << desc.name << " : " << indented<spvi::type>{desc.type,4} << std::endl;
            }
        }

        for(auto & e : info.entry_points)
        {
            std::cout << "  ";
            switch(e.stage)
            {
            case VK_SHADER_STAGE_VERTEX_BIT: std::cout << "Vertex"; break;
            case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT: std::cout << "Tesselation control"; break;
            case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT: std::cout << "Tesselation evaluation"; break;
            case VK_SHADER_STAGE_GEOMETRY_BIT: std::cout << "Geometry"; break;
            case VK_SHADER_STAGE_FRAGMENT_BIT: std::cout << "Fragment"; break;
            case VK_SHADER_STAGE_COMPUTE_BIT: std::cout << "Compute"; break;
            default: throw std::logic_error("bad shader stage");
            }
            std::cout << " shader " << e.name << "(...):\n";
            for(auto & i : e.inputs) std::cout << "    Input " << i.index << " " << i.name << " : " << indented<spvi::type>{i.type,4} << std::endl;
            for(auto & i : e.outputs) std::cout << "    Output " << i.index << " " << i.name << " : " << indented<spvi::type>{i.type,4} << std::endl;
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