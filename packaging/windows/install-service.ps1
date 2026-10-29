# OpenMesh Studio 2.0 — Native Windows Service Installer (Zero Docker)
$BinPath = Join-Path $PSScriptRoot '..\..\bin\openmesh_server.exe'
New-Service -Name 'OpenMeshStudio' -BinaryPathName "$BinPath --port 8000" `
  -DisplayName 'OpenMesh C++20 3D Studio' -StartupType Automatic
Start-Service -Name 'OpenMeshStudio'
