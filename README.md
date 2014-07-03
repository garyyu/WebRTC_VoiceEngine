Name: WebRTC_VoiceEngine
========================
URL: https://github.com/garyyu/WebRTC_VoiceEngine
License: BSD
License File: LICENSE
For WebRTC License & Patents information, please read files LICENSE.webrtc and PATENTS.webrtc.

Description:
This WebRTC VoiceEngine includes Acoustic Echo Cancellation (AEC), Noise Suppression (NS), VAD (Voice Active Detection) and so on.
The purpose of this project is to make use of Google WebRTC OpenSource project, to provide a simple wrapper API for the voice engine part of WebRTC.
So far, this project is built in Win7 with Visual Studio 2010.

Default, this project is built as DLL.
If want to build demo_main.cpp as standalone demo, you can modify the Project Property: 
	(1) Linker->Input: ignore default library "libcmt"; 
	(2) General->Configuration Type: change "DLL" as "exe"; and "User MFC" as "Static Library".
And modify "webrtc_voe.h" to comment this line: 
	//#define _WEBRTC_API_EXPORTS	// For DLL Building.
Both Release Build and Debug Build is OK.

This project depends on WebRTC project, which I put on my folder "C:\Users\garyyu\Work\trunk". If you put "trunk" on different folder, please modify the external library and header files path in Visual Studio IDE. About how to download and build this WebRTC project, please read the following documents:
	http://www.webrtc.org/reference/getting-started
	http://www.webrtc.org/reference/getting-started/prerequisite-sw





