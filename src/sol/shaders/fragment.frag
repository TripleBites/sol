; SPIR-V
; Version: 1.0
; Generator: Khronos SPIR-V Tools Assembler; 0
; Bound: 14
; Schema: 0
               OpCapability Shader
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint Fragment %main "main" %fragColor %outColor
               OpExecutionMode %main OriginUpperLeft
               OpSource GLSL 450
               OpName %main "main"
               OpName %fragColor "fragColor"
               OpName %outColor "outColor"
               OpDecorate %fragColor Location 0
               OpDecorate %outColor Location 0
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
      %float = OpTypeFloat 32
    %v3float = OpTypeVector %float 3
    %v4float = OpTypeVector %float 4
%_ptr_Input_v3float = OpTypePointer Input %v3float
  %fragColor = OpVariable %_ptr_Input_v3float Input
%_ptr_Output_v4float = OpTypePointer Output %v4float
   %outColor = OpVariable %_ptr_Output_v4float Output
   %float_1 = OpConstant %float 1
       %main = OpFunction %void None %3
          %5 = OpLabel
      %fc = OpLoad %v3float %fragColor
     %out = OpCompositeExtract %float %fc 0
    %out1 = OpCompositeExtract %float %fc 1
    %out2 = OpCompositeExtract %float %fc 2
  %result = OpCompositeConstruct %v4float %out %out1 %out2 %float_1
               OpStore %outColor %result
               OpReturn
               OpFunctionEnd
