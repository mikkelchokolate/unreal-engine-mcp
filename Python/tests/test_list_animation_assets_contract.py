import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
PLUGIN_ROOTS = (
    ROOT / "UnrealMCP",
    ROOT / "FlopperamUnrealMCP" / "Plugins" / "UnrealMCP",
)


class ListAnimationAssetsContractTests(unittest.TestCase):
    def test_local_server_exposes_list_animation_assets_tool(self):
        server = (ROOT / "Python" / "unreal_mcp_server_advanced.py").read_text(encoding="utf-8")

        self.assertIn("def list_animation_assets(", server)
        self.assertIn('"list_animation_assets"', server)
        self.assertIn('"search_path"', server)
        self.assertIn('"class_names"', server)

    def test_bundled_plugins_route_list_animation_assets_command(self):
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

                self.assertIn('TEXT("list_animation_assets")', bridge)
                self.assertIn("HandleListAnimationAssets", header)
                self.assertIn("HandleListAnimationAssets", editor_commands)
                self.assertIn("UEditorAssetLibrary::ListAssets", editor_commands)
                self.assertIn('TEXT("search_path")', editor_commands)
                self.assertIn('TEXT("class_names")', editor_commands)


if __name__ == "__main__":
    unittest.main()
