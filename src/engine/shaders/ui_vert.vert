; SPIR-V
; Version: 1.0
; Generator: Khronos SPIR-V Tools Assembler; 0
; Bound: 36
; Schema: 0
               OpCapability Shader
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint Vertex %main "main" %inPosition %inColor %outColor %gl_Position
               OpSource GLSL 450
               OpName %main "main"
               OpName %inPosition "inPosition"
               OpName %inColor "inColor"
               OpName %outColor "outColor"
               OpName %gl_PerVertex "gl_PerVertex"
               OpMemberName %gl_PerVertex 0 "gl_Position"
               OpName %gl_Position "gl_Position"
               OpName %pc "pc"
               OpMemberName %pc 0 "screenSize"
               OpDecorate %inPosition Location 0
               OpDecorate %inColor Location 1
               OpDecorate %outColor Location 0
               OpMemberDecorate %gl_PerVertex 0 BuiltIn Position
               OpDecorate %gl_PerVertex Block
               OpDecorate %pc Block
               OpMemberDecorate %pc 0 Offset 0
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
      %float = OpTypeFloat 32
    %v2float = OpTypeVector %float 2
    %v4float = OpTypeVector %float 4
%_ptr_Input_v2float = OpTypePointer Input %v2float
 %inPosition = OpVariable %_ptr_Input_v2float Input
%_ptr_Input_v4float = OpTypePointer Input %v4float
   %inColor = OpVariable %_ptr_Input_v4float Input
%_ptr_Output_v4float = OpTypePointer Output %v4float
  %outColor = OpVariable %_ptr_Output_v4float Output
%gl_PerVertex = OpTypeStruct %v4float
%_ptr_Output_gl_PerVertex = OpTypePointer Output %gl_PerVertex
%gl_Position = OpVariable %_ptr_Output_gl_PerVertex Output
   %float_2 = OpConstant %float 2
   %float_1 = OpConstant %float 1
   %float_0 = OpConstant %float 0
       %pc = OpTypeStruct %v2float
%_ptr_PushConstant_pc = OpTypePointer PushConstant %pc
   %pc_var = OpVariable %_ptr_PushConstant_pc PushConstant
       %int = OpTypeInt 32 1
     %int_0 = OpConstant %int 0
%_ptr_PushConstant_v2float = OpTypePointer PushConstant %v2float
%_ptr_Output_v2float = OpTypePointer Output %v2float
       %main = OpFunction %void None %3
          %5 = OpLabel
       %pos = OpLoad %v2float %inPosition
       %col = OpLoad %v4float %inColor
               OpStore %outColor %col
  %screen = OpAccessChain %_ptr_PushConstant_v2float %pc_var %int_0
   %ssize = OpLoad %v2float %screen
   %pos_x = OpCompositeExtract %float %pos 0
   %pos_y = OpCompositeExtract %float %pos 1
   %ss_x  = OpCompositeExtract %float %ssize 0
   %ss_y  = OpCompositeExtract %float %ssize 1
   %ndc_x = OpFDiv %float %pos_x %ss_x
   %ndc_x2 = OpFMul %float %ndc_x %float_2
   %ndc_x3 = OpFSub %float %ndc_x2 %float_1
   %ndc_y = OpFDiv %float %pos_y %ss_y
   %ndc_y2 = OpFMul %float %ndc_y %float_2
   %ndc_y3 = OpFSub %float %float_1 %ndc_y2
  %result = OpCompositeConstruct %v4float %ndc_x3 %ndc_y3 %float_0 %float_1
  %out_ptr = OpAccessChain %_ptr_Output_v4float %gl_Position %int_0
               OpStore %out_ptr %result
               OpReturn
               OpFunctionEnd
