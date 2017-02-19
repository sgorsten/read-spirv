#include "spirv.h"

const spv::instruction & spv::module::get_instruction(uint32_t result_id) const 
{ 
    for(auto & i : instructions)
    {
        if(i.result_id == result_id) return i; 
    }
    throw std::logic_error("bad id"); 
}

const char * spv::module::get_name(uint32_t result_id) const 
{ 
    for(auto & i : instructions) 
    {
        if(i.op_code == Op::OpName && i.ids[0] == result_id) return i.string.c_str(); 
    }
    throw std::logic_error("no name");
}

const char * spv::module::get_member_name(uint32_t result_id, size_t index) const
{
    for(auto & i : instructions) 
    {
        if(i.op_code == Op::OpMemberName && i.ids[0] == result_id && i.nums[0] == index) return i.string.c_str(); 
    }
    throw std::logic_error("no name");
}

bool spv::module::get_decoration(uint32_t result_id, Decoration decoration, size_t size, void * data) const
{
    for(auto & i : instructions) 
    {
        if(i.op_code == Op::OpDecorate && i.ids[0] == result_id && i.decoration == decoration)
        {
            if(size != i.words.size()*4) throw std::logic_error("insufficient decoration data");
            memcpy(data, i.words.data(), size);
            return true;
        }
    }
    return false;
}

bool spv::module::get_member_decoration(uint32_t result_id, size_t index, Decoration decoration, size_t size, void * data) const
{
    for(auto & i : instructions) 
    {
        if(i.op_code == Op::OpMemberDecorate && i.ids[0] == result_id && i.nums[0] == index && i.decoration == decoration)
        {
            if(size != i.words.size()*4) throw std::logic_error("insufficient decoration data");
            memcpy(data, i.words.data(), size);
            return true;
        }
    }
    return false;
}

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
struct op_code_info { spv::Op op_code; std::vector<part_info> parts; } op_code_infos[]
{
    {spv::Op::OpNop, {}},
    {spv::Op::OpUndef, {{part::id,0}, part::result_id}},
    {spv::Op::OpSourceContinued, {part::string}},
    // OpSource    
    {spv::Op::OpSourceExtension, {part::string}},
    {spv::Op::OpName, {{part::id,0}, part::string}},
    {spv::Op::OpMemberName, {{part::id,0}, {part::num,0}, part::string}}, // type, member, name
    {spv::Op::OpString, {part::result_id, part::string}},
    {spv::Op::OpLine, {{part::id,0}, {part::num,0}, {part::num,1}}}, // file, line, column
    // ...
    {spv::Op::OpEntryPoint, {part::execution_model, {part::id,0}, part::string, part::id_list}}, //id0=function, id_list=interfaces
    // ...
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
    {spv::Op::OpTypeFunction, {part::result_id, {part::id,0}, part::id_list}},
    {spv::Op::OpTypeEvent, {part::result_id}},
    {spv::Op::OpTypeDeviceEvent, {part::result_id}},
    {spv::Op::OpTypeReserveId, {part::result_id}},
    {spv::Op::OpTypeQueue, {part::result_id}},
    {spv::Op::OpTypeQueue, {part::result_id, part::access_qualifier}},
    {spv::Op::OpTypeForwardPointer, {{part::id,0}, part::storage_class}},
    {spv::Op::OpConstantTrue, {{part::id,0}, part::result_id}},
    {spv::Op::OpConstantFalse, {{part::id,0}, part::result_id}},
    {spv::Op::OpConstant, {{part::id,0}, part::result_id, part::word_list}},
    {spv::Op::OpConstantComposite, {{part::id,0}, part::result_id, part::id_list}},
    // ...
    {spv::Op::OpFunction, {{part::id,0}, part::result_id, part::function_control, {part::id,1}}}, //id0=result type, id1=function type
    // ...
    {spv::Op::OpVariable, {{part::id,0}, part::result_id, part::storage_class, part::optional_id}},
    // ...
    {spv::Op::OpDecorate, {{part::id,0}, part::decoration, part::word_list}},
    {spv::Op::OpMemberDecorate, {{part::id,0}, {part::num,0}, part::decoration, part::word_list}},
};
const op_code_info * get_info(spv::Op op_code)
{
    for(const auto & info : op_code_infos) if(info.op_code == op_code) return &info;
    return nullptr;
}

spv::module spv::load_module(const uint32_t * words, size_t word_count)
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
        inst.op_code = static_cast<Op>(*it & spv::OpCodeMask);
        inst.result_id = 0xFFFFFFFF;
        for(auto & id : inst.ids) id = 0xFFFFFFFF;

        const uint32_t op_code_length = *it >> 16;
        const uint32_t * op_code_end = it + op_code_length;
        if(op_code_end > binary_end) throw std::runtime_error("incomplete opcode");
        if(const op_code_info * info = get_info(inst.op_code))
        {
            ++it;
            for(const part_info & p : info->parts) switch(p.p)
            {
            default:                            throw std::logic_error("unsupported instruction part");
            case part::result_id:               inst.result_id = *it++; break;
            case part::id:                      inst.ids[p.i] = *it++; break;
            case part::id_list:                 while(it != op_code_end) inst.var_ids.push_back(*it++); break;
            case part::optional_id:             if(it != op_code_end) inst.var_ids.push_back(*it++); break;
            case part::num:                     inst.nums[p.i] = *it++; break;
            case part::word_list:               while(it != op_code_end) inst.words.push_back(*it++); break;
            case part::dim:                     inst.dim = static_cast<Dim>(*it++); break;
            case part::storage_class:           inst.storage_class = static_cast<StorageClass>(*it++); break;
            case part::decoration:              inst.decoration = static_cast<Decoration>(*it++); break;
            case part::execution_model:         inst.execution_model = static_cast<ExecutionModel>(*it++); break;
            case part::image_format:            inst.image_format = static_cast<ImageFormat>(*it++); break;
            case part::access_qualifier:        inst.access_qualifier = static_cast<AccessQualifier>(*it++); break;
            case part::opt_access_qualifier:    if(it != op_code_end) inst.access_qualifier = static_cast<AccessQualifier>(*it++); break;
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
