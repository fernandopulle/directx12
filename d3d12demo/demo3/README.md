

You need to compile the shaders locally using fxc.exe. 

Eg: 
Vertex Shader
 C:\Program Files (x86)\Windows Kits\10\bin\10.0.17134.0\x64\fxc.exe  /T vs_5_1 /Fo VertexShader.cso VertexShader.hlsl
 
Pixel Shader
 C:\Program Files (x86)\Windows Kits\10\bin\10.0.17134.0\x64\fxc.exe  /T ps_5_1 /Fo PixelShader.cso PixelShader.hlsl
