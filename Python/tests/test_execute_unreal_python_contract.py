import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
PLUGIN_ROOTS = (
    ROOT / "UnrealMCP",
    ROOT / "FlopperamUnrealMCP" / "Plugins" / "UnrealMCP",
)


class ExecuteUnrealPythonContractTests(unittest.TestCase):
    def test_local_server_exposes_execute_unreal_python_tool(self):
        server = (ROOT / "Python" / "unreal_mcp_server_advanced.py").read_text(encoding="utf-8")

        self.assertIn("def execute_unreal_python(", server)
        self.assertIn('"execute_unreal_python"', server)
        self.assertIn('"script_path"', server)
        self.assertIn('"args_json"', server)

    def test_bundled_plugins_execute_python_via_python_script_plugin(self):
        for plugin_root in PLUGIN_ROOTS:
            with self.subTest(plugin_root=plugin_root):
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

                self.assertIn('TEXT("execute_unreal_python")', bridge)
                self.assertIn("HandleExecuteUnrealPython", editor_commands)
                self.assertIn("IPythonScriptPlugin::Get()", editor_commands)
                self.assertIn("runpy.run_path", editor_commands)
                self.assertIn('"PythonScriptPlugin"', build)
                self.assertIn('"PythonScriptPlugin"', descriptor)


if __name__ == "__main__":
    unittest.main()
