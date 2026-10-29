// Falcon AI v3.0 — Official Unreal Engine 5 C++ Live-Link Implementation
#include "FalconAILiveLink.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "AssetToolsModule.h"
#include "AssetImportTask.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

void UFalconAILiveLinkSubsystem::GenerateAndImportAsset(
    const FString& Prompt,
    int32 TargetPolygons,
    bool bQuadDominant,
    bool bAutoRig,
    const FString& ServerUrl)
{
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = FHttpModule::Get().CreateRequest();
    Req->SetURL(ServerUrl + TEXT("/api/generate/text"));
    Req->SetVerb(TEXT("POST"));
    Req->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

    FString Payload = FString::Printf(
        TEXT("{\"prompt\":\"%s\",\"style\":\"realistic\",\"ultra_detail\":true}"),
        *Prompt.ReplaceCharWithEscapedChar());
    Req->SetContentAsString(Payload);

    Req->OnProcessRequestComplete().BindLambda(
        [ServerUrl, TargetPolygons, bQuadDominant, bAutoRig](FHttpRequestPtr, FHttpResponsePtr Resp, bool bOk)
        {
            if (!bOk || !Resp.IsValid()) return;
            TSharedPtr<FJsonObject> Json;
            TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Resp->GetContentAsString());
            if (!FJsonSerializer::Deserialize(Reader, Json) || !Json.IsValid()) return;

            FString AssetId = Json->GetStringField(TEXT("id"));
            if (AssetId.IsEmpty()) return;

            // Download rigged/quad-remeshed FBX directly into Saved/FalconAI and import to /Game/FalconAI
            FString FbxUrl = FString::Printf(TEXT("%s/api/export/%s/fbx?rigged=%d&quad=%d&target_faces=%d"),
                *ServerUrl, *AssetId, bAutoRig ? 1 : 0, bQuadDominant ? 1 : 0, TargetPolygons);

            TSharedRef<IHttpRequest, ESPMode::ThreadSafe> DlReq = FHttpModule::Get().CreateRequest();
            DlReq->SetURL(FbxUrl);
            DlReq->SetVerb(TEXT("GET"));
            DlReq->OnProcessRequestComplete().BindLambda([AssetId](FHttpRequestPtr, FHttpResponsePtr DlResp, bool bDlOk)
            {
                if (!bDlOk || !DlResp.IsValid()) return;
                FString TempFbx = FPaths::ProjectSavedDir() / TEXT("FalconAI") / (TEXT("Falcon_") + AssetId + TEXT(".fbx"));
                FFileHelper::SaveArrayToFile(DlResp->GetContent(), *TempFbx);

                UAssetImportTask* Task = NewObject<UAssetImportTask>();
                Task->Filename = TempFbx;
                Task->DestinationPath = TEXT("/Game/FalconAI");
                Task->bAutomated = true;
                Task->bSave = true;

                FAssetToolsModule& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
                AssetTools.Get().ImportAssetTasks({ Task });
            });
            DlReq->ProcessRequest();
        });
    Req->ProcessRequest();
}
