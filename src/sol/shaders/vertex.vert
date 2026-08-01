; SPIR-V
; Version: 1.0
; Generator: Khronos SPIR-V Tools Assembler; 0
; Bound: 48
; Schema: 0
               OpCapability Shader
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint Vertex %main "main" %fragColor %_ %gl_VertexIndex
               OpSource GLSL 450
               OpName %main "main"
               OpName %fragColor "fragColor"
               OpName %gl_PerVertex "gl_PerVertex"
               OpMemberName %gl_PerVertex 0 "gl_Position"
               OpName %_ ""
               OpDecorate %fragColor Location 0
               OpDecorate %gl_VertexIndex BuiltIn VertexIndex
               OpDecorate %gl_PerVertex Block
               OpMemberDecorate %gl_PerVertex 0 BuiltIn Position
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
      %float = OpTypeFloat 32
    %v3float = OpTypeVector %float 3
    %v4float = OpTypeVector %float 4
%_ptr_Output_v3float = OpTypePointer Output %v3float
  %fragColor = OpVariable %_ptr_Output_v3float Output
%gl_PerVertex = OpTypeStruct %v4float
%_ptr_Output_gl_PerVertex = OpTypePointer Output %gl_PerVertex
          %_ = OpVariable %_ptr_Output_gl_PerVertex Output
       %uint = OpTypeInt 32 0
%_ptr_Input_uint = OpTypePointer Input %uint
%gl_VertexIndex = OpVariable %_ptr_Input_uint Input
     %uint_0 = OpConstant %uint 0
     %uint_1 = OpConstant %uint 1
%float_n0_5 = OpConstant %float -0.5
   %float_0 = OpConstant %float 0
  %float_0_5 = OpConstant %float 0.5
   %float_1 = OpConstant %float 1
    %pos_0 = OpConstantComposite %v4float %float_0 %float_n0_5 %float_0 %float_1
    %pos_1 = OpConstantComposite %v4float %float_0_5 %float_0_5 %float_0 %float_1
    %pos_2 = OpConstantComposite %v4float %float_n0_5 %float_0_5 %float_0 %float_1
    %col_0 = OpConstantComposite %v3float %float_1 %float_0 %float_0
    %col_1 = OpConstantComposite %v3float %float_0 %float_1 %float_0
    %col_2 = OpConstantComposite %v3float %float_0 %float_0 %float_1
     %bool = OpTypeBool
%_ptr_Output_v4float = OpTypePointer Output %v4float
       %main = OpFunction %void None %3
          %5 = OpLabel
     %vidx = OpLoad %uint %gl_VertexIndex
    %is_0 = OpIEqual %bool %vidx %uint_0
               OpSelectionMerge %merge0 None
               OpBranchConditional %is_0 %block0 %check1
   %block0 = OpLabel
               OpStore %fragColor %col_0
 %elem_ptr0 = OpAccessChain %_ptr_Output_v4float %_ %uint_0
               OpStore %elem_ptr0 %pos_0
               OpBranch %merge0
   %check1 = OpLabel
    %is_1 = OpIEqual %bool %vidx %uint_1
               OpSelectionMerge %inner_merge None
               OpBranchConditional %is_1 %block1 %block2
   %block1 = OpLabel
               OpStore %fragColor %col_1
 %elem_ptr1 = OpAccessChain %_ptr_Output_v4float %_ %uint_0
               OpStore %elem_ptr1 %pos_1
               OpBranch %inner_merge
   %block2 = OpLabel
               OpStore %fragColor %col_2
 %elem_ptr2 = OpAccessChain %_ptr_Output_v4float %_ %uint_0
               OpStore %elem_ptr2 %pos_2
               OpBranch %inner_merge
%inner_merge = OpLabel
               OpBranch %merge0
  %merge0 = OpLabel
               OpReturn
               OpFunctionEnd
