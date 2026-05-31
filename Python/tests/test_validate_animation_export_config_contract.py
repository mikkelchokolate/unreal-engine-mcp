import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
PLUGIN_ROOTS = (
    ROOT / "UnrealMCP",
    ROOT / "FlopperamUnrealMCP" / "Plugins" / "UnrealMCP",
)


class ValidateAnimationExportConfigContractTests(unittest.TestCase):
    def test_local_server_exposes_validate_animation_export_config_tool(self):
        server = (ROOT / "Python" / "unreal_mcp_server_advanced.py").read_text(encoding="utf-8")

        self.assertIn("def validate_animation_export_config(", server)
        self.assertIn('"validate_animation_export_config"', server)
        self.assertIn('"config_path"', server)

    def test_bundled_plugins_route_validate_animation_export_config_command(self):
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

                self.assertIn('TEXT("validate_animation_export_config")', bridge)
                self.assertIn("HandleValidateAnimationExportConfig", header)
                self.assertIn("HandleValidateAnimationExportConfig", editor_commands)
                self.assertIn('TEXT("config_path")', editor_commands)
                self.assertIn("source_animation_paths", editor_commands)
                self.assertIn("UEditorAssetLibrary::LoadAsset", editor_commands)
                self.assertIn("ue_only_blocked", editor_commands)


if __name__ == "__main__":
    unittest.main()
