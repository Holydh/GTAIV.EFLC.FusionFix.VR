// vr_manager.ixx
module;

#include <common.hxx>

export module VR.Manager;

import common;
import VR.Log;
import VR.DXVKInterop;
import comvars;
import natives;

namespace
{
    void VRMod_EarlyInit()
    {
        VRLog_Init();
        LogInfo("VR mod loading (early init)...");
    }

    void VRMod_Shutdown()
    {
        LogInfo("VR mod shutting down");
        VRLog_Shutdown();
    }

    class VRCameraHook
    {
    public:
        static inline safetyhook::InlineHook hook;
        static inline bool bInstalled = false;
        
        static void __fastcall HookFn(rage::Matrix44* mat, void* edx, void* arg2)
        {
            // Call original via trampoline
            hook.unsafe_fastcall(mat, edx, arg2);

            if (!mat)
                return;

            // TEST: alternate camera offset every frame
            static bool toggle = false;
            toggle = !toggle;

            float offset = toggle ? -0.5f : 0.5f;
            mat->pos += mat->right * offset;
        }

        static void Install()
        {
            if (bInstalled)
                return;

            auto pattern = hook::pattern(
                "E8 ? ? ? ? 8A 86 ? ? ? ? 80 A6 ? ? ? ? ? 80 A6"
            );

            if (pattern.empty())
            {
                LogError("VR: camera hook pattern not found");
                return;
            }

            auto callSite = pattern.get_first(0);
            auto target = (void*)injector::GetBranchDestination(callSite).as_int();

            if (!target)
            {
                LogError("VR: failed to resolve camera copy target");
                return;
            }

            hook = safetyhook::create_inline(target, HookFn);

            if (!hook)
            {
                LogError("VR: safetyhook::create_inline failed");
                return;
            }

            LogInfo("VR: hooked global camera copy function at %p", target);
            bInstalled = true;
        }
    };

    struct VRBootstrap
    {
        VRBootstrap()
        {
            FusionFix::onInitEvent() += []()
            {
                VRMod_EarlyInit();
            };

 //           FusionFix::onGameInitEvent() += []()
 //           {
 //               //VRCameraHook::Install();
 //               //Natives::EnableDebugCam(true);
 //   //            int newCam = 99999;
	//			//int viewport;
 //   //            Camera camObj;
	///*			Natives::CreateCam(newCam, &camObj);
 //               Natives::GetGameViewportId(&viewport);
	//			Natives::AttachCamToViewport(newCam, viewport);
	//			Natives::SetCamActive(newCam, true);*/
 //   //            int CurrentCam;
	//			//Natives::GetRootCam(&CurrentCam);
	//			//Natives::CloneCam(CurrentCam, &CloneCam);
 //   //            Natives::SetCamActive(CurrentCam, false);
	//			//int viewport;
	//			//Natives::AttachCamToViewport(CloneCam, viewport);
 //   //            Natives::SetCamActive(CloneCam, true);
 //           };

            FusionFix::onGameProcessEvent() += []()
                {
                    auto viewport = rage::GetCurrentViewport();

                    rage::Vector3 charPosition;
					Ped pPlayerPed = 0;
					Natives::GetPlayerChar(Natives::ConvertIntToPlayerIndex(Natives::GetPlayerId()), &pPlayerPed);
					if (pPlayerPed)
					{
						Natives::GetCharCoordinates(pPlayerPed, &charPosition.x, &charPosition.y, &charPosition.z);
						//LogInfo("DebugCameraPosition: (%.2f, %.2f, %.2f)", DebugCameraPosition.x, DebugCameraPosition.y, DebugCameraPosition.z);
					}

                    if (viewport)
                    {
						viewport->mCameraMatrix[3][0] = charPosition.x;
                        viewport->mCameraMatrix[3][1] = charPosition.y;
                        viewport->mCameraMatrix[3][2] = charPosition.z;
                    }

                    if (viewport)
                    {
                        auto m = viewport->mCameraMatrix;
						LogInfo(
							"4:%.3f 4:%.3f 4:%.3f 4:%.3f "
							"4:%.3f 4:%.3f 4:%.3f 4:%.3f "
							"4:%.3f 4:%.3f 4:%.3f 4:%.3f "
							"4:%.3f 4:%.3f 4:%.3f 4:%.3f",
							m[0][0], m[0][1], m[0][2], m[0][3],
							m[1][0], m[1][1], m[1][2], m[1][3],
							m[2][0], m[2][1], m[2][2], m[2][3],
							m[3][0], m[3][1], m[3][2], m[3][3]
						);
					}

	//			//static rage::Vector3 g_DebugCamOffset = { 0.0f, 0.0f, 0.0f };
	//			//static constexpr float kMoveSpeed = 0.05f;
 //   //            static int camState = 0;
	//			//static int frames = 15;

 //   //            frames--;

 //   //            int CurrentCam;
	//			//Natives::GetRootCam(&CurrentCam);
	//			//


 //   //            if (GetAsyncKeyState(VK_NUMPAD9) & 0x8000 && frames < 0)
 //   //            {
	//			//	frames = 15;
 //   //                camState++;
 //   //                Natives::SetCameraState(CurrentCam, camState);
	//			//	LogInfo("Camera state: %d", camState);
 //   //            }
	//			//	

 //   //            if (GetAsyncKeyState(VK_NUMPAD8) & 0x8000 && frames < 0)
	//			//{
	//			//	frames = 15;
 //   //                camState--;
 //   //                Natives::SetCameraState(CurrentCam, camState);
 //   //                LogInfo("Camera state: %d", camState);
 //   //            }

	//			//if (GetAsyncKeyState(VK_LEFT) & 0x8000)
	//			//	g_DebugCamOffset.x -= kMoveSpeed;

	//			//if (GetAsyncKeyState(VK_RIGHT) & 0x8000)
	//			//	g_DebugCamOffset.x += kMoveSpeed;

	//			//if (GetAsyncKeyState(VK_UP) & 0x8000)
	//			//	g_DebugCamOffset.y += kMoveSpeed;

	//			//if (GetAsyncKeyState(VK_DOWN) & 0x8000)
	//			//	g_DebugCamOffset.y -= kMoveSpeed;

	//			//if (GetAsyncKeyState(VK_NUMPAD1) & 0x8000)
	//			//	g_DebugCamOffset.z += kMoveSpeed;

	//			//if (GetAsyncKeyState(VK_NUMPAD0) & 0x8000)
	//			//	g_DebugCamOffset.z -= kMoveSpeed;
	//			//

 //   //           
 //   //            //int CurrentDebugCam;
 //   //            //Natives::GetDebugCam(&CurrentDebugCam);
 //   //            

	//			//
 //   //           


 //   //            //Natives::SetFixedCamPos(DebugCameraPosition.x, DebugCameraPosition.y, DebugCameraPosition.z);
 //   //            rage::Vector3 cameraPosition;
 //   //            Natives::GetCamPos(CloneCam, &cameraPosition.x, &cameraPosition.y, &cameraPosition.z);
 //   //            LogInfo("CloneCameraPosition: (%.2f, %.2f, %.2f)", cameraPosition.x, cameraPosition.y, cameraPosition.z);


 //   //            //Natives::SetCamPos(
 //   //            //    CloneCam,
 //   //            //    DebugCameraPosition.x + g_DebugCamOffset.x,
 //   //            //    DebugCameraPosition.y + g_DebugCamOffset.y,
 //   //            //    DebugCameraPosition.z + g_DebugCamOffset.z
 //   //            //);

 //   //            //Natives::CamProcess(CurrentCam);
			};
			
            FusionFix::onShutdownEvent() += []()
            {
                VRMod_Shutdown();
            };
        }
    };

    VRBootstrap g_vrBootstrap;
}