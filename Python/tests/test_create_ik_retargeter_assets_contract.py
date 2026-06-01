import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
PLUGIN_ROOTS = (
    ROOT / "UnrealMCP",
    ROOT / "FlopperamUnrealMCP" / "Plugins" / "UnrealMCP",
)


class CreateIKRetargeterAssetsContractTests(unittest.TestCase):
    def test_local_server_exposes_create_ik_retargeter_assets_tool(self):
        server = (ROOT / "Python" / "unreal_mcp_server_advanced.py").read_text(encoding="utf-8")

        self.assertIn("def create_ik_retargeter_assets(", server)
        self.assertIn('"create_ik_retargeter_assets"', server)
        self.assertIn('"source_ik_rig_path"', server)
        self.assertIn('"target_ik_rig_path"', server)
        self.assertIn('"retargeter_path"', server)
        self.assertIn('"target_chains"', server)

    def test_bundled_plugins_create_ik_retargeter_assets_with_ikrig_editor_api(self):
        for plugin_root in PLUGIN_ROOTS:
            with self.subTest(plugin_root=plugin_root):
                header = (
                    plugin_root
                    / "Source"
                    / "UnrealMCP"
                    / "Public"
                    / "Commands"
                    / "EpicUnrealMCPEditorCommands.h"
                ).read_text(encoding="utf-8")
                editor_commands = (
                    plugin_root
                    / "Source"
                    / "UnrealMCP"
                    / "Private"
                    / "Commands"
                    / "EpicUnrealMCPEditorCommands.cpp"
                ).read_text(encoding="utf-8")
                bridge = (
                    plugin_root / "Source" / "UnrealMCP" / "Private" / "EpicUnrealMCPBridge.cpp"
                ).read_text(encoding="utf-8")
                build = (
                    plugin_root / "Source" / "UnrealMCP" / "UnrealMCP.Build.cs"
                ).read_text(encoding="utf-8")
                descriptor = (plugin_root / "UnrealMCP.uplugin").read_text(encoding="utf-8")

                self.assertIn('TEXT("create_ik_retargeter_assets")', bridge)
                self.assertIn("HandleCreateIKRetargeterAssets", header)
                self.assertIn("HandleCreateIKRetargeterAssets", editor_commands)
                self.assertIn("UIKRigDefinitionFactory::CreateNewIKRigAsset", editor_commands)
                self.assertIn("UIKRigController::GetController", editor_commands)
                self.assertIn("SetRetargetRoot", editor_commands)
                self.assertIn("AddRetargetChain", editor_commands)
                self.assertIn("UIKRetargeterController::GetController", editor_commands)
                self.assertIn("SetIKRig(ERetargetSourceOrTarget::Source", editor_commands)
                self.assertIn("SetIKRig(ERetargetSourceOrTarget::Target", editor_commands)
                self.assertIn("SetPreviewMesh", editor_commands)
                self.assertIn("AddDefaultOps", editor_commands)
                self.assertIn("AutoMapChains", editor_commands)
                self.assertIn("SaveLoadedAsset", editor_commands)
                self.assertIn('"IKRig"', build)
                self.assertIn('"IKRigEditor"', build)
                self.assertIn('"Name": "IKRig"', descriptor)
                self.assertIn('"Enabled": true', descriptor)


if __name__ == "__main__":
    unittest.main()
