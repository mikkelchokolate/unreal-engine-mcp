import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
PLUGIN_ROOTS = (
    ROOT / "UnrealMCP",
    ROOT / "FlopperamUnrealMCP" / "Plugins" / "UnrealMCP",
)


class ImportSkeletalMeshAssetContractTests(unittest.TestCase):
    def test_local_server_exposes_import_skeletal_mesh_asset_tool(self):
        server = (ROOT / "Python" / "unreal_mcp_server_advanced.py").read_text(encoding="utf-8")

        self.assertIn("def import_skeletal_mesh_asset(", server)
        self.assertIn('"import_skeletal_mesh_asset"', server)
        self.assertIn('"source_path"', server)
        self.assertIn('"destination_path"', server)
        self.assertIn('"destination_name"', server)

    def test_bundled_plugins_route_import_skeletal_mesh_asset_command(self):
        for plugin_root in PLUGIN_ROOTS:
            with self.subTest(plugin_root=plugin_root):
                build_cs = (plugin_root / "Source" / "UnrealMCP" / "UnrealMCP.Build.cs").read_text(
                    encoding="utf-8"
                )
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

                self.assertIn('"AssetTools"', build_cs)
                self.assertIn('TEXT("import_skeletal_mesh_asset")', bridge)
                self.assertIn("HandleImportSkeletalMeshAsset", header)
                self.assertIn("HandleImportSkeletalMeshAsset", editor_commands)
                self.assertIn("UAssetImportTask", editor_commands)
                self.assertIn("UFbxFactory", editor_commands)
                self.assertIn("FBXIT_SkeletalMesh", editor_commands)
                self.assertIn('TEXT("source_path")', editor_commands)
                self.assertIn('TEXT("destination_path")', editor_commands)
                self.assertNotIn("Task->bSave = bSave", editor_commands)
                self.assertIn("Task->bSave = false", editor_commands)
                self.assertIn("UEditorAssetLibrary::SaveLoadedAsset", editor_commands)
                self.assertIn('TEXT("saved_asset_paths")', editor_commands)


if __name__ == "__main__":
    unittest.main()
