import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
PLUGIN_ROOTS = (
    ROOT / "UnrealMCP",
    ROOT / "FlopperamUnrealMCP" / "Plugins" / "UnrealMCP",
)


class ExportRetargetedAnimationsContractTests(unittest.TestCase):
    def test_local_server_exposes_first_class_export_tool(self):
        server = (ROOT / "Python" / "unreal_mcp_server_advanced.py").read_text(encoding="utf-8")

        self.assertIn("def export_retargeted_animations(", server)
        self.assertIn('"export_retargeted_animations"', server)
        self.assertIn('"config_path"', server)
        self.assertIn('"script_path"', server)
        self.assertIn("execute_unreal_python(", server)
        self.assertIn("json.dumps", server)

    def test_bundled_plugins_route_first_class_export_command(self):
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

                self.assertIn('TEXT("export_retargeted_animations")', bridge)
                self.assertIn("HandleExportRetargetedAnimations", header)
                self.assertIn("HandleExportRetargetedAnimations", editor_commands)
                self.assertIn('TEXT("config_path")', editor_commands)
                self.assertIn('TEXT("script_path")', editor_commands)
                self.assertIn("HandleExecuteUnrealPython", editor_commands)


if __name__ == "__main__":
    unittest.main()
