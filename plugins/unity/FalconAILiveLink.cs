// Falcon AI v3.0 — Official Unity Engine Editor Live-Link Plugin (Zero-Python C#)
// Drop into `Assets/Editor/FalconAILiveLink.cs` inside any Unity 2021.3 / 2022 LTS / Unity 6 project.
// Open via Unity Top Menu: `Window -> Falcon AI Studio v3.0`

#if UNITY_EDITOR
using System;
using System.IO;
using System.Text;
using UnityEditor;
using UnityEngine;
using UnityEngine.Networking;

namespace FalconAI.UnityPlugin
{
    public class FalconAILiveLink : EditorWindow
    {
        private string serverUrl = "http://127.0.0.1:8000";
        private string prompt = "Armored sci-fi cyber knight with glowing cyan visor";
        private string exportFormat = "fbx"; // "fbx" or "glb"
        private bool autoRig = true;
        private bool quadRemesh = true;
        private int targetFaces = 12000;
        private int texResolution = 2048;
        private string statusMessage = "Ready · Connected to local Falcon AI v3.0 C++20 Engine.";
        private bool isBusy = false;

        [MenuItem("Window/Falcon AI Studio v3.0")]
        public static void ShowWindow()
        {
            var wnd = GetWindow<FalconAILiveLink>("Falcon AI v3.0");
            wnd.minSize = new Vector2(360, 420);
        }

        private void OnGUI()
        {
            GUILayout.Space(8);
            EditorGUILayout.LabelField("FALCON AI v3.0 · UNITY LIVE-LINK", EditorStyles.boldLabel);
            EditorGUILayout.HelpBox("Zero-Credit Local C++20 3D Generator, Quad Remesher, 4K PBR Baker & Auto-Rigger.", MessageType.None);

            GUILayout.Space(6);
            serverUrl = EditorGUILayout.TextField("Falcon Server URL", serverUrl);
            GUILayout.Label("3D Asset Prompt", EditorStyles.miniBoldLabel);
            prompt = EditorGUILayout.TextArea(prompt, GUILayout.Height(58));

            GUILayout.Space(6);
            quadRemesh = EditorGUILayout.Toggle("Smart Quad Remesh", quadRemesh);
            if (quadRemesh)
            {
                targetFaces = EditorGUILayout.IntSlider("Target Polycount", targetFaces, 500, 100000);
            }
            texResolution = EditorGUILayout.IntPopup("PBR Map Resolution", texResolution,
                new[] { "256 (Fast)", "1024 (1K)", "2048 (2K HD)", "4096 (4K Ultra)" },
                new[] { 256, 1024, 2048, 4096 });
            autoRig = EditorGUILayout.Toggle("Auto-Rig + 24 Motion Clips", autoRig);
            exportFormat = EditorGUILayout.Popup("Import Format", exportFormat == "fbx" ? 0 : 1, new[] { "Autodesk .FBX (Skeletal + Quads)", "Khronos .GLB (glTF 2.0)" }) == 0 ? "fbx" : "glb";

            GUILayout.Space(12);
            GUI.enabled = !isBusy;
            if (GUILayout.Button(isBusy ? "Synthesizing in C++20 Engine..." : "Generate & Import into Assets/FalconAI", GUILayout.Height(36)))
            {
                GenerateAndImport();
            }
            GUI.enabled = true;

            GUILayout.Space(8);
            EditorGUILayout.HelpBox(statusMessage, MessageType.Info);
        }

        [Serializable]
        private class GenResponse { public string id; public string title; }

        private void GenerateAndImport()
        {
            isBusy = true;
            statusMessage = "1/3 Running Falcon 6-Agent C++20 Swarm...";
            Repaint();

            try
            {
                string jsonBody = "{\"prompt\":\"" + prompt.Replace("\"", "\\\"") + "\",\"style\":\"realistic\",\"ultra_detail\":true}";
                string genJson = PostJson(serverUrl + "/api/generate/text", jsonBody);
                var parsed = JsonUtility.FromJson<GenResponse>(genJson);
                if (parsed == null || string.IsNullOrEmpty(parsed.id))
                    throw new Exception("Invalid response from Falcon AI server.");

                if (quadRemesh)
                {
                    statusMessage = "2/3 Running Garland-Heckbert QEM + Quad Remesh...";
                    PostJson(serverUrl + "/api/mesh/" + parsed.id + "/smart-remesh",
                        "{\"target_faces\":" + targetFaces + ",\"quad_mode\":true,\"tex_resolution\":" + texResolution + "}");
                }

                if (autoRig)
                {
                    PostJson(serverUrl + "/api/mesh/" + parsed.id + "/rig",
                        "{\"rig_type\":\"auto\",\"clip_id\":\"walk_standard\",\"bake_skin\":true}");
                }

                statusMessage = "3/3 Downloading ." + exportFormat.ToUpperInvariant() + " into Assets/FalconAI/...";
                string dlUrl = serverUrl + "/api/export/" + parsed.id + "/" + exportFormat + (autoRig ? "?rigged=1" : "");
                byte[] assetBytes = GetBytes(dlUrl);

                string outDir = Path.Combine(Application.dataPath, "FalconAI");
                Directory.CreateDirectory(outDir);
                string safeName = string.Join("_", (parsed.title ?? parsed.id).Split(Path.GetInvalidFileNameChars()));
                string fullPath = Path.Combine(outDir, safeName + "_" + parsed.id + "." + exportFormat);
                File.WriteAllBytes(fullPath, assetBytes);

                AssetDatabase.Refresh();
                string relPath = "Assets/FalconAI/" + Path.GetFileName(fullPath);
                var imported = AssetDatabase.LoadMainAssetAtPath(relPath);
                if (imported != null) Selection.activeObject = imported;

                statusMessage = "Imported: " + relPath + " (" + (assetBytes.Length / 1024) + " KB)";
            }
            catch (Exception ex)
            {
                statusMessage = "Error: " + ex.Message;
            }
            finally
            {
                isBusy = false;
                Repaint();
            }
        }

        private static string PostJson(string url, string json)
        {
            using var req = new UnityWebRequest(url, "POST");
            req.uploadHandler = new UploadHandlerRaw(Encoding.UTF8.GetBytes(json));
            req.downloadHandler = new DownloadHandlerBuffer();
            req.SetRequestHeader("Content-Type", "application/json");
            var op = req.SendWebRequest();
            while (!op.isDone) { }
            if (req.result != UnityWebRequest.Result.Success)
                throw new Exception(req.error + " (" + req.downloadHandler.text + ")");
            return req.downloadHandler.text;
        }

        private static byte[] GetBytes(string url)
        {
            using var req = UnityWebRequest.Get(url);
            var op = req.SendWebRequest();
            while (!op.isDone) { }
            if (req.result != UnityWebRequest.Result.Success)
                throw new Exception(req.error);
            return req.downloadHandler.data;
        }
    }
}
#endif
