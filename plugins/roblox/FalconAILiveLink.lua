-- Falcon AI v3.0 — Official Roblox Studio Live-Link Plugin (Pure Luau, Zero Python)
-- Save as `FalconAILiveLink.lua` inside `%LOCALAPPDATA%/Roblox/Plugins` and enable HttpService.

local HttpService = game:GetService("HttpService")
local Selection = game:GetService("Selection")

local toolbar = plugin:CreateToolbar("Falcon AI v3.0")
local toggleBtn = toolbar:CreateButton("Falcon 3D", "Open Falcon AI v3.0 Local 3D Studio", "rbxassetid://4458901886")

local info = DockWidgetPluginGuiInfo.new(Enum.InitialDockState.Right, false, false, 320, 380, 280, 300)
local widget = plugin:CreateDockWidgetPluginGui("FalconAIStudioV3", info)
widget.Title = "Falcon AI v3.0 · Live-Link"

local frame = Instance.new("Frame")
frame.Size = UDim2.fromScale(1, 1)
frame.BackgroundColor3 = Color3.fromRGB(13, 14, 18)
frame.Parent = widget

local promptBox = Instance.new("TextBox")
promptBox.Size = UDim2.new(1, -20, 0, 80)
promptBox.Position = UDim2.fromOffset(10, 14)
promptBox.TextWrapped = true
promptBox.MultiLine = true
promptBox.Text = "Sci-fi hover speeder bike with twin glowing thrusters"
promptBox.BackgroundColor3 = Color3.fromRGB(24, 26, 32)
promptBox.TextColor3 = Color3.fromRGB(240, 242, 246)
promptBox.Parent = frame

local genBtn = Instance.new("TextButton")
genBtn.Size = UDim2.new(1, -20, 0, 38)
genBtn.Position = UDim2.fromOffset(10, 106)
genBtn.Text = "Generate 3D Asset via Falcon C++20"
genBtn.BackgroundColor3 = Color3.fromRGB(235, 238, 245)
genBtn.TextColor3 = Color3.fromRGB(12, 13, 17)
genBtn.Parent = frame

local statusLbl = Instance.new("TextLabel")
statusLbl.Size = UDim2.new(1, -20, 0, 120)
statusLbl.Position = UDim2.fromOffset(10, 154)
statusLbl.TextWrapped = true
statusLbl.BackgroundTransparency = 1
statusLbl.TextColor3 = Color3.fromRGB(165, 173, 186)
statusLbl.Text = "Connected to http://127.0.0.1:8000"
statusLbl.Parent = frame

toggleBtn.Click:Connect(function()
	widget.Enabled = not widget.Enabled
end)

genBtn.MouseButton1Click:Connect(function()
	statusLbl.Text = "Synthesizing 3D assembly in Falcon AI..."
	local ok, res = pcall(function()
		return HttpService:RequestAsync({
			Url = "http://127.0.0.1:8000/api/generate/text",
			Method = "POST",
			Headers = { ["Content-Type"] = "application/json" },
			Body = HttpService:JSONEncode({
				prompt = promptBox.Text,
				style = "realistic",
				ultra_detail = true
			})
		})
	end)
	if not ok or not res.Success then
		statusLbl.Text = "Enable Studio HttpService & ensure Falcon AI (:8000) is running."
		return
	end
	local data = HttpService:JSONDecode(res.Body)
	local modelFolder = Instance.new("Model")
	modelFolder.Name = "FalconAI_" .. tostring(data.title or data.id)
	if type(data.parts) == "table" then
		for idx, p in ipairs(data.parts) do
			local part = Instance.new("Part")
			part.Name = tostring(p.name or ("Part_" .. idx))
			part.Anchored = true
			part.Size = Vector3.new(4, 2, 4)
			part.Position = Vector3.new(0, 2 + idx * 1.4, 0)
			if type(p.color) == "string" and #p.color == 7 then
				part.Color = Color3.fromHex(p.color)
			end
			part.Material = (tonumber(p.metallic) or 0) > 0.6 and Enum.Material.Metal or Enum.Material.SmoothPlastic
			part.Parent = modelFolder
		end
	end
	modelFolder.Parent = workspace
	Selection:Set({ modelFolder })
	statusLbl.Text = string.format("Spawned '%s' · Full FBX/OBJ ready at /api/export/%s/fbx", tostring(data.title), tostring(data.id))
end)
