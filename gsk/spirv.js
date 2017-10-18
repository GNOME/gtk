function usage() 
{
  print ("usage: gjs spirv.js enums|functions SPIRV_GRAMMAR_FILE");
}

if (ARGV.length != 2)
  {
    usage();
    throw new SyntaxError ("Script needs 2 arguments but got " + ARGV.length);
  }

var command = ARGV[0];
var extinst = "";
if (ARGV[0].indexOf("-") > 0)
  {
    extinst = ARGV[0].substr (0, ARGV[0].indexOf("-"));
    command = ARGV[0].substr (extinst.length + 1);
  }

var contents = imports.gi.Gio.File.new_for_path(ARGV[1]).load_contents(null);
var spirv = JSON.parse(contents[1]);

if (!String.prototype.format) {
  String.prototype.format = function() {
    var args = arguments;
    return this.replace(/{(\d+)}/g, function(match, number) {
      return typeof args[number] != 'undefined'
        ? args[number]
        : match
      ;
    });
  };
}

function all_lower(s)
{
  let result = "";
  let needs_underscore = false;
  let had_caps = false;
  for (let i = 0; i < s.length; i++)
    {
      if (s[i] >= 'a' && s[i] <= 'z')
        {
          needs_underscore = true;
          had_caps = false;
          result += s[i];
        }
      else if (s[i] >= "A" && s[i] <= "Z")
        {
          if (needs_underscore)
            result += "_";
          else if (s[i+1] && s[i+1] >= "a" && s[i+1] <= "z" && had_caps)
            result += "_";
          needs_underscore = false;
          had_caps = true;
          result += s[i].toLowerCase();
        }
      else if (s[i] >= '0' && s[i] <= '9')
        {
          needs_underscore = true
          had_caps = false;
          result += s[i]
        }
      else
        {
          needs_underscore = false
          had_caps = false;
          result += "_";
        }
    }
  return result;
}

function all_upper(s)
{
  return all_lower(s).toUpperCase();
}

function sanitize_name (name)
{
  name = name.substr(1);
  return all_lower(name.substr(0, name.indexOf("'")));
}

var SpecialTypes = {
  "OpVariable": { "result_type": "IdResultPointerType" },
  "OpImageTexelPointer": { "result_type": "IdResultPointerType" },
  "OpAccessChain": { "result_type": "IdResultPointerType" },
  "OpInBoundsAccessChain": { "result_type": "IdResultPointerType" },
  "OpConvertUToPtr": { "result_type": "IdResultPointerType" },
  "OpPtrCastToGeneric": { "result_type": "IdResultPointerType" },
  "OpGenericCastToPtr": { "result_type": "IdResultPointerType" },
  "OpGenericCastToPtrExplicit": { "result_type": "IdResultPointerType" },
  "OpFunctionParameter": { "result_type": "IdRef" },
  "OpLabel": { "result": "IdRef" }
};

var ExtraOperands = {
  "OpDecorate": [ { "kind" : "LiteralContextDependentNumber", "name" : "'Value'" } ],
  "OpMemberDecorate": [ { "kind" : "LiteralContextDependentNumber", "name" : "'Value'" } ]
};

/* maps opcodes to section in file they appear in */
var Sections = {
  "OpNop": "",
  "OpUndef": "",
  "OpSourceContinued": "",
  "OpSource": "debug",
  "OpSourceExtension": "debug",
  "OpName": "debug",
  "OpMemberName": "debug",
  "OpString": "",
  "OpLine": "",
  "OpExtension": "header",
  "OpExtInstImport": "header",
  "OpExtInst": "",
  "OpMemoryModel": "header",
  "OpEntryPoint": "header",
  "OpExecutionMode": "header",
  "OpCapability": "header",
  "OpTypeVoid": "define",
  "OpTypeBool": "define",
  "OpTypeInt": "define",
  "OpTypeFloat": "define",
  "OpTypeVector": "define",
  "OpTypeMatrix": "define",
  "OpTypeImage": "define",
  "OpTypeSampler": "define",
  "OpTypeSampledImage": "define",
  "OpTypeArray": "define",
  "OpTypeRuntimeArray": "define",
  "OpTypeStruct": "define",
  "OpTypeOpaque": "define",
  "OpTypePointer": "define",
  "OpTypeFunction": "define",
  "OpTypeEvent": "define",
  "OpTypeDeviceEvent": "define",
  "OpTypeReserveId": "define",
  "OpTypeQueue": "define",
  "OpTypePipe": "define",
  "OpTypeForwardPointer": "define",
  "OpConstantTrue": "define",
  "OpConstantFalse": "define",
  "OpConstant": "define",
  "OpConstantComposite": "define",
  "OpConstantSampler": "define",
  "OpConstantNull": "define",
  "OpSpecConstantTrue": "define",
  "OpSpecConstantFalse": "define",
  "OpSpecConstant": "define",
  "OpSpecConstantComposite": "define",
  "OpSpecConstantOp": "define",
  "OpFunction": "declare",
  "OpFunctionParameter": "declare",
  "OpFunctionEnd": "code",
  "OpFunctionCall": "code",
  "OpVariable": "",
  "OpImageTexelPointer": "code",
  "OpLoad": "code",
  "OpStore": "code",
  "OpCopyMemory": "code",
  "OpCopyMemorySized": "code",
  "OpAccessChain": "code",
  "OpInBoundsAccessChain": "code",
  "OpPtrAccessChain": "code",
  "OpArrayLength": "code",
  "OpGenericPtrMemSemantics": "",
  "OpInBoundsPtrAccessChain": "code",
  "OpDecorate": "decorate",
  "OpMemberDecorate": "decorate",
  "OpDecorationGroup": "decorate",
  "OpGroupDecorate": "decorate",
  "OpGroupMemberDecorate": "decorate",
  "OpVectorExtractDynamic": "code",
  "OpVectorInsertDynamic": "code",
  "OpVectorShuffle": "code",
  "OpCompositeConstruct": "code",
  "OpCompositeExtract": "code",
  "OpCompositeInsert": "code",
  "OpCopyObject": "",
  "OpTranspose": "code",
  "OpSampledImage": "",
  "OpImageSampleImplicitLod": "code",
  "OpImageSampleExplicitLod": "code",
  "OpImageSampleDrefImplicitLod": "code",
  "OpImageSampleDrefExplicitLod": "code",
  "OpImageSampleProjImplicitLod": "code",
  "OpImageSampleProjExplicitLod": "code",
  "OpImageSampleProjDrefImplicitLod": "code",
  "OpImageSampleProjDrefExplicitLod": "code",
  "OpImageFetch": "code",
  "OpImageGather": "code",
  "OpImageDrefGather": "code",
  "OpImageRead": "code",
  "OpImageWrite": "code",
  "OpImage": "code",
  "OpImageQueryFormat": "code",
  "OpImageQueryOrder": "code",
  "OpImageQuerySizeLod": "code",
  "OpImageQuerySize": "code",
  "OpImageQueryLod": "code",
  "OpImageQueryLevels": "code",
  "OpImageQuerySamples": "code",
  "OpConvertFToU": "code",
  "OpConvertFToS": "code",
  "OpConvertSToF": "code",
  "OpConvertUToF": "code",
  "OpUConvert": "code",
  "OpSConvert": "code",
  "OpFConvert": "code",
  "OpQuantizeToF16": "code",
  "OpConvertPtrToU": "code",
  "OpSatConvertSToU": "code",
  "OpSatConvertUToS": "code",
  "OpConvertUToPtr": "code",
  "OpPtrCastToGeneric": "code",
  "OpGenericCastToPtr": "code",
  "OpGenericCastToPtrExplicit": "code",
  "OpBitcast": "code",
  "OpSNegate": "code",
  "OpFNegate": "code",
  "OpIAdd": "code",
  "OpFAdd": "code",
  "OpISub": "code",
  "OpFSub": "code",
  "OpIMul": "code",
  "OpFMul": "code",
  "OpUDiv": "code",
  "OpSDiv": "code",
  "OpFDiv": "code",
  "OpUMod": "code",
  "OpSRem": "code",
  "OpSMod": "code",
  "OpFRem": "code",
  "OpFMod": "code",
  "OpVectorTimesScalar": "code",
  "OpMatrixTimesScalar": "code",
  "OpVectorTimesMatrix": "code",
  "OpMatrixTimesVector": "code",
  "OpMatrixTimesMatrix": "code",
  "OpOuterProduct": "code",
  "OpDot": "code",
  "OpIAddCarry": "code",
  "OpISubBorrow": "code",
  "OpUMulExtended": "code",
  "OpSMulExtended": "code",
  "OpAny": "code",
  "OpAll": "code",
  "OpIsNan": "code",
  "OpIsInf": "code",
  "OpIsFinite": "code",
  "OpIsNormal": "code",
  "OpSignBitSet": "code",
  "OpLessOrGreater": "code",
  "OpOrdered": "code",
  "OpUnordered": "code",
  "OpLogicalEqual": "code",
  "OpLogicalNotEqual": "code",
  "OpLogicalOr": "code",
  "OpLogicalAnd": "code",
  "OpLogicalNot": "code",
  "OpSelect": "code",
  "OpIEqual": "code",
  "OpINotEqual": "code",
  "OpUGreaterThan": "code",
  "OpSGreaterThan": "code",
  "OpUGreaterThanEqual": "code",
  "OpSGreaterThanEqual": "code",
  "OpULessThan": "code",
  "OpSLessThan": "code",
  "OpULessThanEqual": "code",
  "OpSLessThanEqual": "code",
  "OpFOrdEqual": "code",
  "OpFUnordEqual": "code",
  "OpFOrdNotEqual": "code",
  "OpFUnordNotEqual": "code",
  "OpFOrdLessThan": "code",
  "OpFUnordLessThan": "code",
  "OpFOrdGreaterThan": "code",
  "OpFUnordGreaterThan": "code",
  "OpFOrdLessThanEqual": "code",
  "OpFUnordLessThanEqual": "code",
  "OpFOrdGreaterThanEqual": "code",
  "OpFUnordGreaterThanEqual": "code",
  "OpShiftRightLogical": "code",
  "OpShiftRightArithmetic": "code",
  "OpShiftLeftLogical": "code",
  "OpBitwiseOr": "code",
  "OpBitwiseXor": "code",
  "OpBitwiseAnd": "code",
  "OpNot": "code",
  "OpBitFieldInsert": "code",
  "OpBitFieldSExtract": "code",
  "OpBitFieldUExtract": "code",
  "OpBitReverse": "code",
  "OpBitCount": "code",
  "OpDPdx": "code",
  "OpDPdy": "code",
  "OpFwidth": "code",
  "OpDPdxFine": "code",
  "OpDPdyFine": "code",
  "OpFwidthFine": "code",
  "OpDPdxCoarse": "code",
  "OpDPdyCoarse": "code",
  "OpFwidthCoarse": "code",
  "OpEmitVertex": "code",
  "OpEndPrimitive": "code",
  "OpEmitStreamVertex": "code",
  "OpEndStreamPrimitive": "code",
  "OpControlBarrier": "code",
  "OpMemoryBarrier": "code",
  "OpAtomicLoad": "code",
  "OpAtomicStore": "code",
  "OpAtomicExchange": "code",
  "OpAtomicCompareExchange": "code",
  "OpAtomicCompareExchangeWeak": "code",
  "OpAtomicIIncrement": "code",
  "OpAtomicIDecrement": "code",
  "OpAtomicIAdd": "code",
  "OpAtomicISub": "code",
  "OpAtomicSMin": "code",
  "OpAtomicUMin": "code",
  "OpAtomicSMax": "code",
  "OpAtomicUMax": "code",
  "OpAtomicAnd": "code",
  "OpAtomicOr": "code",
  "OpAtomicXor": "code",
  "OpPhi": "code",
  "OpLoopMerge": "code",
  "OpSelectionMerge": "code",
  "OpLabel": "",
  "OpBranch": "code",
  "OpBranchConditional": "code",
  "OpSwitch": "code",
  "OpKill": "code",
  "OpReturn": "code",
  "OpReturnValue": "code",
  "OpUnreachable": "",
  "OpLifetimeStart": "",
  "OpLifetimeStop": "",
  "OpGroupAsyncCopy": "",
  "OpGroupWaitEvents": "",
  "OpGroupAll": "",
  "OpGroupAny": "",
  "OpGroupBroadcast": "",
  "OpGroupIAdd": "",
  "OpGroupFAdd": "",
  "OpGroupFMin": "",
  "OpGroupUMin": "",
  "OpGroupSMin": "",
  "OpGroupFMax": "",
  "OpGroupUMax": "",
  "OpGroupSMax": "",
  "OpReadPipe": "",
  "OpWritePipe": "",
  "OpReservedReadPipe": "",
  "OpReservedWritePipe": "",
  "OpReserveReadPipePackets": "",
  "OpReserveWritePipePackets": "",
  "OpCommitReadPipe": "",
  "OpCommitWritePipe": "",
  "OpIsValidReserveId": "",
  "OpGetNumPipePackets": "",
  "OpGetMaxPipePackets": "",
  "OpGroupReserveReadPipePackets": "",
  "OpGroupReserveWritePipePackets": "",
  "OpGroupCommitReadPipe": "",
  "OpGroupCommitWritePipe": "",
  "OpEnqueueMarker": "",
  "OpEnqueueKernel": "",
  "OpGetKernelNDrangeSubGroupCount": "",
  "OpGetKernelNDrangeMaxSubGroupSize": "",
  "OpGetKernelWorkGroupSize": "",
  "OpGetKernelPreferredWorkGroupSizeMultiple": "",
  "OpRetainEvent": "",
  "OpReleaseEvent": "",
  "OpCreateUserEvent": "",
  "OpIsValidEvent": "",
  "OpSetUserEventStatus": "",
  "OpCaptureEventProfilingInfo": "",
  "OpGetDefaultQueue": "",
  "OpBuildNDRange": "",
  "OpImageSparseSampleImplicitLod": "",
  "OpImageSparseSampleExplicitLod": "",
  "OpImageSparseSampleDrefImplicitLod": "",
  "OpImageSparseSampleDrefExplicitLod": "",
  "OpImageSparseSampleProjImplicitLod": "",
  "OpImageSparseSampleProjExplicitLod": "",
  "OpImageSparseSampleProjDrefImplicitLod": "",
  "OpImageSparseSampleProjDrefExplicitLod": "",
  "OpImageSparseFetch": "",
  "OpImageSparseGather": "",
  "OpImageSparseDrefGather": "",
  "OpImageSparseTexelsResident": "",
  "OpNoLine": "",
  "OpAtomicFlagTestAndSet": "",
  "OpAtomicFlagClear": "",
  "OpImageSparseRead": "",
  "OpSubgroupBallotKHR": "",
  "OpSubgroupFirstInvocationKHR": "",
  "OpSubgroupAllKHR": "",
  "OpSubgroupAnyKHR": "",
  "OpSubgroupAllEqualKHR": "",
  "OpSubgroupReadInvocationKHR": "",
  "OpGroupIAddNonUniformAMD": "",
  "OpGroupFAddNonUniformAMD": "",
  "OpGroupFMinNonUniformAMD": "",
  "OpGroupUMinNonUniformAMD": "",
  "OpGroupSMinNonUniformAMD": "",
  "OpGroupFMaxNonUniformAMD": "",
  "OpGroupUMaxNonUniformAMD": "",
  "OpGroupSMaxNonUniformAMD": "",
  "OpFragmentMaskFetchAMD": "",
  "OpFragmentFetchAMD": ""
};

var Operands = {
  "IdMemorySemantics": { ctype: "guint32 {0}",
                         optional_unset: "0",
                         append_many: "g_array_append_vals ({0}, {1}, {2})",
                         append_one: "g_array_append_val ({0}, {1})" },
  "IdRef": { ctype: "guint32 {0}",
             optional_unset: "0",
             append_many: "g_array_append_vals ({0}, {1}, {2})",
             append_one: "g_array_append_val ({0}, {1})" },
  "IdResult": { ctype: "guint32 {0}",
                optional_unset: "0",
                declare_local: "guint32 {0}_id = gsk_spv_writer_make_id (writer);",
                append_one: "g_array_append_val ({0}, {1}_id)" },
  "IdResultType": { ctype: "GskSlType *{0}",
                    optional_unset: "NULL",
                    declare_local: "guint32 {0}_id = gsk_spv_writer_get_id_for_type (writer, {0});",
                    append_one: "g_array_append_val ({0}, {1}_id)" },
  "IdResultPointerType" : { ctype: "GskSlType *{0}, GskSpvStorageClass {0}_storage",
                            optional_unset: "NULL",
                            declare_local: "guint32 {0}_id = gsk_spv_writer_get_id_for_pointer_type (writer, {0}, {0}_storage);",
                            append_one: "g_array_append_val ({0}, {1}_id)" },
  "IdScope": { ctype: "guint32 {0}",
               optional_unset: "0",
               append_many: "g_array_append_vals ({0}, {1}, {2})",
               append_one: "g_array_append_val ({0}, {1})" },
  "LiteralContextDependentNumber": { ctype: "guint32 {0}",
                                     is_many: true,
                                     append_many: "g_array_append_vals ({0}, {1}, {2})" },
  "LiteralExtInstInteger": { ctype: "guint32 {0}",
                             append_many: "g_array_append_vals ({0}, {1}, {2})",
                             append_one: "g_array_append_val ({0}, {1})" },
  "LiteralInteger": { ctype: "guint32 {0}",
                      append_many: "g_array_append_vals ({0}, {1}, {2})",
                      append_one: "g_array_append_val ({0}, {1})" },
  "LiteralString": { ctype: "const char *{0}",
                     optional_unset: "NULL",
                     append_one: "append_string ({0}, {1})" },
  "LiteralSpecConstantOpInteger": { ctype: "guint32 {0}",
                                    is_many: true,
                                    append_many: "g_array_append_vals ({0}, {1}, {2})" },
  "PairIdRefLiteralInteger": { ctype: "guint32 {0}[2]",
                               append_many: "g_array_append_vals ({0}, {1}, 2 * {2})",
                               append_one: "g_array_append_vals ({0}, {1}, 2)" },
  "PairIdRefIdRef": { ctype: "guint32 {0}[2]",
                      append_many: "g_array_append_vals ({0}, {1}, 2 * {2})",
                      append_one: "g_array_append_vals ({0}, {1}, 2)" },
  "PairLiteralIntegerIdRef": { ctype: "guint32 {0}[2]",
                               append_many: "g_array_append_vals ({0}, {1}, 2 * {2})",
                               append_one: "g_array_append_vals ({0}, {1}, 2)" }
};

for (let kind in spirv.operand_kinds)
  {
    kind = spirv.operand_kinds[kind];
    if (kind.category == "BitEnum" ||
        kind.category == "ValueEnum")
      {
        Operands[kind.kind] = { ctype: "GskSpv" + kind.kind + " {0}",
                                append_one: "g_array_append_val ({0}, (guint32) { {1} })" };
        if (kind.category == "BitEnum")
          Operands[kind.kind].optional_unset = "0";
        if (kind.kind == "AccessQualifier")
          Operands[kind.kind].optional_unset = "-1";
      }
  }

function fix_operand (ins, o)
{
  if (o.name)
    {
      if (o.name == "The name of the opaque type.")
        o.varname = "name"
      else
      o.varname = sanitize_name (o.name);
    }
  else
    {
      if (o.kind == "IdResultType")
        o.varname = "result_type"
      else if (o.kind == "IdResult")
        o.varname = "result"
      else if (Operands[o.kind])
        o.varname = all_lower (o.kind);
    }
  if (!o.varname)
    throw new TypeError (o.name + " of type " + o.kind + " has no variable name");
  let operand;
  if (SpecialTypes[ins.opname] &&
      SpecialTypes[ins.opname][o.varname])
    o.kind = SpecialTypes[ins.opname][o.varname];
  operand = Operands[o.kind];

  if (o.varname == "default")
    o.varname += "_";
  if (o.kind == "IdResult")
    ins.result = true;

  o.ctype = operand.ctype;
  if (operand.append_one)
    o.append_one = operand.append_one;
  if (operand.append_many)
    o.append_many = operand.append_many;
  if (operand.declare_local)
    o.declare_local = operand.declare_local;
  if (operand.is_many)
    {
      if (o.quantifier)
        throw new SyntaxError ("Can't deal with lists of " + o.kind);
      else
        o.quantifier = "*";
    }
  if (!o.quantifier)
    o.quantifier = "";
  if (o.quantifier == "?")
    {
      o.varname = "opt_" + o.varname;
      if (operand.optional_unset)
        o.unset = operand.optional_unset;
    }
  if (o.quantifier == "*")
    {
      if (o.varname[o.varname.length - 1] == "1")
        o.varname = o.varname.substr(0, o.varname.length - 2);
      if (o.varname[o.varname.length - 1] != "s")
        o.varname += "s";
    }
}

for (let i in spirv.instructions)
  {
    let ins = spirv.instructions[i];
    ins.result = false;
    ins.enum_value = "GSK_SPV_OP_" + (extinst ? all_upper(extinst) + "_" + all_upper (ins.opname) : all_upper (ins.opname.substr(2)));
    if (!ins.operands)
      ins.operands = [];
    if (ExtraOperands[ins.opname])
      ins.operands = ins.operands.concat (ExtraOperands[ins.opname]);
    if (extinst)
      {
        ins.operands.unshift (
          { "kind" : "IdResultType" },
          { "kind" : "IdResult" }
        );
      }
    for (let o in ins.operands)
      {
        o = ins.operands[o];
        fix_operand (ins, o);
      }
    if (extinst)
      {
        ins.section = "code";
        ins.section_enum = "GSK_SPV_WRITER_SECTION_CODE";
      }
    else if (Sections[ins.opname])
      {
        ins.section = Sections[ins.opname];
        ins.section_enum = "GSK_SPV_WRITER_SECTION_" + ins.section.toUpperCase();
      }
    if (!extinst)
      ins.opname = ins.opname.substr(2);
  }

function header()
{
  print ("/* GTK - The GIMP Toolkit");
  print (" *   ");
  print (" * Copyright © 2017 Benjamin Otte <otte@gnome.org>");
  print (" *");
  print (" * This library is free software; you can redistribute it and/or");
  print (" * modify it under the terms of the GNU Lesser General Public");
  print (" * License as published by the Free Software Foundation; either");
  print (" * version 2 of the License, or (at your option) any later version.");
  print (" *");
  print (" * This library is distributed in the hope that it will be useful,");
  print (" * but WITHOUT ANY WARRANTY; without even the implied warranty of");
  print (" * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU");
  print (" * Lesser General Public License for more details.");
  print (" *");
  print (" * You should have received a copy of the GNU Lesser General Public");
  print (" * License along with this library. If not, see <http://www.gnu.org/licenses/>.");
  print (" */");
  print ("");
  print ("/*");
  print (" * !!! THIS FILE WAS AUTOGENERATED !!!");
  print (" * ");
  print (" * This file was created using the command");
  print (" *   gjs spirv.js " + ARGV.join (" "));
  print (" * Apply any changes to those files and then regenerate using above command.");
  print (" */");
  print ();
}

if (command == "enums")
  {
    header ();
    print ("#ifndef __GSK_SPV_" + (extinst ? all_upper (extinst) + "_" : "") + "ENUMS_H__");
    print ("#define __GSK_SPV_" + (extinst ? all_upper (extinst) + "_" : "") + "ENUMS_H__");
    print ();

    for (let kind in spirv.operand_kinds)
      {
        kind = spirv.operand_kinds[kind];
        if (kind.category == "BitEnum" ||
            kind.category == "ValueEnum")
          {
            print ("typedef enum {");
            for (let i = 0; i < kind.enumerants.length; i++)
              {
                let e = kind.enumerants[i];
                //print (Object.keys(e));
                print ("  GSK_SPV_" + all_upper(kind.kind) + "_" + all_upper (e.enumerant) + " = " + e.value + (i + 1 < kind.enumerants.length ? "," : ""));
              }
            print ("} GskSpv" + kind.kind + ";");
            print ();
          }
      }

    print ("typedef enum {");
    for (let i =0; i < spirv.instructions.length; i++)
      {
        let ins = spirv.instructions[i];
        print ("  " + ins.enum_value + " = " + ins.opcode + (i + 1 < spirv.instructions.length ? "," : ""));
      }
    print ("} GskSpvOpcode" + extinst + ";");
    print ();
    print ("#endif /* __GSK_SPV_" + (extinst ? all_upper (extinst) + "_" : "") + "ENUMS_H__ */");
  }
else if (command == "functions")
  {
    header ();
    print ("#include <string.h>");
    print ();
    print ("#ifndef __GSK_SPV_SEEN_AUTOGENERATED_FUNCTIONS__");
    print ("#define __GSK_SPV_SEEN_AUTOGENERATED_FUNCTIONS__");
    print ("static inline void");
    print ("append_string (GArray     *bytes,");
    print ("               const char *str)");
    print ("{");
    print ("  gsize len = strlen (str);");
    print ("  guint size = bytes->len;");
    print ("  g_array_set_size (bytes, size + len / 4 + 1);");
    print ("  memcpy (&g_array_index (bytes, guint32, size), str, len);");
    print ("}");
    print ("#endif /* __GSK_SPV_SEEN_AUTOGENERATED_FUNCTIONS__ */");
    print ();
    for (let i in spirv.instructions)
      {
        let ins = spirv.instructions[i];
        let prefix = "gsk_spv_writer_" + all_lower(ins.opname) + " (";
        let len = prefix.length;
        if (ins.result)
          print ("static inline guint32");
        else
          print ("static inline void");
        if (ins.operands.length > 1 ||
            ins.operands.length == 1 && !ins.result)
          {
            let seen_result = !ins.result;
            print (prefix + "GskSpvWriter *writer,");
            if (!ins.section)
              print (Array(len+1).join(" ") + "GskSpvWriterSection section,");
            for (let i = 0; i < ins.operands.length; i++)
              {
                let o = ins.operands[i];
                if (o.kind == "IdResult")
                  {
                    seen_result = true;
                    continue;
                  }
                if (o.quantifier == "*")
                  {
                    print (Array(len+1).join(" ") + o.ctype.format ("*" + o.varname) + ",");
                    print (Array(len+1).join(" ") + "gsize n_" + o.varname + (i + 2 - seen_result < ins.operands.length ? "," : ")"));
                  }
                else
                  {
                    print (Array(len+1).join(" ") + o.ctype.format (o.varname) + (i + 2 - seen_result < ins.operands.length ? "," : ")"));
                  }
              }
          }
        else
          {
            if (ins.section)
              print ("gsk_spv_writer_" + all_lower(ins.opname) + " (GskSpvWriter *writer)");
            else
              {
                print ("gsk_spv_writer_" + all_lower(ins.opname) + " (GskSpvWriter *writer,");
                print (Array(len+1).join(" ") + "GskSpvWriterSection section)");
              }
          }
        print ("{");
        print ("  GArray *bytes = gsk_spv_writer_get_bytes (writer, " + (ins.section_enum ? ins.section_enum : "section" ) + ");");
        for (let i = 0; i < ins.operands.length; i++)
          {
            let o = ins.operands[i];
            if (o.declare_local)
              print ("  " + o.declare_local.format (o.varname));
          }
        print ("  guint start_index = bytes->len;");
        print ("");
        print ("  g_array_append_val (bytes, (guint32) { 0 });");
        for (let i = 0; i < ins.operands.length; i++)
          {
            let o = ins.operands[i];
            let indent = "  ";
            if (o.unset)
              {
                print (indent + "if (" + o.varname + " != " + o.unset + ")");
                indent += "  ";
              }
            if (o.quantifier == "*")
              print (indent + o.append_many.format ("bytes", o.varname, "n_" + o.varname) + ";");
            else
              print (indent + o.append_one.format ("bytes", o.varname) + ";");
            if (i == 1 && extinst)
              {
                print ("  g_array_append_val (bytes, (guint32) { gsk_spv_writer_get_id_for_extended_instructions (writer) });");
                print ("  g_array_append_val (bytes, (guint32) { " + ins.enum_value + " });");
              }
          }
        print ("  g_array_index (bytes, guint32, start_index) = (bytes->len - start_index) << 16 | " + (extinst ? "GSK_SPV_OP_EXT_INST" : ins.enum_value) + ";");
        if (ins.result)
          {
            print ("");
            print ("  return result_id;");
          }
        print ("}");
        print ("");
      }
  }
else
  {
    usage ()
  }
