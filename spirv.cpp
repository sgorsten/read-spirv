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
        if(i.op_code == OpName && i.ids[0] == result_id) return i.string.c_str(); 
    }
    throw std::logic_error("no name");
}

const char * spv::module::get_member_name(uint32_t result_id, size_t index) const
{
    for(auto & i : instructions) 
    {
        if(i.op_code == OpMemberName && i.ids[0] == result_id && i.nums[0] == index) return i.string.c_str(); 
    }
    throw std::logic_error("no name");
}

bool spv::module::get_decoration(uint32_t result_id, Decoration decoration, size_t size, void * data) const
{
    for(auto & i : instructions) 
    {
        if(i.op_code == OpDecorate && i.ids[0] == result_id && i.decoration == decoration)
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
        if(i.op_code == OpMemberDecorate && i.ids[0] == result_id && i.nums[0] == index && i.decoration == decoration)
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
struct op_code_info { spv::OpCode op_code; std::vector<part_info> parts; } op_code_infos[]
{
    {spv::OpNop, {}},
    {spv::OpUndef, {{part::id,0}, part::result_id}},
    {spv::OpSourceContinued, {part::string}},
    // OpSource    
    {spv::OpSourceExtension, {part::string}},
    {spv::OpName, {{part::id,0}, part::string}},
    {spv::OpMemberName, {{part::id,0}, {part::num,0}, part::string}}, // type, member, name
    {spv::OpString, {part::result_id, part::string}},
    {spv::OpLine, {{part::id,0}, {part::num,0}, {part::num,1}}}, // file, line, column
    // ...
    {spv::OpEntryPoint, {part::execution_model, {part::id,0}, part::string, part::id_list}}, //id0=function, id_list=interfaces
    // ...
    {spv::OpTypeVoid, {part::result_id}},
    {spv::OpTypeBool, {part::result_id}},
    {spv::OpTypeInt, {part::result_id, {part::num,0}, {part::num,1}}},
    {spv::OpTypeFloat, {part::result_id, {part::num,0}}}, // result, width
    {spv::OpTypeVector, {part::result_id, {part::id,0}, {part::num,0}}}, 
    {spv::OpTypeMatrix, {part::result_id, {part::id,0}, {part::num,0}}},
    {spv::OpTypeImage, {part::result_id, {part::id,0}, part::dim, {part::num,0}, {part::num,1}, {part::num,2}, {part::num,3}, part::image_format, part::opt_access_qualifier}},
    {spv::OpTypeSampler, {part::result_id}},
    {spv::OpTypeSampledImage, {part::result_id, {part::id,0}}},
    {spv::OpTypeArray, {part::result_id, {part::id,0}, {part::id,1}}},
    {spv::OpTypeRuntimeArray, {part::result_id, {part::id,0}}},
    {spv::OpTypeStruct, {part::result_id, part::id_list}},
    {spv::OpTypeOpaque, {part::result_id, part::string}},
    {spv::OpTypePointer, {part::result_id, part::storage_class, {part::id,0}}},
    {spv::OpTypeFunction, {part::result_id, {part::id,0}, part::id_list}},
    {spv::OpTypeEvent, {part::result_id}},
    {spv::OpTypeDeviceEvent, {part::result_id}},
    {spv::OpTypeReserveId, {part::result_id}},
    {spv::OpTypeQueue, {part::result_id}},
    {spv::OpTypeQueue, {part::result_id, part::access_qualifier}},
    {spv::OpTypeForwardPointer, {{part::id,0}, part::storage_class}},
    {spv::OpConstantTrue, {{part::id,0}, part::result_id}},
    {spv::OpConstantFalse, {{part::id,0}, part::result_id}},
    {spv::OpConstant, {{part::id,0}, part::result_id, part::word_list}},
    {spv::OpConstantComposite, {{part::id,0}, part::result_id, part::id_list}},
    // ...
    {spv::OpFunction, {{part::id,0}, part::result_id, part::function_control, {part::id,1}}}, //id0=result type, id1=function type
    // ...
    {spv::OpVariable, {{part::id,0}, part::result_id, part::storage_class, part::optional_id}},
    // ...
    {spv::OpDecorate, {{part::id,0}, part::decoration, part::word_list}},
    {spv::OpMemberDecorate, {{part::id,0}, {part::num,0}, part::decoration, part::word_list}},
};
const op_code_info * get_info(spv::OpCode op_code)
{
    for(const auto & info : op_code_infos) if(info.op_code == op_code) return &info;
    return nullptr;
}

uint32_t operator & (spv::FunctionControl a, spv::FunctionControl b) { return uint32_t(a) | uint32_t(b); }
std::ostream & operator << (std::ostream & out, spv::FunctionControl bits)
{
    char sep = ' ';
    if(bits & spv::FunctionControl::Inline) { out << sep << "Inline"; sep = '|'; }
    if(bits & spv::FunctionControl::DontInline) { out << sep << "DontInline"; sep = '|'; }
    if(bits & spv::FunctionControl::Pure) { out << sep << "Pure"; sep = '|'; }
    if(bits & spv::FunctionControl::Const) { out << sep << "Const"; sep = '|'; }
    if(sep == ' ') { out << " None"; }
    return out;
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
        inst.op_code = static_cast<OpCode>(*it & 0xFFFF);
        inst.result_id = 0xFFFFFFFF;
        for(auto & id : inst.ids) id = 0xFFFFFFFF;

        if(inst.op_code == OpConstant)
        {
            int x = 5;
        }

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

std::ostream & operator << (std::ostream & out, const spv::instruction & inst)
{
    if(auto * info = get_info(inst.op_code))
    {
        out << get_string(inst.op_code);
        for(auto & part_info : info->parts) switch(part_info.p)
        {
        default:                            throw std::logic_error("unsupported instruction part");
        case part::result_id:               out << " %" << std::dec << inst.result_id; break;
        case part::id:                      out << " %" << std::dec << inst.ids[part_info.i]; break;
        case part::id_list:                 for(auto id : inst.var_ids) out << " %" << std::dec << id; break;
        case part::optional_id:             for(auto id : inst.var_ids) out << " %" << std::dec << id; break;
        case part::num:                     out << " " << std::dec << inst.nums[part_info.i]; break;
        case part::word_list:               for(auto word : inst.words) out << " 0x" << std::hex << word; break;
        case part::dim:                     out << " " << get_string(inst.dim); break;
        case part::storage_class:           out << " " << get_string(inst.storage_class); break;
        case part::decoration:              out << " " << get_string(inst.decoration); break;
        case part::execution_model:         out << " " << get_string(inst.execution_model); break;
        case part::image_format:            out << " " << get_string(inst.image_format); break;
        case part::access_qualifier:        out << " " << get_string(*inst.access_qualifier); break;
        case part::opt_access_qualifier:    if(inst.access_qualifier) out << " " << get_string(*inst.access_qualifier); break;
        case part::function_control:        out << " " << inst.function_control; break;
        case part::string:                  out << " \"" << inst.string << '"'; break;
        }
        return out;
    }
    else return out << "? " << get_string(inst.op_code);
}

const char * spv::get_string(OpCode n)
{
    switch(n)
    {
    #define X(N) case N: return #N;
    X(OpNop)
    X(OpUndef)
    X(OpSourceContinued)
    X(OpSource)
    X(OpSourceExtension)
    X(OpName)
    X(OpMemberName)
    X(OpString)
    X(OpLine)
    X(OpExtension)
    X(OpExtInstImport)
    X(OpExtInst)
    X(OpMemoryModel)
    X(OpEntryPoint)
    X(OpExecutionMode)
    X(OpCapability)
    X(OpTypeVoid)
    X(OpTypeBool)
    X(OpTypeInt)
    X(OpTypeFloat)
    X(OpTypeVector)
    X(OpTypeMatrix)
    X(OpTypeImage)
    X(OpTypeSampler)
    X(OpTypeSampledImage)
    X(OpTypeArray)
    X(OpTypeRuntimeArray)
    X(OpTypeStruct)
    X(OpTypeOpaque)
    X(OpTypePointer)
    X(OpTypeFunction)
    X(OpTypeEvent)
    X(OpTypeDeviceEvent)
    X(OpTypeReserveId)
    X(OpTypeQueue)
    X(OpTypePipe)
    X(OpTypeForwardPointer)
    X(OpConstantTrue)
    X(OpConstantFalse)
    X(OpConstant)
    X(OpConstantComposite)
    X(OpConstantSampler)
    X(OpConstantNull)
    X(OpSpecConstantTrue)
    X(OpSpecConstantFalse)
    X(OpSpecConstant)
    X(OpSpecConstantComposite)
    X(OpSpecConstantOp)
    X(OpFunction)
    X(OpFunctionParameter)
    X(OpFunctionEnd)
    X(OpFunctionCall)
    X(OpVariable)
    X(OpImageTexelPointer)
    X(OpLoad)
    X(OpStore)
    X(OpCopyMemory)
    X(OpCopyMemorySized)
    X(OpAccessChain)
    X(OpInBoundsAccessChain)
    X(OpPtrAccessChain)
    X(OpArrayLength)
    X(OpGenericPtrMemSemantics)
    X(OpInBoundsPtrAccessChain)
    X(OpDecorate)
    X(OpMemberDecorate)
    X(OpDecorationGroup)
    X(OpGroupDecorate)
    X(OpGroupMemberDecorate)
    X(OpVectorExtractDynamic)
    X(OpVectorInsertDynamic)
    X(OpVectorShuffle)
    X(OpCompositeConstruct)
    X(OpCompositeExtract)
    X(OpCompositeInsert)
    X(OpCopyObject)
    X(OpTranspose)
    X(OpSampledImage)
    X(OpImageSampleImplicitLod)
    X(OpImageSampleExplicitLod)
    X(OpImageSampleDrefImplicitLod)
    X(OpImageSampleDrefExplicitLod)
    X(OpImageSampleProjImplicitLod)
    X(OpImageSampleProjExplicitLod)
    X(OpImageSampleProjDrefImplicitLod)
    X(OpImageSampleProjDrefExplicitLod)
    X(OpImageFetch)
    X(OpImageGather)
    X(OpImageDrefGather)
    X(OpImageRead)
    X(OpImageWrite)
    X(OpImage)
    X(OpImageQueryFormat)
    X(OpImageQueryOrder)
    X(OpImageQuerySizeLod)
    X(OpImageQuerySize)
    X(OpImageQueryLod)
    X(OpImageQueryLevels)
    X(OpImageQuerySamples)
    X(OpConvertFToU)
    X(OpConvertFToS)
    X(OpConvertSToF)
    X(OpConvertUToF)
    X(OpUConvert)
    X(OpSConvert)
    X(OpFConvert)
    X(OpQuantizeToF16)
    X(OpConvertPtrToU)
    X(OpSatConvertSToU)
    X(OpSatConvertUToS)
    X(OpConvertUToPtr)
    X(OpPtrCastToGeneric)
    X(OpGenericCastToPtr)
    X(OpGenericCastToPtrExplicit)
    X(OpBitcast)
    X(OpSNegate)
    X(OpFNegate)
    X(OpIAdd)
    X(OpFAdd)
    X(OpISub)
    X(OpFSub)
    X(OpIMul)
    X(OpFMul)
    X(OpUDiv)
    X(OpSDiv)
    X(OpFDiv)
    X(OpUMod)
    X(OpSRem)
    X(OpSMod)
    X(OpFRem)
    X(OpFMod)
    X(OpVectorTimesScalar)
    X(OpMatrixTimesScalar)
    X(OpVectorTimesMatrix)
    X(OpMatrixTimesVector)
    X(OpMatrixTimesMatrix)
    X(OpOuterProduct)
    X(OpDot)
    X(OpIAddCarry)
    X(OpISubBorrow)
    X(OpUMulExtended)
    X(OpSMulExtended)
    X(OpAny)
    X(OpAll)
    X(OpIsNan)
    X(OpIsInf)
    X(OpIsFinite)
    X(OpIsNormal)
    X(OpSignBitSet)
    X(OpLessOrGreater)
    X(OpOrdered)
    X(OpUnordered)
    X(OpLogicalEqual)
    X(OpLogicalNotEqual)
    X(OpLogicalOr)
    X(OpLogicalAnd)
    X(OpLogicalNot)
    X(OpSelect)
    X(OpIEqual)
    X(OpINotEqual)
    X(OpUGreaterThan)
    X(OpSGreaterThan)
    X(OpUGreaterThanEqual)
    X(OpSGreaterThanEqual)
    X(OpULessThan)
    X(OpSLessThan)
    X(OpULessThanEqual)
    X(OpSLessThanEqual)
    X(OpFOrdEqual)
    X(OpFUnordEqual)
    X(OpFOrdNotEqual)
    X(OpFUnordNotEqual)
    X(OpFOrdLessThan)
    X(OpFUnordLessThan)
    X(OpFOrdGreaterThan)
    X(OpFUnordGreaterThan)
    X(OpFOrdLessThanEqual)
    X(OpFUnordLessThanEqual)
    X(OpFOrdGreaterThanEqual)
    X(OpFUnordGreaterThanEqual)
    X(OpShiftRightLogical)
    X(OpShiftRightArithmetic)
    X(OpShiftLeftLogical)
    X(OpBitwiseOr)
    X(OpBitwiseXor)
    X(OpBitwiseAnd)
    X(OpNot)
    X(OpBitFieldInsert)
    X(OpBitFieldSExtract)
    X(OpBitFieldUExtract)
    X(OpBitReverse)
    X(OpBitCount)
    X(OpDPdx)
    X(OpDPdy)
    X(OpFwidth)
    X(OpDPdxFine)
    X(OpDPdyFine)
    X(OpFwidthFine)
    X(OpDPdxCoarse)
    X(OpDPdyCoarse)
    X(OpFwidthCoarse)
    X(OpEmitVertex)
    X(OpEndPrimitive)
    X(OpEmitStreamVertex)
    X(OpEndStreamPrimitive)
    X(OpControlBarrier)
    X(OpMemoryBarrier)
    X(OpAtomicLoad)
    X(OpAtomicStore)
    X(OpAtomicExchange)
    X(OpAtomicCompareExchange)
    X(OpAtomicCompareExchangeWeak)
    X(OpAtomicIIncrement)
    X(OpAtomicIDecrement)
    X(OpAtomicIAdd)
    X(OpAtomicISub)
    X(OpAtomicSMin)
    X(OpAtomicUMin)
    X(OpAtomicSMax)
    X(OpAtomicUMax)
    X(OpAtomicAnd)
    X(OpAtomicOr)
    X(OpAtomicXor)
    X(OpPhi)
    X(OpLoopMerge)
    X(OpSelectionMerge)
    X(OpLabel)
    X(OpBranch)
    X(OpBranchConditional)
    X(OpSwitch)
    X(OpKill)
    X(OpReturn)
    X(OpReturnValue)
    X(OpUnreachable)
    X(OpLifetimeStart)
    X(OpLifetimeStop)
    X(OpGroupAsyncCopy)
    X(OpGroupWaitEvents)
    X(OpGroupAll)
    X(OpGroupAny)
    X(OpGroupBroadcast)
    X(OpGroupIAdd)
    X(OpGroupFAdd)
    X(OpGroupFMin)
    X(OpGroupUMin)
    X(OpGroupSMin)
    X(OpGroupFMax)
    X(OpGroupUMax)
    X(OpGroupSMax)
    X(OpReadPipe)
    X(OpWritePipe)
    X(OpReservedReadPipe)
    X(OpReservedWritePipe)
    X(OpReserveReadPipePackets)
    X(OpReserveWritePipePackets)
    X(OpCommitReadPipe)
    X(OpCommitWritePipe)
    X(OpIsValidReserveId)
    X(OpGetNumPipePackets)
    X(OpGetMaxPipePackets)
    X(OpGroupReserveReadPipePackets)
    X(OpGroupReserveWritePipePackets)
    X(OpGroupCommitReadPipe)
    X(OpGroupCommitWritePipe)
    X(OpEnqueueMarker)
    X(OpEnqueueKernel)
    X(OpGetKernelNDrangeSubGroupCount)
    X(OpGetKernelNDrangeMaxSubGroupSize)
    X(OpGetKernelWorkGroupSize)
    X(OpGetKernelPreferredWorkGroupSizeMultiple)
    X(OpRetainEvent)
    X(OpReleaseEvent)
    X(OpCreateUserEvent)
    X(OpIsValidEvent)
    X(OpSetUserEventStatus)
    X(OpCaptureEventProfilingInfo)
    X(OpGetDefaultQueue)
    X(OpBuildNDRange)
    X(OpImageSparseSampleImplicitLod)
    X(OpImageSparseSampleExplicitLod)
    X(OpImageSparseSampleDrefImplicitLod)
    X(OpImageSparseSampleDrefExplicitLod)
    X(OpImageSparseSampleProjImplicitLod)
    X(OpImageSparseSampleProjExplicitLod)
    X(OpImageSparseSampleProjDrefImplicitLod)
    X(OpImageSparseSampleProjDrefExplicitLod)
    X(OpImageSparseFetch)
    X(OpImageSparseGather)
    X(OpImageSparseDrefGather)
    X(OpImageSparseTexelsResident)
    X(OpNoLine)
    X(OpAtomicFlagTestAndSet)
    X(OpAtomicFlagClear)
    X(OpImageSparseRead)
    #undef X
    default: throw std::logic_error("bad OpType");
    }
}

const char * spv::get_string(ExecutionModel n)
{
    switch(n)
    {
    #define X(N) case ExecutionModel::N: return #N;
    X(Vertex)
    X(TesselationControl)
    X(TessellationEvaluation)
    X(Geometry)
    X(Fragment)
    X(GLCompute)
    X(Kernel)
    #undef X
    default: throw std::logic_error("bad ExecutionModel");
    }
}

const char * spv::get_string(StorageClass n)
{
    switch(n)
    {
    #define X(N) case StorageClass::N: return #N;
    X(UniformConstant)
    X(Input)
    X(Uniform)
    X(Output) 
    X(Workgroup)
    X(CrossWorkgroup)
    X(Private)
    X(Function)
    X(Generic)
    X(PushConstant)
    X(AtomicCounter)
    X(Image)
    #undef X
    default: throw std::logic_error("bad StorageClass");
    }
}

const char * spv::get_string(Dim n)
{
    switch(n)
    {
    #define X(N) case Dim::N: return #N;
    case Dim::_1D: return "1D";
    case Dim::_2D: return "2D";
    case Dim::_3D: return "3D";
    X(Cube)
    X(Rect)
    X(Buffer)
    X(SubpassData)
    #undef X
    default: throw std::logic_error("bad Dim");
    }
}

const char * spv::get_string(ImageFormat n)
{
    switch(n)
    {
    #define X(N) case ImageFormat::N: return #N;
    X(Unknown)
    X(Rgba32f)
    X(Rgba16f)
    X(R32f)
    X(Rgba8)
    X(Rgba8Snorm)
    X(Rg32f)
    X(Rg16f)
    X(R11fG11fB10f)
    X(R16f)
    X(Rgba16)
    X(Rgb10A2)
    X(Rg16)
    X(Rg8)
    X(R16)
    X(R8)
    X(Rgba16Snorm)
    X(Rg16Snorm)
    X(Rg8Snorm)
    X(R16Snorm)
    X(R8Snorm)
    X(Rgba32i)
    X(Rgba16i)
    X(Rgba8i)
    X(R32i)
    X(Rg32i)
    X(Rg16i)
    X(Rg8i) 
    X(R16i)
    X(R8i)
    X(Rgba32ui)
    X(Rgba16ui)
    X(Rgba8ui)
    X(R32ui)
    X(Rgb10a2ui)
    X(Rg32ui)
    X(Rg16ui)
    X(Rg8ui)
    X(R16ui)
    X(R8ui)
    #undef X
    default: throw std::logic_error("bad ImageFormat");
    }
}

const char * spv::get_string(AccessQualifier n)
{
    switch(n)
    {
    #define X(N) case AccessQualifier::N: return #N;
    X(ReadOnly)
    X(WriteOnly)
    X(ReadWrite)
    #undef X
    default: throw std::logic_error("bad AccessQualifier");
    }
}

const char * spv::get_string(Decoration n)
{
    switch(n)
    {   
    #define X(N) case Decoration::N: return #N;
    X(RelaxedPrecision)
    X(SpecId)
    X(Block)
    X(BufferBlock)
    X(RowMajor)
    X(ColMajor)
    X(ArrayStride)
    X(MatrixStride)
    X(GLSLShared)
    X(GLSLPacked)
    X(CPacked)
    X(BuiltIn)
    X(NoPerspective)
    X(Flat) 
    X(Patch)
    X(Centroid)
    X(Sample)
    X(Invariant)
    X(Restrict)
    X(Aliased)
    X(Volatile)
    X(Constant)
    X(Coherent)
    X(NonWritable)
    X(NonReadable)
    X(Uniform)
    X(SaturatedConversion)
    X(Stream)
    X(Location)
    X(Component)
    X(Index)
    X(Binding)
    X(DescriptorSet)
    X(Offset)
    X(XfbBuffer)
    X(XfbStride)
    X(FuncParamAttr)
    X(FPRoundingMode)
    X(FPFastMathMode)
    X(LinkageAttributes)
    X(NoContraction)
    X(InputAttachmentIndex)
    X(Alignment)
    #undef X
    default: throw std::logic_error("bad Decoration");
    }
}

const char * spv::get_string(BuiltIn n)
{
    switch(n)
    {
    #define X(N) case BuiltIn::N: return #N;
    X(Position)
    X(PointSize)
    X(ClipDistance)
    X(CullDistance)
    X(VertexId)
    X(InstanceId)
    X(PrimitiveId)
    X(InvocationId)
    X(Layer)
    X(ViewportIndex)
    X(TessLevelOuter)
    X(TessLevelInner)
    X(TessCoord)
    X(PatchVertices)
    X(FragCoord)
    X(PointCoord)
    X(FrontFacing)
    X(SampleId)
    X(SamplePosition)
    X(SampleMask)
    X(FragDepth)
    X(HelperInvocation)
    X(NumWorkgroups)
    X(WorkgroupSize)
    X(WorkgroupId)
    X(LocalInvocationId)
    X(GlobalInvocationId)
    X(LocalInvocationIndex)
    X(WorkDim)
    X(GlobalSize)
    X(EnqueuedWorkgroupSize)
    X(GlobalOffset)
    X(GlobalLinearId)
    X(SubgroupSize)
    X(SubgroupMaxSize)
    X(NumSubgroups)
    X(NumEnqueuedSubgroups)
    X(SubgroupId)
    X(SubgroupLocalInvocationId)
    X(VertexIndex)
    X(InstanceIndex)
    X(SubgroupEqMaskKHR)
    X(SubgroupGeMaskKHR)
    X(SubgroupGtMaskKHR)
    X(SubgroupLeMaskKHR)
    X(SubgroupLtMaskKHR)
    X(BaseVertex)
    X(BaseInstance)
    X(DrawIndex)
    #undef X
    default: throw std::logic_error("bad BuiltIn");
    }
}
